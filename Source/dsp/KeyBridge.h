// KeyBridge.h
// Bridge letting a "companion" key-detection plug-in (OpenVoxKey) publish the
// detected key/scale so the main OpenVoxTuner instance can read it.
//
// CRITICAL: OpenVoxKey and OpenVoxTuner are *separate* VST3 binaries. A plain
// in-process singleton does NOT work between them — each binary gets its own
// static instance, so the companion's publish() would never reach the main
// plug-in's read(). To share state across the two modules we back the bridge
// with a named, OS-level memory-mapped file (pagefile-backed, session-local).
// Every module that maps the same name sees the same physical pages, so a
// publish() from OpenVoxKey.vst3 is observable by OpenVoxTuner.vst3 (and by any
// other OpenVoxTuner instance in the same session, including across processes).
//
// Instances are paired by a user-supplied group string (A..D by default), so
// several independent (companion -> main) pairs can coexist.
//
// The companion writes via publish(); the main plug-in reads via read().
// On non-Windows platforms the in-process singleton is kept as a fallback
// (the companion feature is primarily Windows / Studio One; cross-binary
// sharing for other platforms would use the same memory-mapped approach).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cstdlib>
#include <cstring>

#if JUCE_WINDOWS
  #include <windows.h>
#endif

namespace ovtdsp
{
    static constexpr int kKeyBridgeMaxSlots = 16;
    static constexpr int kKeyBridgeNameLen  = 16;
    static const wchar_t* kKeyBridgeSharedName = L"Local\\OpenVoxTunerKeyBridge";

    // POD layout placed inside the single OS-shared memory region. The
    // atomically-updated fields use juce::Atomic so a write from one module is
    // observed by another (lock-free int/double on x64). The region itself is
    // never *constructed* in C++ (we reinterpret the mapped bytes); the atomics
    // simply wrap the existing memory locations.
    struct KeyBridgeSlot
    {
        char                group[kKeyBridgeNameLen]; // zero-terminated group, "" if unused
        juce::Atomic<int>   key;        // 0..11 (C..B)
        juce::Atomic<int>   scale;      // ovtdsp::Scale index
        juce::Atomic<double> timestamp; // last publish time (seconds)
    };

    struct KeyBridgeRegion
    {
        juce::Atomic<int> initFlag;                 // 0 = uninitialised, 1 = initialised
        KeyBridgeSlot     slots[kKeyBridgeMaxSlots];
    };

    class KeyBridge
    {
    public:
        static KeyBridge& getInstance()
        {
            static KeyBridge inst;
            return inst;
        }

        /** Companion: publish the detected key/scale into the named group. */
        void publish (const juce::String& group, int key, int scale) noexcept
        {
            KeyBridgeSlot* slot = getOrCreateSlot (group);
            if (slot == nullptr)
                return;
            slot->key.set (key);
            slot->scale.set (scale);
            slot->timestamp.set (static_cast<double> (juce::Time::getCurrentTime().toMilliseconds()) / 1000.0);
        }

        /** Main plug-in: read the latest published values. Returns false if the
         *  group was never published (so callers can keep their current key). */
        bool read (const juce::String& group, int& outKey, int& outScale, double& outTimestamp) const noexcept
        {
            const KeyBridgeSlot* slot = findSlot (group);
            if (slot == nullptr)
                return false;
            outKey       = slot->key.get();
            outScale     = slot->scale.get();
            outTimestamp = slot->timestamp.get();
            return true;
        }

    private:
        KeyBridge()  { attachSharedMemory(); }
        ~KeyBridge() { detachSharedMemory(); }

        // Deterministically map a group string to a fixed slot so the same group
        // always lands in the same shared slot. A..D => 0..3 (reserved); any
        // other group is hashed into the upper slots (4..Max-1) to avoid clobbering
        // the A..D slots.
        static int slotIndexForGroup (const juce::String& group) noexcept
        {
            if (group.isNotEmpty())
            {
                const juce::juce_wchar c = group[0];
                if (c >= 'A' && c <= 'D') return static_cast<int> (c - 'A');
                if (c >= 'a' && c <= 'd') return static_cast<int> (c - 'a');
            }
            const int h = std::abs (group.hashCode()) % (kKeyBridgeMaxSlots - 4);
            return 4 + h;
        }

        KeyBridgeSlot* getOrCreateSlot (const juce::String& group) noexcept
        {
            if (region_ == nullptr)
                return nullptr;
            const int idx = slotIndexForGroup (group);
            KeyBridgeSlot* slot = &region_->slots[idx];
            // Always record the group string so reads can verify the slot belongs
            // to this group. We overwrite unconditionally (including stale data
            // left by a previous session in the shared memory) — skipping the
            // write when non-empty would let a stale/different group in the slot
            // silently mask a fresh publish.
            const juce::String g = group.substring (0, kKeyBridgeNameLen - 1);
            const char* utf = g.toUTF8().getAddress();
            std::memcpy (slot->group, utf, static_cast<size_t> (g.length()));
            slot->group[g.length()] = '\0';
            return slot;
        }

        const KeyBridgeSlot* findSlot (const juce::String& group) const noexcept
        {
            if (region_ == nullptr)
                return nullptr;
            const int idx = slotIndexForGroup (group);
            const KeyBridgeSlot* slot = &region_->slots[idx];
            // The slot must carry exactly this group; an empty slot (never
            // published) or a different group (hash collision) is not a match.
            if (juce::String (slot->group) != group.substring (0, kKeyBridgeNameLen - 1))
                return nullptr;
            return slot;
        }

        void attachSharedMemory()
        {
        #if JUCE_WINDOWS
            const SIZE_T size = sizeof (KeyBridgeRegion);
            mapHandle_ = CreateFileMappingW (INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                             0, static_cast<DWORD> (size), kKeyBridgeSharedName);
            if (mapHandle_ != nullptr)
            {
                region_ = static_cast<KeyBridgeRegion*> (MapViewOfFile (mapHandle_, FILE_MAP_ALL_ACCESS, 0, 0, size));
                if (region_ != nullptr)
                {
                    // First opener initialises the region once. Fresh file mappings
                    // are already zero filled by the OS, but we force a clean init
                    // so a stale region from a crashed previous session is cleared.
                    if (region_->initFlag.compareAndSetBool (1, 0))
                    {
                        std::memset (region_, 0, sizeof (KeyBridgeRegion));
                        region_->initFlag.set (1);
                    }
                    return;
                }
                CloseHandle (mapHandle_);
                mapHandle_ = nullptr;
            }
            // Shared memory unavailable (e.g. sandboxed test runner or restricted
            // environment): fall back to an in-process region. Cross-binary
            // sharing won't work in that case, but the bridge still behaves
            // correctly inside a single module.
            std::memset (&fallbackRegion_, 0, sizeof (KeyBridgeRegion));
            region_ = &fallbackRegion_;
        #else
            // Non-Windows: in-process fallback (companion feature is primarily
            // Windows / Studio One).
            std::memset (&fallbackRegion_, 0, sizeof (KeyBridgeRegion));
            region_ = &fallbackRegion_;
        #endif
        }

        void detachSharedMemory()
        {
        #if JUCE_WINDOWS
            if (region_ != &fallbackRegion_ && region_ != nullptr)
                UnmapViewOfFile (region_);
            region_ = nullptr;
            if (mapHandle_ != nullptr)
                CloseHandle (mapHandle_);
            mapHandle_ = nullptr;
        #else
            region_ = nullptr;
        #endif
        }

        KeyBridgeRegion* region_ = nullptr;
        KeyBridgeRegion   fallbackRegion_; // used when shared memory is unavailable
    #if JUCE_WINDOWS
        HANDLE mapHandle_ = nullptr;
    #endif
    };
}
