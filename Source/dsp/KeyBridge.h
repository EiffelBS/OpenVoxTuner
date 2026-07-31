// KeyBridge.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cstdint>
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

    // POD layout placed inside the OS-shared memory region. The mapped bytes
    // contain no C++ objects; Windows interlocked operations below provide the
    // required cross-process synchronization.
    struct KeyBridgeSlot
    {
        char                group[kKeyBridgeNameLen]; // zero-terminated group, "" if unused
        std::int32_t        key;        // 0..11 (C..B)
        std::int32_t        scale;      // ovtdsp::Scale index
        std::int64_t        timestamp; // milliseconds since epoch
    };

    struct KeyBridgeRegion
    {
        std::int32_t initFlag;                       // 0 = uninitialised, 1 = initialised
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
            writeInt (slot->key, key);
            writeInt (slot->scale, scale);
            writeInt64 (slot->timestamp, juce::Time::getCurrentTime().toMilliseconds());
        }

        /** Main plug-in: read the latest published values. Returns false if the
         *  group was never published (so callers can keep their current key). */
        bool read (const juce::String& group, int& outKey, int& outScale, double& outTimestamp) const noexcept
        {
            const KeyBridgeSlot* slot = findSlot (group);
            if (slot == nullptr)
                return false;
            outKey       = readInt (slot->key);
            outScale     = readInt (slot->scale);
            outTimestamp = static_cast<double> (readInt64 (slot->timestamp)) / 1000.0;
            return true;
        }

    private:
        KeyBridge()  { attachSharedMemory(); }
        ~KeyBridge() { detachSharedMemory(); }

        static void writeInt (std::int32_t& target, std::int32_t value) noexcept
        {
        #if JUCE_WINDOWS
            InterlockedExchange (reinterpret_cast<volatile LONG*> (&target), static_cast<LONG> (value));
        #else
            target = value;
        #endif
        }

        static std::int32_t readInt (const std::int32_t& target) noexcept
        {
        #if JUCE_WINDOWS
            return static_cast<std::int32_t> (InterlockedCompareExchange (
                reinterpret_cast<volatile LONG*> (const_cast<std::int32_t*> (&target)), 0, 0));
        #else
            return target;
        #endif
        }

        static void writeInt64 (std::int64_t& target, std::int64_t value) noexcept
        {
        #if JUCE_WINDOWS
            InterlockedExchange64 (reinterpret_cast<volatile LONGLONG*> (&target), static_cast<LONGLONG> (value));
        #else
            target = value;
        #endif
        }

        static std::int64_t readInt64 (const std::int64_t& target) noexcept
        {
        #if JUCE_WINDOWS
            return static_cast<std::int64_t> (InterlockedCompareExchange64 (
                reinterpret_cast<volatile LONGLONG*> (const_cast<std::int64_t*> (&target)), 0, 0));
        #else
            return target;
        #endif
        }

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
            // left by a previous session in the shared memory) â€” skipping the
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
                const bool mappingAlreadyExists = (GetLastError() == ERROR_ALREADY_EXISTS);
                region_ = static_cast<KeyBridgeRegion*> (MapViewOfFile (mapHandle_, FILE_MAP_ALL_ACCESS, 0, 0, size));
                if (region_ != nullptr)
                {
                    // Only the creator may clear a fresh mapping. Existing
                    // clients must never memset the shared region while it is
                    // being used by another process.
                    if (! mappingAlreadyExists)
                    {
                        std::memset (region_, 0, sizeof (KeyBridgeRegion));
                        writeInt (region_->initFlag, 1);
                    }
                    return;
                }
                CloseHandle (mapHandle_);
                mapHandle_ = nullptr;
            }
            // Shared memory unavailable: fall back to an in-process POD region.
            fallbackRegion_ = KeyBridgeRegion{};
            region_ = &fallbackRegion_;
        #else
            // Non-Windows: in-process fallback (companion feature is primarily
            // Windows / Studio One).
            fallbackRegion_ = KeyBridgeRegion{};
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



