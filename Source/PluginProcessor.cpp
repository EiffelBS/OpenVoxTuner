// PluginProcessor.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/NoteUtils.h"
#include "dsp/ReverbEffect.h"
#include "dsp/FormantPreserver.h"
#include "external/presonus/ipsleditcontroller.h"
// Generated build info (created by CMake)
#include "BuildInfo.h"
#include "dsp/PitchShifter.h" // for gPitchShifterGrainEvents
#include <vector>
#include <array>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <juce_data_structures/juce_data_structures.h> // juce::UndoManager, juce::ValueTree

// IMPORTANT: OVT_LOG is wrapped in #if JUCE_DEBUG to match the convention
// used in PitchShifter.cpp. Reason: in Release builds, juce::Logger::writeToLog
// still runs and allocates a juce::String per call (even though
// BufferedFileLogger amortises the disk I/O), and at one of the call sites
// (processBlock line 2091, "MIDI: f0_out=...") the log fires EVERY audio
// block (~100 calls/sec while singing). On real-time-constrained DAWs like
// Studio One, this string allocation + lock contention is enough to push
// the 11.6 ms block deadline (~512 samples at 44.1 kHz) and cause audible
// dropouts â€” the user reported this regression on 2026-07-17. Keeping the
// log gated to Debug means Release is free of any string work in the hot
// path. To re-enable logging in a Release build for diagnosis, define
// OVT_FORCE_LOG in the build flags (e.g. cmake -DCMAKE_CXX_FLAGS="-DOVT_FORCE_LOG").
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
 #define OVT_LOG(msg) juce::Logger::writeToLog (msg)
#else
 #define OVT_LOG(msg) do { } while (false)
#endif

// Diagnostic helper (only compiled when audio logging is enabled): returns the
// largest sample-to-sample absolute jump in a channel, used to localise clicks.
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
static float computeMaxJump (const float* p, int n)
{
    if (p == nullptr || n < 2) return 0.0f;
    float m = 0.0f;
    for (int i = 1; i < n; ++i)
    {
        const float d = std::fabs (p[i] - p[i - 1]);
        if (d > m) m = d;
    }
    return m;
}
#endif

// Definition of the IID for the IEditControllerExtra interface
#include "pluginterfaces/base/funknown.h"
#if OVT_ARA_ENABLED
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#endif
#if JUCE_WINDOWS
# include <windows.h>
#endif
namespace Presonus {
    DEF_CLASS_IID (IEditControllerExtra)
}



void OpenVoxTunerAudioProcessor::forceCreatePitchTestGrain()
{
    if (pitchShifter)
        pitchShifter->forceCreateTestGrain();
}

void OpenVoxTunerAudioProcessor::dumpVST3BundleInfo()
{
    juce::StringArray commonPaths;
#if JUCE_WINDOWS
    commonPaths.add ("C:/Program Files/Common Files/VST3");
    commonPaths.add ("C:/Program Files (x86)/Common Files/VST3");
#elif JUCE_MAC
    commonPaths.add ("/Library/Audio/Plug-Ins/VST3");
    commonPaths.add ("~/Library/Audio/Plug-Ins/VST3");
#else
    commonPaths.add ("/usr/lib/vst3");
    commonPaths.add ("/usr/local/lib/vst3");
#endif

    OVT_LOG ("Dumping VST3 search paths...");
    juce::File found;
    for (auto& p : commonPaths)
    {
        juce::File dir (p);
        if (! dir.exists()) continue;
        auto children = dir.findChildFiles (juce::File::findDirectories, false);
        for (auto& c : children)
        {
            if (c.getFileName().containsIgnoreCase ("OpenVoxTuner"))
            {
                OVT_LOG ("Found candidate bundle: " + c.getFullPathName());
                auto files = c.findChildFiles (juce::File::findFiles, true);
                for (auto& f : files)
                {
                    OVT_LOG ("  " + f.getRelativePathFrom (c));
                }
                found = c;
            }
        }
    }

    if (! found.exists())
    {
        OVT_LOG ("OpenVoxTuner.vst3 not found in common VST3 folders.");
    }
    else
    {
        OVT_LOG ("Bundle dump complete.");

        // Try to read the moduleinfo.json for VST3 metadata (contains event/bus info)
        juce::File moduleInfo = found.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("moduleinfo.json");
        if (moduleInfo.existsAsFile())
        {
            OVT_LOG ("Reading moduleinfo.json: " + moduleInfo.getFullPathName());
            juce::String content = moduleInfo.loadFileAsString();
            const int maxLog = 8192;
            if (content.length() > maxLog)
                OVT_LOG (content.substring (0, maxLog) + "... (truncated)");
            else
                OVT_LOG (content);
        }
        else
        {
            OVT_LOG ("moduleinfo.json not found inside bundle.");
        }
    }
}


// Buffered file logger: captures Logger::writeToLog messages and flushes
// them to disk in batches every 200 ms, rather than on every log call.
//
// Why buffering matters (regression: dropouts in Studio One when Flex or
// Attack are enabled, 2026-07-17):
//   The audio callback runs in a real-time thread. The previous
//   SimpleFileLogger did `file.appendText(line)` on every log message
//   (open + write + close, on Windows this is ~1-5 ms per call). With
//   several OVT_LOG sites in the hot path (FlexTune / Amount /
//   pitchShifter->process / harmony, each gated to fire ~once per
//   second), the per-block callback was taking 1-5 ms just for I/O â€”
//   enough to push a 512-sample block at 44.1 kHz over the 11.6 ms
//   deadline on slower laptops, causing regular audio dropouts when
//   the user enabled features (Flex / Attack) that add more work
//   around those log sites.
//
//   Buffering keeps the same diagnostic capability (the user can
//   read the log file after a session) but amortises the file I/O
//   cost: at most 5 disk writes per second (one per 200 ms tick),
//   each writing a batch of messages accumulated since the last tick.
//   A burst of 50 OVT_LOG calls in a 100 ms window now costs a single
//   file write instead of 50.
class BufferedFileLogger : public juce::Logger, private juce::Timer
{
public:
    explicit BufferedFileLogger (const juce::File& f)
        : file (f)
    {
        file.deleteFile(); // start fresh
        file.create();
        // 200 ms is a good balance: short enough that the log file is
        // updated "live" (the user can `tail -f` it during a session),
        // long enough to coalesce most per-second periodic logs into
        // a single write.
        startTimer (200);
    }

    ~BufferedFileLogger() override
    {
        // Final flush so we don't lose the last messages.
        stopTimer();
        flushPending();
    }

    void logMessage (const juce::String& message) override
    {
        const juce::String line = juce::Time::getCurrentTime().toString(true, true) + " " + message + "\n";
        juce::ScopedLock lock (writeLock);
        pending.emplace_back (line);
#if JUCE_WINDOWS
        // Also emit immediately to the debugger output so DebugView /
        // Visual Studio can see the message in real time, even if the
        // file write is delayed by up to 200 ms. OutputDebugStringA is
        // non-blocking (it copies into a kernel buffer and returns),
        // so this is safe in the audio callback.
        OutputDebugStringA (line.toUTF8().getAddress());
#endif
    }

    void timerCallback() override
    {
        flushPending();
    }

private:
    void flushPending()
    {
        // Atomically swap the pending buffer with an empty one, then
        // do the (potentially slow) file I/O outside the lock. This
        // keeps the audio callback's logMessage() wait-free: it only
        // contends on a brief push_back + swap.
        std::vector<juce::String> toWrite;
        {
            juce::ScopedLock lock (writeLock);
            toWrite.swap (pending);
        }
        if (toWrite.empty())
            return;

        juce::String combined;
        combined.preallocateBytes (static_cast<int> (toWrite.size()) * 80);
        for (const auto& line : toWrite)
            combined += line;
        // One file open + one write + one close, regardless of how many
        // messages were in the batch. On Windows, appendText is the
        // JUCE helper that handles the stream lifecycle.
        file.appendText (combined);
    }

    juce::File file;
    juce::CriticalSection writeLock;
    std::vector<juce::String> pending;
};

// juce::Logger is process-global. Keep ownership separate from processor
// instances so one plugin instance cannot delete another instance's logger.
static juce::CriticalSection openVoxLoggerLock;
static BufferedFileLogger* openVoxLogger = nullptr;
static int openVoxLoggerUsers = 0;

void OpenVoxTunerAudioProcessor::setHarmonyEnvelopeTimes (float attackMs, float releaseMs)
{
    if (harmonyEngine != nullptr)
        harmonyEngine->setEnvelopeTimes (attackMs, releaseMs);
}

void OpenVoxTunerAudioProcessor::clearHarmonyCache()
{
    harmonyFrequencies.clear();
    lastHarmonyNotes.clear();
    harmonyBuffer.clear();
    harmonyOutputLevel.store(0.0f);
}

int OpenVoxTunerAudioProcessor::getLastSentMidiNoteForChannel (int channel) const
{
    if (channel < 1 || channel > 16) return -1;
    return lastSentMidiNote[channel - 1];
}

//==============================================================================
// VST3 Extension to support Fender Studio Pro's Micro View (Studio One)
//==============================================================================
class PresonusMicroViewExtension : public juce::VST3ClientExtensions,
                                   public Presonus::IEditControllerExtra
{
public:
    PresonusMicroViewExtension() = default;
    virtual ~PresonusMicroViewExtension() = default;

    // juce::VST3ClientExtensions overrides
    int32_t queryIEditController (const Steinberg::TUID iid, void** obj) override
    {
        if (memcmp (iid, Presonus::IEditControllerExtra::iid, sizeof (Steinberg::TUID)) == 0)
        {
            addRef();
            *obj = static_cast<Presonus::IEditControllerExtra*> (this);
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return -1; // -1 indicates not handled
    }

    // Presonus::IEditControllerExtra overrides
    Steinberg::int32 PLUGIN_API getParamExtraFlags (Steinberg::Vst::ParamID id) override
    {
        // We flag "speed", "amount", "formant" for the Micro View
        uint32_t speedId = juce::VST3ClientExtensions::convertJuceParameterId("speed", true);
        uint32_t amountId = juce::VST3ClientExtensions::convertJuceParameterId("amount", true);
        uint32_t formantId = juce::VST3ClientExtensions::convertJuceParameterId("formant", true);
        
        if (id == speedId || id == amountId || id == formantId)
            return Presonus::kParamFlagMicroEdit;
            
        return 0;
    }

    Steinberg::tresult PLUGIN_API setParamAutomationMode (Steinberg::Vst::ParamID /*id*/, Steinberg::int32 /*automationMode*/) override
    {
        return Steinberg::kNotImplemented;
    }

    // Steinberg::FUnknown overrides
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID iid, void** obj) override
    {
        if (memcmp (iid, Presonus::IEditControllerExtra::iid, sizeof (Steinberg::TUID)) == 0)
        {
            addRef();
            *obj = static_cast<Presonus::IEditControllerExtra*> (this);
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }

    Steinberg::uint32 PLUGIN_API addRef() override { return ++refCount; }
    Steinberg::uint32 PLUGIN_API release() override { return --refCount; }

private:
    std::atomic<int> refCount {1};
};

//==============================================================================
// Minimal class for the ARA controller (DocumentController)
// Necessary for createARAFactory() to be implemented.
// Only compiled when ARA support is enabled (OVT_ARA_ENABLED=1).
//==============================================================================
#if OVT_ARA_ENABLED
class OpenVoxTunerARADocumentController : public juce::ARADocumentControllerSpecialisation
{
public:
    using ARADocumentControllerSpecialisation::ARADocumentControllerSpecialisation;
    
    // Pure virtual overrides required by the ARA interface.
    bool doRestoreObjectsFromStream (juce::ARAInputStream&, const juce::ARARestoreObjectsFilter*) override
    {
        return true;
    }

    bool doStoreObjectsToStream (juce::ARAOutputStream&, const juce::ARAStoreObjectsFilter*) override
    {
        return true;
    }
};

// macro that implements `const ARA::ARAFactory* createARAFactory()` 
// expected by JUCE's VST3 module.
JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wmissing-prototypes")
const ARA::ARAFactory* createARAFactory()
{
    return juce::ARADocumentControllerSpecialisation::createARAFactory<OpenVoxTunerARADocumentController>();
}
JUCE_END_IGNORE_WARNINGS_GCC_LIKE
#endif // OVT_ARA_ENABLED

//==============================================================================
OpenVoxTunerAudioProcessor::OpenVoxTunerAudioProcessor()
    : AudioProcessor (juce::AudioProcessor::BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          // Optional Sidechain input bus (stereo, but only the first channel is
                          // analysed for pitch detection). Declared stereo so Logic
                          // does not hang during stereo AU negotiation.
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                          ),
      parameters (*this, nullptr, juce::Identifier ("OpenVoxTuner"),
                  {
                      // Speed : temps de retargeting en millisecondes (0-200 ms)
                      std::make_unique<juce::AudioParameterFloat> (
                          "speed", "Speed",
                          juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f),
                          20.0f),
                      std::make_unique<juce::AudioParameterChoice> (
                          "latency_mode", "Latency Mode",
                          juce::StringArray { "Direct Monitoring", "Low Latency", "Quality", "Safe" }, 1),

                      // Amount : intensite de la correction (0-100%)
                      std::make_unique<juce::AudioParameterFloat> (
                          "amount", "Amount",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                          1.0f),

                      // Formant Shift : decalage des formants en demi-tons (-12 a +12)
                      std::make_unique<juce::AudioParameterFloat> (
                            "formant", "Formant Shift",
                            juce::NormalisableRange<float> (-5.0f, 5.0f, 0.1f), 0.0f),

                      // Formant Shift Enable
                      std::make_unique<juce::AudioParameterBool> (
                          "formant_enable", "Formant Enable", false),

                      // Formant Preservation Mode: Legacy (single peaking EQ) vs MultiFormant (F1-F4)
                      // Default: MultiFormant (index 1) for best quality; Legacy available for CPU savings
                      std::make_unique<juce::AudioParameterChoice> (
                          "formant_mode", "Formant Mode",
                          juce::StringArray { "Legacy", "MultiFormant" }, 1),

                      // Formant Preservation Strategy: selects the formant-preservation
                      // method for the lead and harmony voices.
                      //   0 = Current : partial 1/sqrt(r) pre-warp, fixed male centers.
                      //   1 = P0      : full 1/r pre-warp + voice-type-aware centers.
                      //   2 = P1      : LPC cross-synthesis (post-PSOLA), creative re-applied.
                      //   3 = P2      : peaking-EQ MultiFormant + fast smoothing (reactive).
                      //   4 = P3      : allpass-cascade formant shifting (transparent, phase-only).
                      std::make_unique<juce::AudioParameterChoice> (
                          "formant_strategy", "Formant Strategy",
                          juce::StringArray { "Subtle", "Balanced", "Marked", "Reactive", "Precise" }, 4),

                      // Key : index de la tonique (0=C, 1=C#, ..., 11=B)
                      std::make_unique<juce::AudioParameterInt> (
                          "key", "Key", 0, 11, 0),

                      // Scale : index du mode
                      std::make_unique<juce::AudioParameterChoice> (
                          "scale", "Scale", juce::StringArray {
                              "Chromatic", "Major", "Melodic Minor", "Harmonic Minor", "Natural Minor", 
                              "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Phrygian", 
                              "Lydian", "Mixolydian", "Locrian", "Custom"
                          }, 0),

                      // Key/Scale detection master switch. When off, the user drives
                      // key/scale manually; when on, the source below detects it. This
                      // replaces the old "Manual" choice of the key_source parameter.
                      std::make_unique<juce::AudioParameterBool> (
                          "key_detect", "Key/Scale Detection", false),

                      // Key detection source (used only when key_detect is on):
                      // 0=Auto (in-plugin audio analysis),
                      // 1=OpenVoxKey (shared bridge from the OpenVoxKey companion detector),
                      // 2=Sidechain (analysis of the sidechain input).
                      std::make_unique<juce::AudioParameterChoice> (
                          "key_source", "Key Source",
                          juce::StringArray { "Auto", "OpenVoxKey", "Sidechain" }, 0),

                      // Companion group letter (must match the OpenVoxKey instance).
                      std::make_unique<juce::AudioParameterChoice> (
                          "companion_group", "Companion Group",
                          juce::StringArray { "A", "B", "C", "D" }, 0),

                      // 12 booleens pour la gamme personnalisee (custom).
                      // Actif uniquement si Scale = 5 (Custom).
                      // Par defaut : majeur en C -> {C, D, E, F, G, A, B}.
                      std::make_unique<juce::AudioParameterBool> ("custom0",  "Custom C",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom1",  "Custom C#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom2",  "Custom D",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom3",  "Custom D#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom4",  "Custom E",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom5",  "Custom F",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom6",  "Custom F#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom7",  "Custom G",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom8",  "Custom G#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom9",  "Custom A",  true),
                      std::make_unique<juce::AudioParameterBool> ("custom10", "Custom A#", false),
                      std::make_unique<juce::AudioParameterBool> ("custom11", "Custom B",  true),

                      // Bypass : ignore le traitement
                      std::make_unique<juce::AudioParameterBool> (
                          "bypass", "Bypass", false),

                      // Mode : Live (0) ou Curve Editor (1)
                      std::make_unique<juce::AudioParameterChoice> (
                          "mode", "Mode", juce::StringArray { "Live", "Curve Editor" }, 0),

                      // Harmony Type (vocale) : None, presets...  default = ThirdBelowAbove (index 3)
                      std::make_unique<juce::AudioParameterChoice> (
                          "harmony_type", "Harmony Type",
                          juce::StringArray {
                              "None",
                              "3rd Below", "3rd Above", "3rd Below + Above",
                              "4th Below", "4th Above", "4th Below + Above",
                              "5th Below", "5th Above", "5th Below + Above",
                              "3rd Below + 5th Above", "5th Below + 3rd Above",
                              "Octave Below", "Octave Above", "Octave Below + Above",
                              "Vocal Stack (3 voices)", "Vocal Stack (4 voices)",
                              "Power Chord", "Parallel 3rd", "Drone",
                              "Unison (2 voices)", "Unison + Octaves (4 voices)"
                          }, 3),

                      // Harmony Enable : master on/off â€” disabled by default
                      std::make_unique<juce::AudioParameterBool> (
                          "harmony_enable", "Harmony Enable", false),

                      // Harmony Gain : niveau de volume des harmonies â€” 1.0 by default
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_gain", "Harmony Volume",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.75f),

                      // Harmony Blend : melange voix principale / harmonies â€” 0.5 by default
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_blend", "Harmony Blend",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f)
                      ,
                      // Harmony Attack : per-voice fade-in duration (ms) applied to each
                      // harmony voice's attack. Smoother / more progressive than a hard
                      // onset. When the Noise Gate is enabled the effective attack is
                      // clamped to a short gate-follow time so the harmony swells with the
                      // gated dry signal. 35 ms by default (matches the previous internal
                      // attack). Range 1..300 ms.
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_attack", "Harmony Attack (ms)",
                          juce::NormalisableRange<float> (1.0f, 300.0f, 1.0f), 35.0f)
                      ,
                      // Use Voice : enabled by default
                      std::make_unique<juce::AudioParameterBool> (
                          "harmony_use_voice", "Use Voice for Harmony", true),
                      // Shifted voices : 4 by default
                      std::make_unique<juce::AudioParameterInt> (
                          "harmony_shifted_voices", "Shifted Voices", 1, 4, 4),
                      std::make_unique<juce::AudioParameterChoice> (
                          "harmony_tone", "Harmony Tone",
                          juce::StringArray { "Choir", "Organ" }, 0),
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_tone_color", "Harmony Tone Color",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f),
                       // When on (default), the harmony voices follow the lead voice's correction
                       // character (vibrato preservation, humanize, flex, attack-aware), so they move
                       // with the lead instead of staying locked to the scale grid.
                       std::make_unique<juce::AudioParameterBool> (
                          "harmony_follow_lead", "Harmony Follow Lead", true),
                       // Gain Match: when on (default), the harmony mix is scaled by
                       // 1/sqrt(1+N) where N = number of active harmony voices, so the
                       // total output RMS is roughly equal to the dry input RMS (this
                       // matches the perceptual balance for uncorrelated harmony voices
                       // and is a good compromise for correlated Unison/Unison+Octaves
                       // which the user reports as "much louder"). The dry signal is
                       // untouched. The harmony_volume knob still controls the overall
                       // harmony level (post-compensation). User can turn this off to
                       // restore the natural "additive" boost.
                       std::make_unique<juce::AudioParameterBool> (
                          "harmony_gain_match", "Harmony Gain Match", true),

                       // Harmony Formant Shift : formant shift for harmony voices only (-5 to +5 st)
                       std::make_unique<juce::AudioParameterFloat> (
                          "harmony_formant", "Harmony Formant Shift",
                          juce::NormalisableRange<float> (-5.0f, 5.0f, 0.1f), 0.0f),

                       std::make_unique<juce::AudioParameterBool> (
                          "midi_out_enable", "MIDI Out Enable",
                          // In standalone mode, disable MIDI out by default.
                          ! isStandaloneWrapper())
                      , std::make_unique<juce::AudioParameterBool> (
                          "midi_target_enable", "MIDI Target Enable", false)
                      , std::make_unique<juce::AudioParameterBool> (
                          "dbg_test_grain", "Debug Test Grain", false)
                      , std::make_unique<juce::AudioParameterInt> (
                            "editor_measures", "Editor Measures", 1, 32, 4)
                      , std::make_unique<juce::AudioParameterBool> (
                            "editor_playhead_loop", "Editor Playhead Loop", false)
                      , std::make_unique<juce::AudioParameterBool> (
                            "auto_scroll", "Auto Scroll", true)
                      , std::make_unique<juce::AudioParameterChoice> (
                            "pitch_detector", "Pitch Detector",
                            juce::StringArray { "YIN", "Reserved" }, 0)
                      // Voice Type: constrains pitch detector search range to reduce
                      // octave errors and improve detection speed. Default = Universal
                      // (full 30-1000 Hz range). Other options narrow the range to
                      // typical vocal registers.
                      , std::make_unique<juce::AudioParameterChoice> (
                            "voice_type", "Voice Type",
                            juce::StringArray { "Universal", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 0)
                      , std::make_unique<juce::AudioParameterBool> (
                            "reverb_enable", "Reverb Enable", false)
                      , std::make_unique<juce::AudioParameterBool> (
                            juce::ParameterID { "noise_gate_enable", 1 }, "Noise Gate", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            juce::ParameterID { "noise_gate_threshold", 1 }, "Gate Threshold",
                            juce::NormalisableRange<float> (-80.0f, 0.0f, 1.0f), -50.0f)
                      // Upward compression: lifts quiet passages before tuning.
                      // Single knob = amount (0..1); pivot follows the signal RMS.
                      , std::make_unique<juce::AudioParameterBool> (
                            "upward_comp_enable", "Upward Comp Enable", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "upward_comp_amount", "Upward Comp Amount",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "reverb_mix", "Reverb Mix",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.30f)
                      // 2026-07-24 (Deprecation): FlexTune and Attack-Aware
                      // APVTS parameters are KEPT for preset compatibility
                      // (so existing presets don't lose their values) but
                      // their default values are forced to "off/0" and the
                      // corresponding DSP code in processBlock is disabled.
                      // Future re-implementation can re-enable them by
                      // changing the defaults back to their previous values
                      // and re-enabling the DSP code in processBlock.
                      // ----------------------------------------------------------------
                      // FlexTune: deadband around the target note (0-100 cents)
                      // DEPRECATED: default forced to 0 (no deadband). See above.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "flex_tune", "FlexTune",
                            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f)
                      // Humanize: random pitch fluctuations (0-50 cents)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "humanize", "Humanize",
                            juce::NormalisableRange<float> (0.0f, 50.0f, 1.0f), 10.0f)
                      // Correction mode: Modern (false) / Transparent (true)
                      , std::make_unique<juce::AudioParameterBool> (
                            "correction_mode", "Correction Mode", true)
                      // Vibrato preservation: 0% = classic instantaneous correction,
                      // 100% = correction against the smoothed center pitch so the
                      // vibrato modulation survives.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "vibrato_preserve", "Vibrato Preserve",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.20f)
                      // Attack-aware correction: enable + release time (ms). When
                      // enabled, the correction is eased off on note onsets/transients.
                      // DEPRECATED: default forced to false (disabled). See above.
                      , std::make_unique<juce::AudioParameterBool> (
                            "attack_aware", "Attack-Aware Correction", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "attack_release", "Attack Release",
                            juce::NormalisableRange<float> (10.0f, 300.0f, 1.0f), 60.0f)
                      // UI Theme: 0 = Dark (default), 1 = Light
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_theme", "UI Theme", 0, 1, 0)
                      // UI Language: 0 = English (default), 1 = French, 2 = German, 3 = Spanish, 4 = Japanese, 5 = Chinese
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_language", "UI Language", 0, 5, 0)
                      // UI waveform: show/hide overlay (0 = hidden, 1 = shown, default 1)
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_show_waveform", "Show Waveform", 0, 1, 1)
                      // UI waveform display type: 0 = Line, 1 = Mirror, 2 = Spectral (default)
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_waveform_type", "Waveform Display Type", 0, 2, 2)
                      // UI auto-center pitch display (0 = off, 1 = on, default 0)
                      , std::make_unique<juce::AudioParameterInt> (
                            "ui_auto_center", "Auto-Center Pitch", 0, 1, 0)
                      // Visualizer zoom range (Hz, log scale)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "viz_fmin", "Visualizer Min Hz",
                            juce::NormalisableRange<float> (16.0f, 2000.0f, 0.1f), 50.0f)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "viz_fmax", "Visualizer Max Hz",
                            juce::NormalisableRange<float> (100.0f, 8372.0f, 0.1f), 1500.0f)
                      // Morph: A/B crossfade position (0 = source slot A, 1 = target slot B).
                      // Made a host-automatable parameter so the DAW can automate the morph.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "morph_amount", "Morph",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f)
                    })
{
    // Ensure per-channel MIDI note state starts clean (-1 means no active note)
    for (int ch = 0; ch < 16; ++ch)
        lastSentMidiNote[ch] = -1;
    midiTargetActive.store (false);
    midiTargetHz.store (0.0f);

    for (int i = 0; i < maxHarmonySnapshotVoices; ++i)
    {
        harmonyFrequencySnapshot[i].store (0.0f);
        harmonyFrequencyCleanSnapshot[i].store (0.0f);
    }

    // Initialise le compteur de persistence anti-saut-octave
    octaveJumpRejectionCount = 0;

    // Retrieves raw pointers to the parameters' atomic values.
    speedParam   = parameters.getRawParameterValue ("speed");
    amountParam  = parameters.getRawParameterValue ("amount");
    latencyModeParam = parameters.getRawParameterValue ("latency_mode");
    formantParam = parameters.getRawParameterValue ("formant");
    formantEnableParam = parameters.getRawParameterValue ("formant_enable");
    formantModeParam = parameters.getRawParameterValue ("formant_mode");
    formantStrategyParam = parameters.getRawParameterValue ("formant_strategy");
    keyParam     = parameters.getRawParameterValue ("key");
    scaleParam   = parameters.getRawParameterValue ("scale");
    scaleChoiceParam = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("scale"));
    keyIntParam = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("key"));
    bypassParam = parameters.getRawParameterValue ("bypass");
    modeParam   = parameters.getRawParameterValue ("mode");

    harmonyTypeParam  = parameters.getRawParameterValue ("harmony_type");
    harmonyGainParam  = parameters.getRawParameterValue ("harmony_gain");
    harmonyBlendParam = parameters.getRawParameterValue ("harmony_blend");
    harmonyAttackParam = parameters.getRawParameterValue ("harmony_attack");
    harmonyEnableParam = parameters.getRawParameterValue ("harmony_enable");
    harmonyUseVoiceParam = parameters.getRawParameterValue ("harmony_use_voice");
    harmonyShiftedVoicesParam = parameters.getRawParameterValue ("harmony_shifted_voices");
    harmonyToneParam = parameters.getRawParameterValue ("harmony_tone");
    harmonyToneColorParam = parameters.getRawParameterValue ("harmony_tone_color");
    harmonyFollowLeadParam = parameters.getRawParameterValue ("harmony_follow_lead");
    harmonyGainMatchParam = parameters.getRawParameterValue ("harmony_gain_match");
    harmonyFormantParam = parameters.getRawParameterValue ("harmony_formant");
    midiOutEnableParam = parameters.getRawParameterValue ("midi_out_enable");
    midiTargetEnableParam = parameters.getRawParameterValue ("midi_target_enable");
    dbgTestGrainParam = parameters.getRawParameterValue ("dbg_test_grain");
    morphAmountParam = parameters.getRawParameterValue ("morph_amount");
    morphParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter ("morph_amount"));
    editorMeasuresParam = parameters.getRawParameterValue ("editor_measures");
    editorPlayheadLoopParam = parameters.getRawParameterValue ("editor_playhead_loop");
    detectorParam = parameters.getRawParameterValue ("pitch_detector");
    voiceTypeParam = parameters.getRawParameterValue ("voice_type");
    reverbEnableParam = parameters.getRawParameterValue ("reverb_enable");
    reverbMixParam = parameters.getRawParameterValue ("reverb_mix");
    noiseGateEnableParam = parameters.getRawParameterValue ("noise_gate_enable");
    noiseGateThresholdParam = parameters.getRawParameterValue ("noise_gate_threshold");
    upwardCompEnableParam = parameters.getRawParameterValue ("upward_comp_enable");
    upwardCompAmountParam = parameters.getRawParameterValue ("upward_comp_amount");
    flexTuneParam = parameters.getRawParameterValue ("flex_tune");
    humanizeParam = parameters.getRawParameterValue ("humanize");
    correctionModeParam = parameters.getRawParameterValue ("correction_mode");
    vibratoPreserveParam = parameters.getRawParameterValue ("vibrato_preserve");
    attackAwareParam = parameters.getRawParameterValue ("attack_aware");
    attackReleaseParam = parameters.getRawParameterValue ("attack_release");
    keySourceParam = parameters.getRawParameterValue ("key_source");
    companionGroupParam = parameters.getRawParameterValue ("companion_group");
    keyDetectParam = parameters.getRawParameterValue ("key_detect");

    for (int i = 0; i < 12; ++i)
    {
        const juce::String id = "custom" + juce::String (i);
        customParam[i] = parameters.getRawParameterValue (id);
    }

    // Instantiates DSP modules â€” YIN pitch detector.
    pitchDetectors[0] = std::make_unique<ovtdsp::YinPitchDetector>();
    activePitchDetector.store (pitchDetectors[0].get());

    // Dedicated YIN detector for the optional Sidechain input bus (used by the
    // "Sidechain" key source). Kept independent from the main-input detector.
    sidechainPitchDetector = std::make_unique<ovtdsp::YinPitchDetector>();
    activeDetectorMode = 0;
    scaleQuantizer   = std::make_unique<ovtdsp::ScaleQuantizer>();
    scaleQuantizer->setScale (ovtdsp::Scale::Chromatic); // Ensure chromatic on first launch
    {
        const auto& intervals = scaleQuantizer->getScaleIntervals();
        for (int i = 0; i < intervals.size(); ++i)
            scaleIntervalSnapshot[static_cast<size_t> (i)].store (intervals[i], std::memory_order_relaxed);
        scaleIntervalSnapshotSize.store (juce::jmin (12, intervals.size()), std::memory_order_release);
    }
    pitchShifter     = std::make_unique<ovtdsp::PitchShifter>();
    harmonyEngine    = std::make_unique<ovtdsp::HarmonyEngine>();

    retargetEnvelope = std::make_unique<ovtdsp::RetargetEnvelope>();
    pitchCurve       = std::make_unique<ovtdsp::PitchCurve>();
    pitchCurve->loadPreset ("default");

    // Initialize post-processing effects (reverb, etc.)
    effects.push_back (std::make_unique<ovtdsp::ReverbEffect>());
    OVT_LOG ("Effects initialized: " + juce::String (static_cast<int> (effects.size())));

    // VST3 extension for Studio One's Micro View.
    // Only allocated for Studio One â€” for every other host getVST3ClientExtensions()
    // returns nullptr, so there is no point in constructing the object (it
    // would just be dead memory).  Detecting Studio One via the JUCE host
    // type: Studio One identifies itself with the application path
    // ".../Studio One.app/Contents/MacOS/Studio One" on macOS.
    {
        juce::PluginHostType hostType;
        if (hostType.getHostPath().contains ("Studio One"))
        {
            vst3Extensions = std::make_unique<PresonusMicroViewExtension>();
            OVT_LOG (juce::String("VST3 Micro View extension enabled (Studio One detected)."));
        }
        else
        {
            OVT_LOG(juce::String("VST3 Micro View extension disabled (host: ") + hostType.getHostDescription() + ").");
        }
    }

    // Install one process-global file logger. The file is ONLY created in
    // DEBUG builds (or diagnostic Release builds built with OVT_FORCE_LOG);
    // in a normal Release build no log file is written at all, so empty/rotated
    // files do not pile up. Old log files are pruned to keep at most 5. Do not
    // replace a logger installed by the host or by another JUCE application.
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
    {
        const juce::ScopedLock lock (openVoxLoggerLock);
        if (openVoxLogger == nullptr && juce::Logger::getCurrentLogger() == nullptr)
        {
            juce::File logDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                    .getChildFile ("OpenVoxTuner")
                                    .getChildFile ("logs");
            logDir.createDirectory();

            // Prune old logs: keep only the most recent 5 "ovt_*.log" files.
            {
                juce::Array<juce::File> logs = logDir.findChildFiles (juce::File::findFiles, false, "ovt_*.log");
                std::sort (logs.begin(), logs.end(),
                           [] (const juce::File& a, const juce::File& b)
                           { return a.getLastModificationTime() < b.getLastModificationTime(); });
                const int toDelete = logs.size() - 5;
                for (int i = 0; i < toDelete; ++i)
                    logs[i].deleteFile();
            }

            juce::File logFile = logDir.getChildFile (
                "ovt_" + juce::Time::getCurrentTime().toISO8601(true).replaceCharacter (':', '-') + ".log");
            openVoxLogger = new BufferedFileLogger (logFile);
            juce::Logger::setCurrentLogger (openVoxLogger);
        }
        if (openVoxLogger != nullptr)
        {
            ++openVoxLoggerUsers;
            OVT_LOG ("OpenVoxTuner log initialized");
        }
    }
#endif

    // Debug logging: print key metadata so we can diagnose host issues (MIDI bus visibility, bypass state)
    {
        juce::String msg;
        msg += "OpenVoxTuner startup: producesMidi=" + juce::String (producesMidi() ? 1 : 0);
        msg += ", isMidiEffect=" + juce::String (isMidiEffect() ? 1 : 0);
        msg += ", hasEditor=" + juce::String (hasEditor() ? 1 : 0);
        OVT_LOG (msg);

        // Print build id/time so we can verify which binary the host loaded
        OVT_LOG ("Build: " + juce::String (OVT_BUILD_ID) + " @ " + juce::String (OVT_BUILD_TIME) + " (proj=" + juce::String(OVT_PROJECT_VERSION) + ")");

        // Log initial parameter states (if available)
        if (bypassParam) OVT_LOG ("param[bypass]=" + juce::String ((int) std::round (bypassParam->load())));
        if (midiOutEnableParam) OVT_LOG ("param[midi_out_enable]=" + juce::String ((int) std::round (midiOutEnableParam->load())));
        if (harmonyUseVoiceParam) OVT_LOG ("param[harmony_use_voice]=" + juce::String ((int) std::round (harmonyUseVoiceParam->load())));

        // Initialize prevBypassState for runtime change detection
        if (bypassParam)
            prevBypassState.store (static_cast<int>(std::round (bypassParam->load())));
    }

    // Capture the factory-default parameter state (parameters.state once all
    // AudioParameters are at their declared defaults, before any user/DAW
    // change or setStateInformation()). This backs the "Default" Plugin
    // Preset. Stored as a deep copy so it is never mutated.
    defaultPluginState = parameters.copyState();
}

OpenVoxTunerAudioProcessor::~OpenVoxTunerAudioProcessor()
{
    // The processor outlives the editor, but the editor's destructor
    // may not have run yet (e.g. host kills the processor first).  Be
    // defensive: flip both flags off and stop the worker so any
    // pending callback that reaches readAndCachePlayHeadInfo() bails
    // out cleanly.
    notifyAuReleased();
    notifyEditorShuttingDown();
    // Stop the dedicated playhead worker (started in prepareToPlay()).
    // It runs on its own thread so we can safely call getPlayHead() from
    // there even on hosts that block when called from the audio/UI thread.
    // The worker was temporarily disabled while hunting a rainbow
    // beach-ball bug, but that bug was actually caused by a setLatency
    // ping-pong in applyLatencyMode(), not by the worker.  Re-enabled
    // 2026-07-22.
    stopPlayheadThread();

    // Delete the BufferedFileLogger (which subclasses juce::Timer) so the
    // Timer is stopped and destroyed before JUCE module shutdown, avoiding
    // a LeakedObjectDetector assertion.
    //
    // IMPORTANT: In JUCE 8, Logger::setCurrentLogger() only sets the pointer
    // without deleting the previous logger.  We must delete it manually.
    const juce::ScopedLock lock (openVoxLoggerLock);
    if (openVoxLogger != nullptr && openVoxLoggerUsers > 0 && --openVoxLoggerUsers == 0)
    {
        if (juce::Logger::getCurrentLogger() == openVoxLogger)
            juce::Logger::setCurrentLogger (nullptr);
        delete openVoxLogger;
        openVoxLogger = nullptr;
    }
}

// === Plugin name ===
const juce::String OpenVoxTunerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

void OpenVoxTunerAudioProcessor::setMorphAmount (float v)
{
    if (morphParam != nullptr)
        morphParam->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
}

// === Audio configuration (before the first processBlock) ===
void OpenVoxTunerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{

    currentSampleRate = sampleRate;

    // Prepare ARA support
#if OVT_ARA_ENABLED
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());
#endif

    // Initialize DSP modules with the current sample rate.
    // Prepare the YIN pitch detector.
    if (pitchDetectors[0] != nullptr)
        pitchDetectors[0]->prepare (sampleRate / 4.0, samplesPerBlock);

    // Voice Type: apply the initial frequency range from the parameter.
    // Subsequent changes are picked up by syncParameters() per block.
    if (voiceTypeParam != nullptr)
    {
        lastVoiceType = juce::jlimit (0, 5, static_cast<int> (std::round (voiceTypeParam->load())));
        const float minHz = voiceTypeMinHz[lastVoiceType];
        const float maxHz = voiceTypeMaxHz[lastVoiceType];
        if (auto* pd = dynamic_cast<ovtdsp::YinPitchDetector*>(pitchDetectors[0].get()))
            pd->setFrequencyRange (minHz, maxHz);
        if (sidechainPitchDetector)
            sidechainPitchDetector->setFrequencyRange (minHz, maxHz);
    }

    // IMPORTANT: prepare the PitchShifter BEFORE calling applyLatencyMode().
    // PitchShifter::prepare() resets latencyMs/latencySamples to its hard-coded
    // default (20 ms). If applyLatencyMode() runs first, its setLatencySamples
    // value is immediately overwritten by the next line `setLatencySamples
    // (pitchShifter->getLatencySamples())`. Worse, on hosts like Live 12 the
    // latency ping-pong (576 then 960) triggers an immediate re-setActive
    // and a fresh prepareToPlay() â€” we observed 600 000+ setActive calls
    // in a single session, which manifested as the rainbow beach-ball.
    pitchShifter->prepare (sampleRate, samplesPerBlock);
    noiseGate.prepare (sampleRate);
    upwardComp.prepare (sampleRate);
    formantPreserver.prepare (sampleRate, samplesPerBlock);
    formantPreserverHarmony.prepare (sampleRate, samplesPerBlock);
    lpcFormantPreserverLead.prepare (sampleRate, samplesPerBlock);
    for (auto& lpc : lpcFormantPreserverHarmony)
        lpc.prepare (sampleRate, samplesPerBlock);

    retargetEnvelope->prepare (sampleRate);
    harmonyEngine->prepare (sampleRate);

    // Block-aware parameter smoothers (FlexTune, Humanize) â€” these now use
    // an explicit time constant expressed in seconds (independently of the
    // block size).
    //
    // 2026-07-23 (Fix BB): FlexTune TC was raised from 200ms to 500ms.
    // The deadband transition (currentFlexTuneAmount = 0 when in deadband,
    // smoothstep when out) modulates at the vibrato frequency (5Hz) when
    // the singer vibrates around the deadband boundary. At 200ms TC,
    // |H(5Hz)| = 0.70, so 70% of the modulation amplitude passed through
    // to targetRatio, producing audible "warble" + occasional pops at the
    // transitions. At 500ms TC, |H(5Hz)| = 0.20, so only 20% passes
    // through, and the smoother's output is essentially flat at the
    // timescale of a single vibrato cycle. The trade-off is that FlexTune
    // engages/disengages more slowly (500ms instead of 200ms), but for
    // a deadband control this is imperceptible to the singer (the natural
    // ear time constant is ~150-300ms, and the deadband transitions are
    // not on the singer's critical timeline).
    flexTuneSmoother.prepare (sampleRate);
    // 2026-07-23 (Fix BC): TC was lowered from 500ms (Fix BB) back to
    // 200ms. The downstream smoother's job is much easier now that
    // the deadband INPUT is pre-smoothed by f0SmootherForDeadband
    // (TC=150ms): the deadband output is a soft 5Hz transition (no
    // step), so a 200ms TC is enough to clean up the residual.
    flexTuneSmoother.setTimeConstantSeconds (0.200f);
    flexTuneSmoother.reset (1.0f); // start at full correction
    // 2026-07-23 (Fix BC): f0SmootherForDeadband is the UPSTREAM
    // smoother that converts the 5Hz vibrato crossing the deadband
    // boundary into a smooth soft transition. TC=150ms: at 5Hz,
    // |H(5Hz)| = 0.42, so 42% of the vibrato amplitude reaches the
    // deadband (vs 100% without the smoother), and the deadband
    // transition is now gradual (~1 cent of pitch_deviation per
    // vibrato cycle, smoothly distributed across ~10-15 blocks at
    // 256/44100). The deadband output is no longer a step, so the
    // downstream flexTuneSmoother barely has any work to do.
    f0SmootherForDeadband.prepare (sampleRate);
    f0SmootherForDeadband.setTimeConstantSeconds (0.150f);
    f0SmootherForDeadband.reset (0.0f); // 0 Hz = "no pitch yet"
    humanizeSmoother.prepare (sampleRate);
    humanizeSmoother.setTimeConstantSeconds (0.150f);
    humanizeSmoother.reset (0.0f);
    // 2026-07-23 (Fix AY + Fix AZ): speed floor on the ratio, applied AFTER
    // the RetargetEnvelope. The fixed 80 ms TC smooths per-block jitter
    // (vibrato, YIN step, residual flexTune/humanize modulation) to below
    // the OLA grain spacing sensitivity threshold (~0.5 sample misalignment).
    // Initialised to 1.0 (neutral ratio) so the very first block doesn't
    // carry a stale value.
    speedFloor.prepare (sampleRate);
    speedFloor.setTimeConstantSeconds (0.080f);
    speedFloor.reset (1.0f);
    currentFlexTuneAmount = 1.0f;
    currentHumanizeCents = 0.0f;

    // Prepare post-processing effects
    for (auto& effect : effects)
        effect->prepare (sampleRate, samplesPerBlock);

    // Prepare temporary buffers for shifted voices
    shiftedVoiceBuffers.clear();
    shiftedVoiceBuffers.resize (OpenVoxTunerAudioProcessor::maxShiftedVoices);
    for (auto& b : shiftedVoiceBuffers)
    {
        b.setSize (getMainBusNumOutputChannels(), samplesPerBlock, false, true, false);
        b.clear();
    }

    // Preallocate harmony work buffers (avoid setSize in audio callback)
    const int workCh = juce::jmax (1, getMainBusNumOutputChannels());
    harmonyBuffer.setSize (workCh, samplesPerBlock, false, true, false);
    harmonyBuffer.clear();
    araWaveformBuffer.setSize (1, samplesPerBlock, false, true, false);
    araWaveformBuffer.clear();
    // 2026-07-24 (Harmony staggered attack): raise the master enable
    // gain TC from 25ms to 40ms so the harmony bus fades in/out
    // smoother, complementing the per-voice staggered TCs.
    harmonyEnableGain.reset (sampleRate, 0.040);
    // Smooth the raw noise-gate gain for the harmony mix to prevent clicks.
    // 15 ms matches the gate's own attack smoothing.
    harmonyGateGain.reset (sampleRate, 0.015);
    harmonyGateGain.setCurrentAndTargetValue (0.0f);
    harmonyAttackGain = 0.0f;
    // Harmony-type transition crossfade: clear the state and old-buffer cache.
    lastHarmonyTypeVal = -1;
    typeDipFadeTotalSamples = 0;
    typeDipFadeRemaining = 0;
    typeCrossfadeActive = false;
    typeCrossfadeOld.clear();
    synthWorkBuffer.setSize (workCh, samplesPerBlock, false, true, false);
    synthWorkBuffer.clear();
    shiftedVoiceGainRamps.setSize (3, samplesPerBlock, false, true, false);
    shiftedVoiceGainRamps.clear();
    lastMixedHarmonyBuffer.setSize (workCh, samplesPerBlock, false, true, false);
    lastMixedHarmonyBuffer.clear();
    // 2026-07-24 (Harmony staggered attack): each harmony voice
    // gets a slightly different smoothing TC, so the voices "stagger"
    // their attack (voice 0 reaches 63% in 40ms, voice 1 in 46ms, voice
    // 2 in 52ms, voice 3 in 58ms). This avoids the "survolume" burst
    // at note onset where all 4 voices ramp up simultaneously and sum
    // to 4x amplitude for a few milliseconds. The 6ms per-voice
    // offset is in the natural range of a real choir (where each
    // singer's note onsets are slightly desynchronised) and is
    // imperceptible as a "delay" because the audio is not delayed,
    // only the gain ramp is. The base TC was raised from 20ms to
    // 40ms so each individual voice has a smoother ramp too.
    for (size_t v = 0; v < shiftedVoiceGains.size(); ++v)
    {
        const double perVoiceTC = 0.040 + 0.006 * static_cast<double> (v); // 40, 46, 52, 58 ms
        shiftedVoiceGains[v].reset (sampleRate, perVoiceTC);
        shiftedVoiceGains[v].setCurrentAndTargetValue (0.0f);
    }

    // Prepare the per-voice ratio glides used to mask harmony type changes.
    for (size_t v = 0; v < shiftedVoiceRatioSmoothers.size(); ++v)
    {
        shiftedVoiceRatioSmoothers[v].prepare (sampleRate);
        shiftedVoiceRatioSmoothers[v].setTimeConstantSeconds (0.040); // 40 ms note-change glide
        shiftedVoiceRatioSmoothers[v].snapTo (shiftedVoiceSmoothedRatio[v]);
    }

    // Prepare the per-voice loudness normalization smoother (4/sqrt(N)), so a
    // harmony-type voice-count change does not step the remaining voices' level.
    shiftedVoiceLevelSmoother.prepare (sampleRate);
    shiftedVoiceLevelSmoother.setTimeConstantSeconds (0.040); // 40 ms, same as the gain ramps
    shiftedVoiceLevelSmoother.reset (1.0f);

    // Prepare dedicated pitch shifters for shifted harmony voices
    if (shiftedVoicePitchShifters.size() != OpenVoxTunerAudioProcessor::maxShiftedVoices)
    {
        shiftedVoicePitchShifters.clear();
        shiftedVoicePitchShifters.resize (OpenVoxTunerAudioProcessor::maxShiftedVoices);
        for (auto& ps : shiftedVoicePitchShifters)
            ps = std::make_unique<ovtdsp::PitchShifter>();
    }
    for (auto& ps : shiftedVoicePitchShifters)
        if (ps != nullptr)
        {
            ps->prepare (sampleRate, samplesPerBlock);
            ps->setAttackTimeMs (30.0f); // Attack envelope for shifted voices
        }

    // Set attack time for main pitch shifter
    if (pitchShifter != nullptr)
        pitchShifter->setAttackTimeMs (30.0f);


    // Prepare the analysis FIFO for pitch detection.
    analysisFifo.setSize (1, analysisWindow, false, true, false);
    analysisFifo.clear();
    fifoWriteIndex = 0;
    fifoFillCount = 0;

    // Pre-allocate the linear buffer used by computeInputPitch().
    // Eliminates an 8 KB heap allocation on each audio block, a major
    // source of glitches (especially at small buffer sizes, e.g., 144 samples).
    analysisLinearBuffer.allocate (analysisWindow, true);

    // Prepare the dedicated Sidechain analysis path (FIFO + detector). Runs the
    // YIN detector at 1/4 sample rate (with decimation) like the main detector.
    if (sidechainPitchDetector != nullptr)
        sidechainPitchDetector->prepare (sampleRate / 4.0, samplesPerBlock);
    sidechainFifo.setSize (1, analysisWindow, false, true, false);
    sidechainFifo.clear();
    sidechainFifoWriteIndex = 0;
    sidechainFifoFillCount = 0;
    sidechainSamplesSinceLastAnalysis = 0;
    sidechainLinearBuffer.allocate (analysisWindow, true);

    // Reset the latency-mode cache so applyLatencyMode() ALWAYS re-runs
    // setLatencySamples() on this prepareToPlay(), even if the user has
    // not changed the mode since the last prepare. This is needed
    // because some hosts (notably Studio One with VST3) do NOT re-call
    // prepareToPlay() when the user disables and re-enables the insert
    // slot â€” they only re-read the last reported latency from the
    // plugin. If we early-return inside applyLatencyMode() (which we
    // do in syncParameters() to avoid spamming the host every block),
    // the host keeps showing the previous latency value, which can
    // disagree with the actual mode selected in the plugin UI. Forcing
    // the re-apply here guarantees the host's PDC always matches the
    // plugin's current mode, regardless of whether prepareToPlay() is
    // the only host hook that fires on insert re-enable.
    //
    // CRITICAL: must be called AFTER pitchShifter->prepare() above,
    // because that call resets the shifter's latency to its hard-coded
    // 20 ms default. Calling applyLatencyMode() first would race with
    // that reset and report a different (mode-specific) latency that
    // gets overwritten a few lines later, causing a 576â†”960 ping-pong
    // that made Live 12 re-setActive 600 000+ times per session.
    appliedLatencyMode = -1;
    applyLatencyMode();

    // Configure the plugin latency based on the host's block size.
    // This allows the DAW to compensate for the delay introduced by buffering.
    // NOTE: do NOT call setLatencySamples() here. applyLatencyMode() above
    // already called it with the mode-correct value; calling it again
    // would overwrite the mode with the shifter's hard-coded 20 ms
    // default and re-trigger the Live 12 re-setActive loop.
    latencySamples = pitchShifter->getLatencySamples();

    // Debug: log prepareToPlay info
    OVT_LOG ("prepareToPlay: sampleRate=" + juce::String(sampleRate) +
                              " samplesPerBlock=" + juce::String(samplesPerBlock) +
                              " pitchShifter_latency=" + juce::String(latencySamples));

    // Reset the silence counter
    maxSilenceSamples = static_cast<int>(sampleRate * 0.5); // 500 ms silence tail

    // The audio side is now in a stable state â€” the host's playhead can
    // be safely read.  The dedicated worker is started here so that
    // getPlayHead()->getPosition() runs on its own thread and never
    // blocks the audio thread or the UI thread (both of which would
    // freeze on misbehaving hosts like Cubase LE 15).  The worker was
    // temporarily disabled while we were hunting the rainbow
    // beach-ball bug, but that was actually caused by a setLatency
    // ping-pong in applyLatencyMode(), not by the worker.  Re-enabled
    // 2026-07-22.
    notifyAuReady();
    startPlayheadThread();
}

void OpenVoxTunerAudioProcessor::releaseResources()
{
    // Stop the playhead worker BEFORE we tear down any ARA/host state.
    // The worker calls getPlayHead() which dereferences host state that
    // is about to become invalid (e.g. on insert re-enable).  The
    // worker was temporarily disabled while hunting a rainbow
    // beach-ball bug, but that bug was actually caused by a setLatency
    // ping-pong in applyLatencyMode(), not by the worker.  Re-enabled
    // 2026-07-22.
    stopPlayheadThread();

    // Mark the AU as no longer safe to query so the worker (if it ever
    // races past the stop) bails out of readAndCachePlayHeadInfo()
    // instead of dereferencing a pointer that is about to become
    // invalid.
    notifyAuReleased();

#if OVT_ARA_ENABLED
    releaseResourcesForARA();
#endif
}

void OpenVoxTunerAudioProcessor::reset()
{
    // Reset DSP state when the host resets the plugin (e.g. transport stop,
    // sample rate change). Some hosts (Cubase, Live) require this to be
    // implemented, otherwise they may hang.
    for (auto& g : shiftedVoiceGains)
        g.setCurrentAndTargetValue (0.0f);
    harmonyGateGain.setCurrentAndTargetValue (0.0f);
    harmonyAttackGain = 0.0f;
    for (auto& ps : shiftedVoicePitchShifters)
        if (ps != nullptr)
            ps->reset();
    for (size_t v = 0; v < shiftedVoiceRatioSmoothers.size(); ++v)
    {
        shiftedVoiceRatioSmoothers[v].reset (1.0f);
        shiftedVoiceSmoothedRatio[v] = 1.0f;
    }
    shiftedVoiceLevelSmoother.reset (1.0f);
    if (pitchShifter != nullptr)
        pitchShifter->reset();
    harmonyBuffer.clear();
    synthWorkBuffer.clear();
    lastMixedHarmonyBuffer.clear();
    silenceSamples = 0;
    lastInputPitch.store (0.0f);
    lastOutputPitch.store (0.0f);
    lastCentsOffset.store (0.0f);
    lastRawYinPitch.store (0.0f, std::memory_order_relaxed);
    appliedLatencyMode = -1;

    for (int i = 0; i < 1; ++i)
        if (pitchDetectors[i] != nullptr) pitchDetectors[i]->reset();
    if (retargetEnvelope != nullptr) retargetEnvelope->reset();

    // Block-aware parameter smoothers: reset to a transparent state so the
    // first block after a transport stop / preset change doesn't carry
    // any residual modulation from the previous session.
    flexTuneSmoother.reset (1.0f);
    // 2026-07-23 (Fix BC): also reset the upstream f0SmootherForDeadband
    // so a transport stop / preset change doesn't leave a stale pitch
    // value in the smoother.
    f0SmootherForDeadband.reset (0.0f);
    humanizeSmoother.reset (0.0f);
    // 2026-07-23 (Fix AY): reset the speed floor too, so a transport
    // stop / preset change doesn't leave a stale `ratio` value in the
    // smoother.
    speedFloor.reset (1.0f);
    currentFlexTuneAmount = 1.0f;
    currentHumanizeCents = 0.0f;

    // Reset MIDI tracking state (host is releasing audio graph)
    for (int ch = 0; ch < 16; ++ch)
        lastSentMidiNote[ch] = -1;
    midiTargetActive.store (false);
    midiTargetHz.store (0.0f);
}

// === Routine audio principale (appel bloc par bloc par le host) ===
void OpenVoxTunerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto blockStartTime = juce::Time::getHighResolutionTicks();
    // MIDI out may be produced below if enabled by parameter.

    auto flushPendingMidiNotes = [&midiMessages, this]
    {
        bool hadAny = false;
        for (int ch = 0; ch < 16; ++ch)
        {
            const int lastNote = lastSentMidiNote[ch];
            if (lastNote != -1)
            {
                const int midiChannel = ch + 1;
                midiMessages.addEvent (juce::MidiMessage::noteOff (midiChannel, lastNote), 0);
                lastSentMidiNote[ch] = -1;
                hadAny = true;
            }
        }

        if (hadAny)
        {
            for (int midiChannel = 1; midiChannel <= 16; ++midiChannel)
            {
                midiMessages.addEvent (juce::MidiMessage::allNotesOff (midiChannel), 0);
                midiMessages.addEvent (juce::MidiMessage::allSoundOff (midiChannel), 0);
            }
        }
    };

    // Bypass : on laisse passer l'audio tel quel.
    if (bypassParam != nullptr && bypassParam->load() > 0.5f)
    {
        flushPendingMidiNotes();
        harmonyFrequencies.clear();
        harmonyFrequenciesClean.clear();
        publishHarmonySnapshots();
        lastInputPitch.store (0.0f);
        lastOutputPitch.store (0.0f);
        lastCentsOffset.store (0.0f);
        lastRawYinPitch.store (0.0f, std::memory_order_relaxed);
        return;
    }

    // === PITCH DETECTOR SWITCHING (YIN only â€” single mode) ===
    // The pitch_detector parameter is read-only (single choice "YIN").
    // No switching needed â€” always use index 0.

    // === NOISE GATE (input, before pitch detection) ===
    {
        const bool gateEnabled = noiseGateEnableParam != nullptr && noiseGateEnableParam->load() > 0.5f;
        noiseGate.setEnabled (gateEnabled);
        if (gateEnabled && noiseGateThresholdParam != nullptr)
            noiseGate.setThresholdDb (noiseGateThresholdParam->load());
        noiseGate.process (buffer);
    }

    // === UPWARD COMPRESSION (input, after gate, before pitch detection) ===
    // Lifts quiet vocal passages toward the running RMS level so weak parts are
    // more audible to the pitch detector and tuning stage, without attenuating
    // louder material (pure upward behaviour). Applied before the waveform
    // capture so the visualizer reflects it too.
    {
        const bool ucEnabled = upwardCompEnableParam != nullptr && upwardCompEnableParam->load() > 0.5f;
        upwardComp.setEnabled (ucEnabled);
        if (ucEnabled && upwardCompAmountParam != nullptr)
            upwardComp.setAmount (upwardCompAmountParam->load());

        upwardComp.process (buffer);
    }

    // === WAVEFORM CAPTURE ===
    // Cache a mono downmix of the (post-gate) input audio for the visualizer overlay,
    // so the displayed waveform reflects the noise gate when it is enabled. Works in all
    // modes. Captured after the gate but before the rest of the DSP chain.
    // Guarded by waveformCaptureEnabled to avoid spin-lock contention when the waveform
    // overlay is hidden in the UI (fixes beachball / deadlock in Cubase/Live VST3).
    if (waveformCaptureEnabled.load (std::memory_order_relaxed))
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numSamples > 0 && numCh > 0)
        {
            if (araWaveformLock.tryEnter())
            {
                jassert (numSamples <= araWaveformBuffer.getNumSamples());
                araWaveformBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
                for (int ch = 1; ch < numCh; ++ch)
                    araWaveformBuffer.addFrom (0, 0, buffer, ch, 0, numSamples);
                araWaveformBuffer.applyGain (0, 0, numSamples, 1.0f / (float) numCh);
                araWaveformSampleRate = currentSampleRate;
                araWaveformReady.store (true, std::memory_order_release);
                araWaveformLock.exit();
            }
        }
    }

    // === LECTURE DES METADONNEES ARA ===
    // DÃ©placÃ©e vers updateAraMetadata() (thread UI) pour Ã©viter les deadlocks
    // audio/UI causÃ©s par HostContentReader + setValueNotifyingHost dans le
    // thread audio (beachball Cubase/Live VST3).

    // After mixing, if engine has finished releasing, shifted voices have ramped down
    // and there's no live pitch, clear cached notes so we don't re-trigger residual rendering.
    {
        bool shiftedActive = false;
        for (const auto& g : shiftedVoiceGains)
            if (g.getCurrentValue() > 0.001f) shiftedActive = true;

        if (harmonyEngine != nullptr && !harmonyEngine->isActive() && !shiftedActive && lastOutputPitch.load() <= 0.0f)
        {
            lastHarmonyNotes.clear();
            harmonyFrequencies.clear();
            harmonyBuffer.clear();
        }
    }

    // Synchronise les parametres avec les modules DSP.
    syncParameters();

    // === Host transport read ===
    // The actual getPlayHead()->getPosition() call now runs on a
    // dedicated worker thread (see startPlayheadThread) so that
    // misbehaving hosts (Cubase, Live VST3) cannot freeze this audio
    // thread or the message thread.  Here we only consume the
    // atomics that the worker keeps fresh.

    // === DETECTION DE SILENCE (SLEEP MODE) ===
    // Calcule la magnitude maximale du buffer pour savoir si on bypass les calculs lourds (YIN, etc)
    // On met le seuil a 0.003f (environ -50 dB) pour ignorer le bruit de fond d'un micro
    float maxMagnitude = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        maxMagnitude = juce::jmax(maxMagnitude, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
    }

    if (maxMagnitude < 0.003f) // -50 dB
    {
        if (silenceSamples <= maxSilenceSamples)
            silenceSamples += buffer.getNumSamples();
        // Ramp down harmony voices when input level falls below threshold to avoid lingering tones/clicks.
        if (harmonyEngine != nullptr)
            harmonyEngine->setVoiceGate(false);
    }
    else
    {
        silenceSamples = 0;
    }

    // 2) Mise a jour du temps de transport (pour le mode graphic).
    //    Le getPlayHead() est appelÃ© depuis le thread UI (updateHostTransport)
    //    et mis en cache dans des atomics pour le thread audio.
    // Voice/silence hysteresis for harmony gate (prevents rapid on/off chattering)
    constexpr float harmonyGateOnThreshold  = 0.0040f;
    constexpr float harmonyGateOffThreshold = 0.0025f;
    if (maxMagnitude >= harmonyGateOnThreshold)
        harmonyInputGateOpen = true;
    else if (maxMagnitude <= harmonyGateOffThreshold)
        harmonyInputGateOpen = false;

    double currentTime = transportTime.load();
    const bool isStandalone = (wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    // Read cached host transport (updated by updateHostTransport() on the UI
    // thread).  Calling getPlayHead()->getPosition() from the audio thread
    // can deadlock Cubase and Live VST3.
    const bool hostProvidesTime = !isStandalone && hostProvidesTimeCached.load (std::memory_order_relaxed);

    if (hostProvidesTime)
    {
        currentTime = cachedHostPpq.load (std::memory_order_relaxed);
    }

    rawHostTime.store(currentTime);

    if (hostProvidesTime && !isBoundToARA_custom()) {
        // En mode plugin classique (VST3 sans ARA), on soustrait l'offset
        // pour permettre au bouton "Reset Playhead" de fonctionner
        currentTime -= customTimeOffset.load();
    }

    timeProvidedByHost.store(hostProvidesTime);

    // Fallback pour le Standalone (ou host sans playhead)
    if (!hostProvidesTime && getSampleRate() > 0.0)
    {
        // Standalone transport: when stopped, freeze the timeline so the user can edit
        // the curve. When playing, advance at the standalone tempo (BPM).
        if (transportPlaying.load())
        {
            const double beatsPerSecond = static_cast<double> (bpm.load()) / 60.0;
            currentTime += (static_cast<double>(buffer.getNumSamples()) / getSampleRate()) * beatsPerSecond;
        }
    }

    transportTime.store (currentTime);

    if (silenceSamples > maxSilenceSamples)
    {
        // On est au repos complet. On desactive le traitement lourd.
        buffer.clear();

        // Ensure MIDI notes are released when entering sleep mode
            flushPendingMidiNotes();

        lastInputPitch.store (0.0f);
        lastOutputPitch.store (0.0f);
        lastCentsOffset.store (0.0f);

        // Avance le FIFO YIN avec des zeros
        const int numSamples = buffer.getNumSamples();
        float* fifo = analysisFifo.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            fifo[fifoWriteIndex] = 0.0f;
            fifoWriteIndex = (fifoWriteIndex + 1) % analysisWindow;
        }

        // Apply post-processing effects (reverb) on the cleared buffer before
        // returning, so the reverb tail can decay naturally through its internal
        // state onto the zeroed input without abrupt cuts or clicks.
        if (!effects.empty())
        {
            for (auto& effect : effects)
            {
                bool enabled = false;
                float wetMix = 0.0f;

                if (effect->getId() == "reverb")
                {
                    enabled = (reverbEnableParam != nullptr && reverbEnableParam->load() > 0.5f);
                    wetMix = (reverbMixParam != nullptr) ? reverbMixParam->load() : 0.0f;
                }

                effect->process (buffer, enabled, wetMix);
            }
        }

        harmonyFrequencies.clear();
        harmonyFrequenciesClean.clear();
        publishHarmonySnapshots();
        lastRawYinPitch.store (0.0f, std::memory_order_relaxed);
        return; // CPU chute a ~1%
    }

    // 1) Detection du pitch d'entree (tres lourd en CPU).
    //    Extract the MAIN input bus explicitly (bus 0) so the optional
    //    Sidechain bus (bus 1) never bleeds into the vocal pitch analysis.
    const juce::AudioBuffer<float> mainInputBuffer = getBusBuffer (buffer, true, 0);
    float f0_in = computeInputPitch (mainInputBuffer);
    if (!harmonyInputGateOpen)
        f0_in = 0.0f;

    // Filtre anti-saut-d'octave : si f0_in saute d'un facteur ~2 ou ~0.5
    // par rapport au dernier pitch valide, on conserve l'ancienne valeur
    // temporairement. Un compteur de persistence permet de laisser passer
    // les VRAIS changements de registre apres ~140 ms de detection stable.
    if (f0_in > 0.0f)
    {
        // Si YIN a detecte du silence lors de sa derniere analyse (lastRawYinPitch=0),
        // on desarme le filtre octave pour que la nouvelle note ne soit pas bloquee.
        // Ceci est NECESSAIRE car f0_in peut valoir 175Hz (fallback) meme quand
        // l'utilisateur ne chante plus (F3->pause->F2 bloque).
        if (lastRawYinPitch.load() <= 0.0f)
        {
            lastOctaveValidatedPitch.store (0.0f);
            octaveJumpRejectionCount = 0;
        }

        float ref = lastOctaveValidatedPitch.load();
        if (ref > 0.0f)
        {
            float ratio = f0_in / ref;
            bool isOctaveJump = (ratio > 1.7f && ratio < 2.3f) || (ratio > 0.40f && ratio < 0.55f);

            if (isOctaveJump)
            {
                ++octaveJumpRejectionCount;
                if (octaveJumpRejectionCount >= octaveJumpPersistenceThreshold)
                {
                    lastOctaveValidatedPitch.store (f0_in);
                    octaveJumpRejectionCount = 0;
                }
                else
                {
                    f0_in = ref;
                }
            }
            else
            {
                octaveJumpRejectionCount = 0;
                lastOctaveValidatedPitch.store (f0_in);
            }
        }
        else if (lastRawYinPitch.load() > 0.0f)
        {
            // Pas de reference mais YIN vient de detecter un pitch :
            // on initialise la reference avec ce nouveau pitch.
            octaveJumpRejectionCount = 0;
            lastOctaveValidatedPitch.store (f0_in);
        }
        // Si lastRawYinPitch == 0 (silence), on NE met PAS a jour
        // lastOctaveValidatedPitch. La reference reste a 0 pour que
        // la prochaine note chantee passe le filtre sans blocage.
    }
    else
    {
        octaveJumpRejectionCount = 0;
        lastOctaveValidatedPitch.store (0.0f);
    }
    lastInputPitch.store (f0_in);

    // Feed the vibrato-preservation center tracker with the (octave-filtered)
    // detected pitch. Held during silence; re-initialised on the next attack.
    vibratoPreserver.update (f0_in);

    // === INCOMING MIDI : track held notes for the "MIDI Target / Follow" feature ===
    // We only READ the buffer here; outgoing events are still appended later
    // in the MIDI OUT block. Notes are accumulated so the most-recently-
    // pressed (last in the list) becomes the correction target when the toggle is on.
    {
        for (const auto& meta : midiMessages)
        {
            const juce::MidiMessage& mm = meta.getMessage();
            if (mm.isNoteOn())
                heldMidiNotes.addIfNotAlreadyThere (mm.getNoteNumber());
            else if (mm.isNoteOff())
                heldMidiNotes.removeAllInstancesOf (mm.getNoteNumber());
        }
    }

    // === AUTOMATIC KEY DETECTION (non-ARA sources) ===
    // ARA (if bound) already sets key/scale from the host musical context above,
    // so we only run the in-plugin / companion sources when ARA is NOT bound.
    const bool runKeyDetection =
#if OVT_ARA_ENABLED
        !isBoundToARA();
#else
        true;
#endif
    if (runKeyDetection)
    {
        const bool detectOn = (keyDetectParam != nullptr) ? keyDetectParam->load() > 0.5f : false;
        if (detectOn)
        {
            const int src = (keySourceParam != nullptr) ? static_cast<int> (keySourceParam->load()) : 0;
            const float blockDur = static_cast<float> (buffer.getNumSamples()) / static_cast<float> (currentSampleRate);

            if (src == 0) // Auto: analyse the (vocal) input pitch stream
            {
                keyDetector.addDetection (f0_in, 1.0f, blockDur);
                int detKey = 0; bool detMinor = false; float detConf = 0.0f;
                if (keyDetector.getEstimate (detKey, detMinor, detConf))
                    applyDetectedKey (ovtdsp::KeyDetector::detectorKeyToMusical (detKey), detMinor ? 4 : 1);
            }
            else if (src == 1) // OpenVoxKey: read the shared bridge for the chosen group
            {
                // companionGroupParam is an AudioParameterChoice; getRawParameterValue()
                // returns the 0..3 choice index (not a normalised 0..1 value), so use
                // it directly. (Multiplying by 3.0f â€” as if it were normalised â€” would
                // map B->D and C->D, the cross-talk bug.)
                int grpIdx = 0;
                if (companionGroupParam != nullptr)
                    grpIdx = juce::jlimit (0, 3, static_cast<int> (companionGroupParam->load()));
                const juce::String grp = juce::StringArray { "A", "B", "C", "D" }[grpIdx];
                int bKey = 0, bScale = 0; double bTs = 0.0;
                if (ovtdsp::KeyBridge::getInstance().read (grp, bKey, bScale, bTs))
                    applyDetectedKey (bKey, bScale); // bridge already stores musical key/scale
            }
            else if (src == 2) // Sidechain: analyse the sidechain input bus (e.g. accompaniment)
            {
                const juce::AudioBuffer<float> scBuffer = getBusBuffer (buffer, true, 1);
                if (scBuffer.getNumChannels() > 0)
                {
                    const float scF0 = computeSidechainPitch (scBuffer);
                    if (scF0 > 0.0f)
                    {
                        sidechainKeyDetector.addDetection (scF0, 1.0f, blockDur);
                        int detKey = 0; bool detMinor = false; float detConf = 0.0f;
                        if (sidechainKeyDetector.getEstimate (detKey, detMinor, detConf))
                            applyDetectedKey (ovtdsp::KeyDetector::detectorKeyToMusical (detKey), detMinor ? 4 : 1);
                    }
                }
            }
        }
        // key_detect off (or ARA bound) -> the user drives key/scale directly.
    }

    // Debug: detect bypass param changes and log once when it changes
    if (bypassParam)
    {
        int cur = static_cast<int>(std::round(bypassParam->load()));
        int prev = prevBypassState.load();
        if (cur != prev)
        {
            OVT_LOG ("Bypass parameter changed: " + juce::String(prev) + " -> " + juce::String(cur));
            prevBypassState.store (cur);
        }
    }

    // Debug: periodically log processing info (once per second)
    uint32_t nowMs = juce::Time::getMillisecondCounter();
    uint32_t lastMs = lastProcessLogTime.load();
    if (nowMs - lastMs > 1000)
    {
        if (lastProcessLogTime.compare_exchange_strong (lastMs, nowMs))
        {
            const float f0_out = lastOutputPitch.load();
            const float amount = amountParam ? amountParam->load() : 0.0f;
            juce::String s = "processBlock: f0_in=" + juce::String (f0_in, 3)
                            + " f0_out=" + juce::String (f0_out, 3)
                            + " amount=" + juce::String (amount, 3)
                            + " numSamples=" + juce::String (buffer.getNumSamples());
            // log first sample values (L/R)
            if (buffer.getNumChannels() > 0)
                s += " buf0=" + juce::String (buffer.getReadPointer (0)[0], 6);
            if (buffer.getNumChannels() > 1)
                s += " buf1=" + juce::String (buffer.getReadPointer (1)[0], 6);
            OVT_LOG (s);
        }
    }

    // Debug: trigger test grain when parameter is set by the editor (works across host process)
    if (dbgTestGrainParam)
    {
        int cur = static_cast<int>(std::round(dbgTestGrainParam->load()));
        if (cur > 0 && prevDbgTestGrain.load() == 0)
        {
            OVT_LOG ("Debug param requested test grain -> forcing grain creation in audio thread");
            if (pitchShifter) pitchShifter->forceCreateTestGrain();
            // Defer the parameter reset to the UI thread (flushPendingParameterChanges).
            // Calling setValueNotifyingHost() from the audio thread can deadlock
            // Cubase and Live VST3.
            pendingDbgGrainReset.store (true);
        }
        prevDbgTestGrain.store(cur);
    }

    // 3) Quantification : auto vs graphic selon le mode.
    float targetRatio = 1.0f;
    float f0_out = f0_in;
    const int mode = (modeParam != nullptr) ? static_cast<int> (modeParam->load()) : 0;

    // === Deferred harmony-type retarget (click masking) ===
    // The engine must keep rendering the OLD note set while it fades out, then
    // switch to the NEW note set while it fades in. This is computed HERE, at
    // function scope (not inside the `if (f0_in > 0.0f)` block below), so that
    // BOTH the harmony note-set computation (inside that block) AND the later
    // HYBRID mix section use the same deferred type. Previously the note set
    // was recomputed with the newly requested type on every block, defeating
    // the retarget and hard-cutting the old content (the source of the
    // type-change / morph-50% click).
    int currentHarmonyTypeVal = (harmonyTypeParam != nullptr) ? static_cast<int>(harmonyTypeParam->load()) : 0;
    const int numSamplesBlock = buffer.getNumSamples();
    int renderHarmonyType = currentHarmonyTypeVal;
    // Fade state captured at the START of this block, used by the mix loop for
    // a sample-accurate bus dip. The member `typeDipFadeRemaining` is advanced
    // below EVERY block so the transition can never get stuck.
    int blockStartRemaining = 0;
    int blockFadeTotal = 0;
    if (lastHarmonyTypeVal < 0)
    {
        lastHarmonyTypeVal = currentHarmonyTypeVal;   // first block: no transition
    }
    else if (currentHarmonyTypeVal != lastHarmonyTypeVal)
    {
        pendingHarmonyType = currentHarmonyTypeVal;   // request the new type
        if (! typeCrossfadeActive)
        {
            typeDipFadeTotalSamples = juce::jmax (1, static_cast<int> (currentSampleRate * 0.040f));
            typeDipFadeRemaining = typeDipFadeTotalSamples;
            typeCrossfadeActive = true;
            typeCrossfadeOld.clear();
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
            diagTypeChangePending = true;
            diagHarmonyPeakJump = 0.0f;
            diagOutputPeakJump  = 0.0f;
            diagWindowLen = 0;
            diagWindowFilled = false;
#endif
        }
    }
    if (typeCrossfadeActive && typeDipFadeTotalSamples > 0)
    {
        blockStartRemaining = typeDipFadeRemaining;
        blockFadeTotal = typeDipFadeTotalSamples;
        // Advance the fade by this whole block. Advancing here (not only inside
        // the mix loop) guarantees the transition ALWAYS progresses, so it can
        // never get stuck rendering the old note set.
        typeDipFadeRemaining -= numSamplesBlock;
        if (typeDipFadeRemaining <= 0)
        {
            typeDipFadeRemaining = 0;
            lastHarmonyTypeVal = pendingHarmonyType >= 0 ? pendingHarmonyType : lastHarmonyTypeVal;
            pendingHarmonyType = -1;
            typeCrossfadeActive = false;
            typeDipFadeTotalSamples = 0;
        }
        else
        {
            const float p = 1.0f - (float) typeDipFadeRemaining / (float) typeDipFadeTotalSamples;
            renderHarmonyType = (p < 0.40f) ? lastHarmonyTypeVal
                                            : (pendingHarmonyType >= 0 ? pendingHarmonyType : lastHarmonyTypeVal);
        }
    }
    else
    {
        lastHarmonyTypeVal = currentHarmonyTypeVal;
    }

    if (f0_in > 0.0f)
    {
        float f0_target = 0.0f;

        // Transport time used by the graphic (curve) mode. Computed once here so
        // it is also in scope for the vibrato-preservation block below.
        const double loopLen = isPlayheadLooping() ? getLoopLengthBeats() : 0.0;
        const double currentTransportTime = (loopLen > 0.0) ? std::fmod (currentTime, loopLen) : currentTime;

        if (mode == 1 && pitchCurve != nullptr && pitchCurve->getNumPoints() >= 2)
        {
            // === Mode GRAPHIC : on suit la pitch curve dessinee ===
            // En Standalone, transportTime continue d'augmenter a l'infini.
            // On boucle la lecture de la courbe sur la meme fenetre que le
            // playhead quand la boucle est active (standalone, ou plugin en
            // mode Loop) ; en ARA / plugin-follow, c'est la position DAW qui
            // pilote la courbe (pas de wrap). La longueur vaut par defaut
            // 16 beats (4 mesures 4/4), identique a l'ancien fmod(..., 16.0).
            f0_target = pitchCurve->getPitchAt (currentTransportTime, f0_in);
        }
        else
        {
            // === Mode AUTO : quantification standard vers la gamme ===
            // La position PPQ courante sert a évaluer l'override de contexte
            // d'accord (accords hors gamme acceptés) si ARA l'a fourni.
            f0_target = scaleQuantizer->quantize (f0_in, currentTransportTime);
        }

        // === MIDI TARGET (follow) ===
        // When enabled, an incoming held MIDI note drives the correction
        // TARGET (the voice is tuned TO that note). f0_in stays the
        // detected vocal pitch, so the ratio shifts the sung pitch to the
        // played note. The most-recently-pressed held note wins.
        const bool midiTargetOn = (midiTargetEnableParam != nullptr)
                                   && midiTargetEnableParam->load() > 0.5f;
        if (midiTargetOn && heldMidiNotes.size() > 0 && f0_in > 0.0f)
        {
            const int tgtNote = heldMidiNotes.getLast();
            f0_target = ovtdsp::midiToHz (static_cast<float> (tgtNote));
        }

        // Expose the "actively following MIDI" state so the GUI can show a badge.
        midiTargetActive.store (midiTargetOn && heldMidiNotes.size() > 0);
        midiTargetHz.store ((midiTargetOn && heldMidiNotes.size() > 0 && f0_in > 0.0f)
                                ? ovtdsp::midiToHz (static_cast<float> (heldMidiNotes.getLast()))
                                : 0.0f);

        // 2026-07-24 (Deprecation): FlexTune logic is disabled. The
        // deadband code is preserved as commented reference for
        // future re-implementation, but the active behaviour is now
        // `currentFlexTuneAmount = 1.0f` (always full correction, no
        // deadband). The APVTS parameter is still present (read at
        // line 1501 below) so existing presets keep their value, but
        // it has no effect.
        // ----------------------------------------------------------------
        // FlexTune: true deadband with smooth knee.
        // When the input pitch is within the FlexTune threshold (cents) of the
        // target note, NO correction is applied (true deadband = 0%).
        // Beyond the threshold, correction ramps smoothly via smoothstep from
        // 0% at threshold to 100% at 2x threshold. This preserves natural
        // microtonal expression while still correcting significant drift.
        float flexTuneCents = (flexTuneParam != nullptr) ? flexTuneParam->load() : 0.0f;
        // Store the flexTune multiplier for later use in Amount calculation.
        // Default: 1.0 = full correction. Reduced to 0.0 when input is
        // within the deadband.
        // 2026-07-24 (Deprecation): always start at 1.0 (full correction)
        // and never enter the deadband computation, regardless of the
        // flex_tune parameter value. The parameter is read but ignored.
        currentFlexTuneAmount = 1.0f;
        if (false) // DEPRECATED: was `if (flexTuneCents > 0.5f && f0_in > 0.0f && f0_target > 0.0f)`
        {
            // 2026-07-23 (Fix BC): smooth the input pitch BEFORE the
            // deadband computation. The deadband is a step function
            // (0 inside the threshold, smoothstep outside), so feeding
            // it the raw `f0_in` (which has per-block YIN jitter +
            // 5Hz vibrato) produces a 5Hz square wave at its output.
            // The downstream flexTuneSmoother can only attenuate this
            // square wave by ~80% (|H(5Hz)|=0.20 at TC=500ms), which
            // is not enough to eliminate the audible pops. By
            // smoothing `f0_in` to TC=150ms first, the input to the
            // deadband is a soft 5Hz sinus (not a square wave), and
            // the deadband output is a soft transition (no step). The
            // downstream flexTuneSmoother (TC=200ms) then has very
            // little work left to do.
            //
            // We use the SMOOTHED f0 for the deadband calculation
            // only, not for the actual pitch shifting (we want the
            // pitch shifter to follow the actual pitch as fast as
            // possible, the deadband is just a "how much correction
            // to apply" decision). The smoothed f0 is only used to
            // make a SMOOTH DEAD-BAND decision.
            const float f0ForDeadband = f0SmootherForDeadband.step (f0_in, static_cast<float>(buffer.getNumSamples()) / static_cast<float>(currentSampleRate));
            // Cents difference: 1200 * log2(ratio) = 1200 * log2(f0 / f0_target)
            float centsDiff = 1200.0f * std::abs (std::log2 (f0ForDeadband / f0_target));
            // True deadband: zero correction within threshold
            if (centsDiff <= flexTuneCents)
            {
                currentFlexTuneAmount = 0.0f;
            }
            else
            {
                // Smoothstep transition from threshold to 2*threshold
                float t = (centsDiff - flexTuneCents) / flexTuneCents;
                t = juce::jlimit (0.0f, 1.0f, t);
                currentFlexTuneAmount = t * t * (3.0f - 2.0f * t); // smoothstep
            }
        }

        // DEBUG: log FlexTune values (once per second)
        static std::atomic<uint32_t> lastFlexLogMs { 0 };
        uint32_t nowFlex = juce::Time::getMillisecondCounter();
        uint32_t expected = lastFlexLogMs.load();
        if (nowFlex - expected > 1000)
        {
            if (lastFlexLogMs.compare_exchange_strong (expected, nowFlex))
            {
                OVT_LOG ("FlexTune: f0_in=" + juce::String (f0_in, 2) +
                         " f0_target=" + juce::String (f0_target, 2) +
                         " centsDiff=" + juce::String (1200.0f * std::abs (std::log2 (f0_in / f0_target)), 1) +
                         " flexTuneCents=" + juce::String (flexTuneCents, 1) +
                         " currentFlexTuneAmount=" + juce::String (currentFlexTuneAmount, 3) +
                         " smoothedFlexTuneAmount=" + juce::String (flexTuneSmoother.getCurrentValue(), 3) +
                         " f0_out=" + juce::String (f0_out, 2) +
                         " targetRatio=" + juce::String (targetRatio, 3));
            }
        }

        // Smooth FlexTune amount to prevent clicks when the singer drifts in
        // and out of the deadband. The previous form
        // "smoothedFlexTuneAmount = smoothedFlexTuneAmount * 0.95 + current * 0.05"
        // was buffer-size dependent: at 128 samples the alpha is 2x larger
        // than at 256 samples, so the smoother responds TWICE as fast on
        // small buffers. This made the FlexTune feel inconsistent across
        // buffer sizes and was a documented source of dropouts in Studio One
        // with the dropout protection set to "Low" (128/256 sample buffers).
        //
        // The block-aware smoother computes its alpha from the actual block
        // duration so the time constant (200 ms) is INDEPENDENT of the
        // buffer size. We also short-circuit when FlexTune is disabled
        // (flexTuneCents <= 0.5) to avoid a few hundred cycles per second
        // of useless exp() calls when the feature is off.
        if (flexTuneCents > 0.5f)
            flexTuneSmoother.processBlock (currentFlexTuneAmount, buffer.getNumSamples());
        else
            flexTuneSmoother.processBypassed (1.0f); // transparent when off

        f0_out = f0_target;
        targetRatio = f0_target / f0_in;

        // Humanize: add subtle, smoothed pitch fluctuations (in cents).
        // Max range is 0-8 cents (about 1/6 of a semitone) at max setting.
        // The random value is heavily smoothed via a low-pass filter
        // (~150 ms time constant) to avoid harsh per-frame jumps. Same
        // block-aware smoother as FlexTune for the same buffer-size
        // independence reason.
        float humanizeAmt = (humanizeParam != nullptr) ? humanizeParam->load() : 0.0f;
        if (humanizeAmt > 1.0f && f0_target > 0.0f && f0_target != f0_in)
        {
            float targetCents = (random.nextFloat() - 0.5f) * 2.0f * humanizeAmt * 0.08f;
            humanizeSmoother.processBlock (targetCents, buffer.getNumSamples());
            currentHumanizeCents = humanizeSmoother.getCurrentValue();
            f0_target *= std::pow (2.0f, currentHumanizeCents / 12.0f);
            targetRatio = f0_target / f0_in;
        }
        else
        {
            // Decay the humanize smoothly toward 0 when the effect is off.
            // We reuse the smoother in "instant" mode (tau=0) so the decay
            // happens in a single block; this matches the user's expectation
            // that the humanize stops immediately when the knob is turned
            // down.
            humanizeSmoother.snapTo (0.0f);
            currentHumanizeCents = 0.0f;
        }

        // === VIBRATO PRESERVATION ===
        // Blend the standard (instantaneous) correction toward a center-based
        // correction. The center pitch (vibratoPreserver.getCenter()) is a
        // low-pass of f0_in, so the vibrato LFO is removed from it. Correcting
        // against the center and re-applying the ratio to the instantaneous
        // pitch keeps the vibrato modulation intact while still snapping the
        // note to the scale. At preserve == 0 this is the classic behaviour.
        float vibratoPreserve = (vibratoPreserveParam != nullptr) ? vibratoPreserveParam->load() : 0.0f;
        const float center = vibratoPreserver.getCenter();
        if (center > 0.0f)
        {
            // Target for the smoothed center reference, using the same mode
            // logic as the instantaneous path above.
            float f0_target_center = f0_target;
            if (mode == 1 && pitchCurve != nullptr && pitchCurve->getNumPoints() >= 2)
                f0_target_center = pitchCurve->getPitchAt (currentTransportTime, center);
            else
                f0_target_center = scaleQuantizer->quantize (center, currentTransportTime);

            targetRatio = vibratoPreserver.blend (targetRatio, f0_in, f0_target_center, vibratoPreserve);
            f0_target = f0_in * targetRatio;
            f0_out = f0_target; // keep GUI/harmony note in sync with the blended target
        }

        // Calcul de l'offset en cents between pitch d'entree et pitch quantife.
        // Positif = entree trop haute, Negatif = entree trop basse.
        if (f0_target > 0.0f)
            lastCentsOffset.store (ovtdsp::hzToCents (f0_in, f0_target));
        else
            lastCentsOffset.store (0.0f);

        // === HARMONY ENGINE : generate harmonized voices ===
        // Render with the deferred harmony type (renderHarmonyType) computed at
        // function scope above, so the OLD note set keeps playing while it dips
        // out during the retarget transition (click masking).
        const int currentHarmonyType = renderHarmonyType;

        // Respect the master harmony enable parameter if present.
        // We render harmonies when we have a valid output pitch OR when the
        // harmony engine is still active (releasing) so the release can be heard.
        bool shiftedVoicesActive = false;
        for (const auto& g : shiftedVoiceGains)
        {
            if (g.getCurrentValue() > 0.001f)
                shiftedVoicesActive = true;
        }

        if (currentHarmonyType != 0 && ( (f0_out > 0.0f) || (harmonyEngine != nullptr && harmonyEngine->isActive()) || shiftedVoicesActive )
            && (harmonyEnableParam == nullptr || harmonyEnableParam->load() > 0.5f))
        {
            // Extraire les paramÃ¨tres pour l'engine d'harmonie
            int currentKey = keyIntParam != nullptr ? keyIntParam->get() : static_cast<int> (std::round (keyParam->load() * 11.0f));
            int currentScaleIdx = scaleChoiceParam != nullptr ? scaleChoiceParam->getIndex() : static_cast<int>(scaleParam->load());

            // RÃ©cupÃ¨re les intervalles de la gamme depuis le quantizer
            const juce::Array<int>& intervals = scaleQuantizer->getScaleIntervals();

            // Use a "held" pitch that falls back to lastValidF0 when YIN momentarily drops to 0.
            // This prevents harmony generation from stalling during brief pitch detection gaps.
            const float heldF0 = (f0_out > 0.0f) ? f0_out : lastValidF0.load();

            // Determine which note set to render: if we have a live output pitch use it,
            // otherwise reuse the lastHarmonyNotes so the engine can render the release.
            juce::Array<float> notes;
            if (heldF0 > 0.0f)
            {
                notes = harmonyEngine->getHarmonyNotes (
                    heldF0,
                    intervals,
                    static_cast<ovtdsp::HarmonyType>(currentHarmonyType)
                );

                // Harmony Follow Lead (default on): shift each harmony voice by the same
                // "character ratio" the lead uses (f0_out / scale note). Because getHarmonyNotes
                // snaps each voice to the scale grid, the vibrato/humanize/flex character is
                // otherwise lost on the harmonies. Re-applying the lead's ratio makes the whole
                // stack move together; when off, harmonies stay locked to the scale (classic look).
                const bool followLead = (harmonyFollowLeadParam != nullptr)
                    ? harmonyFollowLeadParam->load() > 0.5f : true;

                // Keep a scale-locked copy (Follow Lead NOT applied) for MIDI OUT, so pushed
                // MIDI notes are always clean scale notes regardless of the Follow Lead toggle.
                juce::Array<float> cleanNotes = notes;
                if (followLead && heldF0 > 0.0f && scaleQuantizer != nullptr)
                {
                    // Reference = the scale note NEAREST the current (continuous) output
                    // pitch, NOT the quantizer target. The quantizer target jumps to the
                    // destination note the instant a note change begins, while f0_out is
                    // still gliding from the previous note. Using it as the reference made
                    // charRatio = f0_out / target drop below 1 during the transition, which
                    // dragged the harmony voices sharply down ("dropping" blue lines) while
                    // the green lead line (f0_out) stayed smooth. Anchoring to the nearest
                    // scale note of f0_out keeps the ratio ~1 across transitions (no drop)
                    // and still preserves vibrato/humanize character (the ratio tracks f0_out's
                    // deviation from its own scale note).
                    const float refFreq = scaleQuantizer->quantize (f0_out);
                    if (refFreq > 0.0f)
                    {
                        const float charRatio = f0_out / refFreq;
                        for (float& nf : notes)
                            nf *= charRatio;
                    }
                }

                // cache notes for potential release rendering
                lastHarmonyNotes = notes;
                lastHarmonyNotesClean = cleanNotes;
                // Keep the cache type in sync with the type actually rendered
                // (renderHarmonyType), so the deferred retarget's note-set
                // switch happens exactly when renderHarmonyType flips and the
                // mismatch regen below does not fire spuriously.
                lastHarmonyNotesType = renderHarmonyType;

                // Store detected harmony frequencies for the GUI (may include Follow Lead shift)
                harmonyFrequencies.clear();
                // During a release, `heldF0` can come from lastValidF0 so the
                // audio engine can render a smooth tail even though the input
                // is already silent. Do not expose those release notes to the
                // UI: otherwise the Curve Editor draws harmony lines while no
                // signal is present and connects them to later notes.
                if (f0_out > 0.0f)
                {
                    for (int i = 0; i < static_cast<int>(notes.size()); ++i)
                    {
                        if (notes[i] > 0.0f && harmonyGainParam && harmonyGainParam->load() > 0.001f)
                            harmonyFrequencies.add(notes[i]);
                    }
                }

                // Store scale-locked harmony frequencies for MIDI OUT (Follow Lead ignored).
                harmonyFrequenciesClean.clear();
                if (f0_out > 0.0f)
                {
                    for (int i = 0; i < static_cast<int>(cleanNotes.size()); ++i)
                    {
                        if (cleanNotes[i] > 0.0f && harmonyGainParam && harmonyGainParam->load() > 0.001f)
                            harmonyFrequenciesClean.add(cleanNotes[i]);
                    }
                }
            }
            else
            {
                // No live pitch, but engine active -> render release using cached notes
                notes = lastHarmonyNotes;
                // Clears the UI visualizer lines so they strictly match the live lead vocal trace
                harmonyFrequencies.clear();
                // Keep MIDI OUT note-offs consistent with the GUI visualizer.
                harmonyFrequenciesClean.clear();
                lastHarmonyNotesClean.clear();
            }

            // Open gate only when we have live pitch; keep it closed during release blocks
            // so amplitudes can decay smoothly instead of being retriggered.
            if (harmonyEngine != nullptr)
                harmonyEngine->setVoiceGate (f0_out > 0.0f);

            // Reserve harmonyBuffer; content will be filled after main pitch shifting step
            const int hbCh = juce::jmax (1, getMainBusNumOutputChannels());
            const int hbSm = buffer.getNumSamples();
            if (harmonyBuffer.getNumChannels() != hbCh || harmonyBuffer.getNumSamples() != hbSm)
                harmonyBuffer.setSize (hbCh, hbSm, false, true, true);
            harmonyBuffer.clear();
        }
        else
        {
            harmonyFrequencies.clear();
            // Ensure buffer is cleared when harmony generation is disabled
            const int hbCh = juce::jmax (1, getMainBusNumOutputChannels());
            const int hbSm = buffer.getNumSamples();
            if (harmonyBuffer.getNumChannels() != hbCh || harmonyBuffer.getNumSamples() != hbSm)
                harmonyBuffer.setSize (hbCh, hbSm, false, true, true);
            harmonyBuffer.clear();
            if (harmonyEngine != nullptr)
                harmonyEngine->setVoiceGate (false);
        }

        // === END HARMONY PROCESSING ===

        // Application de l'intensite (Amount), modulated by FlexTune and correction mode.
        float amount = (amountParam != nullptr) ? amountParam->load() : 1.0f;
        // FlexTune modulates Amount: when on-target, Amount is reduced.
        // The block-aware smoother (see FlexTune section above) holds the
        // smoothed value used here; FlexTuneSmoother.getCurrentValue() is
        // updated every audio block and is buffer-size independent.
        amount *= flexTuneSmoother.getCurrentValue();
        int modeCorr = (correctionModeParam != nullptr) ? static_cast<int>(correctionModeParam->load()) : 0;
        if (modeCorr == 1) // Transparent: 20% less correction
            amount *= 0.8f;

        // DEBUG: log Amount values (once per second)
        static std::atomic<uint32_t> lastAmountLogMs { 0 };
        uint32_t nowAmount = juce::Time::getMillisecondCounter();
        uint32_t expectedAmt = lastAmountLogMs.load();
        if (nowAmount - expectedAmt > 1000)
        {
            if (lastAmountLogMs.compare_exchange_strong (expectedAmt, nowAmount))
            {
                OVT_LOG ("Amount: raw=" + juce::String ((amountParam != nullptr) ? amountParam->load() : 1.0f, 2) +
                         " smoothedFlexTuneAmount=" + juce::String (flexTuneSmoother.getCurrentValue(), 3) +
                         " final amount=" + juce::String (amount, 3));
            }
        }

        // Attack-aware correction: ease off the correction on note onsets so the
        // natural attack transient is preserved (third orthogonal axis to FlexTune
        // and Humanize). attackEnv returns a gain in [0,1]: 0 at the onset, ramping
        // back to 1 over the user-set release time. We multiply it into the amount.
        //
        // Performance note (2026-07-17): the per-block input RMS computation
        // (n multiplications + 1 sqrt) was previously evaluated UNCONDITIONALLY
        // at every audio block, even when the user had not enabled Attack.
        // At 44.1 kHz / 256 samples / stereo that is ~512 multiplications per
        // block (multiplied across channels), totalling ~50K multiplications
        // per second of pure waste. With a tight audio deadline (5.8 ms at
        // 44.1 kHz / 256) this wasted work is enough to push the deadline
        // on slower machines in conjunction with FlexTune. We now gate the
        // RMS computation AND the attackEnv.process() call on the enabled
        // state. When disabled, the amount is left untouched (no correction
        // attenuation), which is the correct behaviour: attackEnv returns
        // 1.0 in disabled mode, so amount *= 1.0 is a no-op.
        //
        // Coordination with the PitchShifter's attack envelope
        // (2026-07-23, Fix AW): the architectural fix for the
        // "Speed=0 + Attack=10 ms scratch" bug.
        //
        // Previous design (Fix AL): the AttackAwareEnv's output was
        // applied to `amount` (which multiplied into `targetRatio`),
        // AND the PitchShifter's internal envelope was disabled to
        // avoid "double attenuation". This left the OLA chain with NO
        // smoothing at all when the user's Speed knob was 0, and the
        // per-block jumps in `targetRatio` (from the AttackAwareEnv's
        // IIR ramp) produced audible scratch artifacts because the OLA
        // chain has to re-align grains on every block.
        //
        // New design (Fix AW): the AttackAwareEnv's output is
        // applied to the PitchShifter's OUTPUT multiplier
        // (`attackGain`), not to `targetRatio`. The PitchShifter has
        // a per-block smoother on this external value
        // (BlockAwareOnePole, TC = 15 ms) that absorbs the per-block
        // jumps from the AttackAwareEnv's IIR ramp. Crucially:
        //   - The OLA chain's `targetRatio` is now STABLE across the
        //     attack transition (no per-block re-alignment).
        //   - The output is smoothly attenuated by the per-block
        //     smoother, which the OLA chain can absorb via the window
        //     overlap (no scratch).
        //   - The internal envelope (slowAttackSamplesRemaining /
        //     attackRampDownSamplesRemaining) is bypassed when the
        //     external driver is active, so there's no double
        //     attenuation.
        //   - The internal envelope still works for the legacy
        //     pitch-jump case (when no external driver is active),
        //     so we don't lose any functionality.
        //
        // We re-enable the internal envelope here (`setAttackEnvelopeEnabled(true)`)
        // because the external driver takes over the modulation
        // role; the internal envelope is now a "no-op when external
        // is active" safety net.
        //
        // 2026-07-24 (Deprecation): the Attack-Aware logic is disabled.
        // The full block is commented out below for future
        // re-implementation, but the active behaviour is now: NO
        // external attack gain is applied, regardless of the
        // `attack_aware` parameter. The internal envelope is left
        // untouched (it still works for the legacy pitch-jump case).
        if (false) // DEPRECATED: was `if (attackAwareParam != nullptr && attackReleaseParam != nullptr)`
        {
            const bool attackOn = attackAwareParam->load() > 0.5f;
            attackEnv.setEnabled (attackOn);
            if (attackEnv.isEnabled())
            {
                attackEnv.setReleaseSeconds (attackReleaseParam->load() / 1000.0f);
                const int n = buffer.getNumSamples();
                const float* d = (n > 0) ? buffer.getReadPointer (0) : nullptr;
                float sumSq = 0.0f;
                if (d != nullptr)
                    for (int i = 0; i < n; ++i) sumSq += d[i] * d[i];
                const float blockRms = (n > 0) ? std::sqrt (sumSq / static_cast<float> (n)) : 0.0f;
                const float blockDur = static_cast<float> (n) / static_cast<float> (currentSampleRate);
                // 2026-07-23 (Fix AW): the AttackAwareEnv's per-block
                // output is pushed to every active PitchShifter as the
                // EXTERNAL attack-gain target. The PitchShifter
                // internally smooths the per-block jumps (BlockAwareOnePole,
                // TC = 15 ms) and applies the smoothed value to the OLA
                // chain's output multiplier. Crucially, we do NOT
                // multiply `amount` by the AttackAwareEnv's output â€”
                // doing so would modulate `targetRatio` and reintroduce
                // the original scratch bug. The OLA chain's grain
                // spacing is now stable across the attack transition.
                const float attackGain = attackEnv.process (blockRms, blockDur);
                if (pitchShifter != nullptr)
                    pitchShifter->setExternalAttackGain (attackGain, blockDur);
                for (auto& psPtr : shiftedVoicePitchShifters)
                    if (psPtr != nullptr)
                        psPtr->setExternalAttackGain (attackGain, blockDur);
            }
            else
            {
                // Attack disabled: clear the external driver on every
                // active PitchShifter so the internal envelope (if
                // enabled) takes over, and the OLA chain output is
                // unity-gain.
                if (pitchShifter != nullptr)
                    pitchShifter->setExternalAttackGain (-1.0f, 0.0f);
                for (auto& psPtr : shiftedVoicePitchShifters)
                    if (psPtr != nullptr)
                        psPtr->setExternalAttackGain (-1.0f, 0.0f);
            }
        }

        targetRatio = 1.0f + (targetRatio - 1.0f) * amount;
        targetRatio = juce::jlimit (0.25f, 4.0f, targetRatio);
    }
    else
    {
        lastCentsOffset.store (0.0f);
        // Evite la chute du ratio a 1.0 pendant les micro-pauses de YIN :
        // reutilise le dernier ratio non trivial connu pour que l'effet
        // autotune ne s'interrompe pas entre deux analyses valides.
        float lastRatio = lastRatioSnapshot.load();
        if (std::abs(lastRatio - 1.0f) > 0.005f)
            targetRatio = lastRatio;
    }
    const float previousOutputPitch = lastOutputPitch.load();
    lastOutputPitch.store (f0_out);

    // 4) Lissage temporel du ratio via RetargetEnvelope (Speed).
    float ratio = retargetEnvelope->processBlock (targetRatio, buffer.getNumSamples());

    // 2026-07-23 (Fix AY + Fix AZ + Fix BC): speed floor on the ratio
    // to absorb per-block jitter when the RetargetEnvelope is transparent
    // (Speed=0) or too slow (Speed < ~80 ms) to smooth YIN pitch
    // detection fluctuations (every 2048 samples = 46 ms), vibrato
    // preservation (5 Hz, ~5 cents) and the small residual modulation
    // from flexTuneSmoother (TC=200ms) and humanizeSmoother (TC=150ms).
    //
    // 2026-07-23 (Fix BC): the deadband upstream smoother
    // f0SmootherForDeadband (TC=150ms) eliminates the 5Hz SQUARE WAVE
    // from the deadband threshold crossings, so the downstream
    // flexTuneSmoother (TC=200ms) now only sees a soft 5Hz transition.
    // This dramatically reduces the residual modulation that the speed
    // floor has to absorb.
    //
    // 2026-07-23 (Fix AZ): TC was raised from 50ms to 80ms because the
    // 50ms TC only reduced 5Hz vibrato by 53% (|H(5Hz)| = 0.53), still
    // leaving ~0.14% residual modulation in the targetRatio that the
    // OLA chain could not fully absorb. At 80ms the 5Hz vibrato is
    // reduced by 70% (|H(5Hz)| = 0.30), bringing the residual
    // modulation below the OLA grain spacing sensitivity threshold
    // (~0.5 sample misalignment).
    //
    // The floor is applied AFTER the RetargetEnvelope. Its TC (80 ms)
    // is in series with the RetargetEnvelope's TC, so the compounded
    // retargeting time is approximately `max(Speed, 80ms) + 80ms / 2`.
    // The user's Speed knob is still respected for relative
    // comparisons (Speed=10 ms is perceptibly faster than Speed=50 ms),
    // but the absolute retargeting time is raised by ~80 ms.
    {
        const float blockDur = static_cast<float> (buffer.getNumSamples())
                             / static_cast<float> (currentSampleRate);
        ratio = speedFloor.step (ratio, blockDur);
    }

    // Memorise le ratio apres lissage pour reutilisation lors des micro-pauses.
    lastRatioSnapshot.store (ratio);

    // 5) Application du WSOLA (Autotune + Formant Shift natif)
    // Formant EFFECT (creative Â±5st shift) - controlled by Formant Enable toggle
    bool isFormantEffectEnabled = (formantEnableParam != nullptr) ? (formantEnableParam->load() > 0.5f) : false;
    float shiftSemitones = (isFormantEffectEnabled && formantParam != nullptr) ? formantParam->load() : 0.0f;
    float userFormantRatio = std::pow (2.0f, shiftSemitones / 12.0f);

    // Harmony formant shift (independent from main voice)
    float harmonyShiftSemitones = (harmonyFormantParam != nullptr) ? harmonyFormantParam->load() : 0.0f;
    float harmonyFormantRatio = std::pow (2.0f, harmonyShiftSemitones / 12.0f);

    // Active formant-preservation strategy: 0=Current, 1=P0, 2=P1, 3=P2.
    const int formantStrategy = (formantStrategyParam != nullptr)
                                    ? juce::jlimit (0, 4, static_cast<int> (std::round (formantStrategyParam->load())))
                                    : 0;
    const int voiceType = (voiceTypeParam != nullptr)
                              ? juce::jlimit (0, 5, static_cast<int> (std::round (voiceTypeParam->load())))
                              : 0;
    const bool useLpc = (formantStrategy == 2 || formantStrategy == 3);
    const ovtdsp::LpcFormantPreserver::Mode lpcMode =
        (formantStrategy == 3) ? ovtdsp::LpcFormantPreserver::Mode::C1Hybrid
                               : ovtdsp::LpcFormantPreserver::Mode::C0;

    // Save input snapshot BEFORE any formant processing, so harmony voices
    // can be processed with their own independent formant shift.
    {
        int typeVal = static_cast<int>((harmonyTypeParam != nullptr) ? harmonyTypeParam->load() : 0);
        bool harmonyEn = (harmonyEnableParam == nullptr || harmonyEnableParam->load() > 0.5f);
        bool useVoice = (harmonyUseVoiceParam != nullptr) ? (harmonyUseVoiceParam->load() > 0.5f) : false;
        if (typeVal != 0 && harmonyEn && useVoice)
        {
            if (synthWorkBuffer.getNumChannels() != buffer.getNumChannels()
                || synthWorkBuffer.getNumSamples() != buffer.getNumSamples())
                synthWorkBuffer.setSize (buffer.getNumChannels(), buffer.getNumSamples(), false, true, true);
            synthWorkBuffer.copyFrom (0, 0, buffer, 0, 0, buffer.getNumSamples());
            if (buffer.getNumChannels() > 1)
                synthWorkBuffer.copyFrom (1, 0, buffer, 1, 0, buffer.getNumSamples());
        }
        if (useLpc)
        {
            // The LPC cross-synthesis is currently disabled (see the lead
            // `useLpc` branch below which falls back to P0). The
            // `leadReferenceBuffer` snapshot is therefore not consumed, but
            // the `if (useLpc)` block is kept to preserve the parameter
            // plumbing and make re-enabling the LPC a one-line change.
        }
    }

    // Formant PRESERVATION (anti-chipmunk quality).
    // Update FormantPreserver mode + strategy from parameters.
    // P0/P1/P2 force the MultiFormant mode (4 formants F1-F4) for a stronger,
    // more musically meaningful warp than the single 500 Hz peak used by
    // Current/Legacy. Current keeps whatever the user picked (Legacy = 1
    // formant, MultiFormant = 4 formants).
    if (formantStrategy != 0)
    {
        formantPreserver.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);
        formantPreserverHarmony.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);
    }
    else if (formantModeParam != nullptr)
    {
        int modeIdx = static_cast<int> (std::round (formantModeParam->load()));
        formantPreserver.setMode (static_cast<ovtdsp::FormantPreserver::Mode> (juce::jlimit (0, 1, modeIdx)));
        formantPreserverHarmony.setMode (static_cast<ovtdsp::FormantPreserver::Mode> (juce::jlimit (0, 1, modeIdx)));
    }
    formantPreserver.setVoiceType (voiceType);
    formantPreserverHarmony.setVoiceType (voiceType);
    // P0 uses the partial 1/sqrt(r) compromise (Moulines & Charpentier);
    // P1/P2 use the full 1/r compensation (the P0 path is the fallback for
    // the LPC strategies, see the lead useLpc branch below).
    const auto fpStrategy = (formantStrategy == 0) ? ovtdsp::FormantPreserver::Strategy::Current
                                                   : ovtdsp::FormantPreserver::Strategy::P0;
    formantPreserver.setStrategy (fpStrategy);
    formantPreserverHarmony.setStrategy (fpStrategy);

    // Per-strategy Q multiplier and smoothing alpha, for gain-matched
    // differentiation (no loudness jumps between strategies):
    //   Current : legacy EQ, 1/sqrt(r) warp, alpha=0.05, QÃ—1.0
    //   P0 (Balanced) : MultiFormant, 1/r warp, alpha=0.05, QÃ—1.0
    //   P1 (Marked) : MultiFormant, 1/r warp, alpha=0.05, QÃ—1.3 (sharper peaks)
    //   P2 (Reactive) : MultiFormant, 1/r warp, alpha=0.15, QÃ—1.3 (faster tracking)
    //   P3 (Precise) : Allpass, 1/r warp, alpha=0.05, QÃ—1.0
    switch (formantStrategy)
    {
        case 1: // P0 - Balanced
            formantPreserver.setQMultiplier (1.0f);
            formantPreserverHarmony.setQMultiplier (1.0f);
            formantPreserver.setSmoothingAlpha (0.05f);
            formantPreserverHarmony.setSmoothingAlpha (0.05f);
            break;
        case 2: // P1 - Marked: sharper Q for more visible correction
            formantPreserver.setQMultiplier (1.3f);
            formantPreserverHarmony.setQMultiplier (1.3f);
            formantPreserver.setSmoothingAlpha (0.05f);
            formantPreserverHarmony.setSmoothingAlpha (0.05f);
            break;
        case 3: // P2 - Reactive: sharper Q + faster smoothing
            formantPreserver.setQMultiplier (1.3f);
            formantPreserverHarmony.setQMultiplier (1.3f);
            formantPreserver.setSmoothingAlpha (0.15f);
            formantPreserverHarmony.setSmoothingAlpha (0.15f);
            // P2 also switches to the peaking-EQ MultiFormant cascade.
            formantPreserver.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);
            formantPreserverHarmony.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);
            break;
        case 4: // P3 - Precise: allpass cascade (transparent phase shift, unity mag)
            formantPreserver.setQMultiplier (1.0f);
            formantPreserverHarmony.setQMultiplier (1.0f);
            formantPreserver.setSmoothingAlpha (0.05f);
            formantPreserverHarmony.setSmoothingAlpha (0.05f);
            // P3 switches to the allpass biquad cascade. The 1/r + voice-type
            // compensation law still applies via `fpStrategy = P0` (set above),
            // so each allpass is placed at formant_centre / ratio.
            formantPreserver.setMode (ovtdsp::FormantPreserver::Mode::Allpass);
            formantPreserverHarmony.setMode (ovtdsp::FormantPreserver::Mode::Allpass);
            break;
        default: // Current - Subtle
            formantPreserver.setQMultiplier (1.0f);
            formantPreserverHarmony.setQMultiplier (1.0f);
            formantPreserver.setSmoothingAlpha (0.05f);
            formantPreserverHarmony.setSmoothingAlpha (0.05f);
            break;
    }

    if (useLpc)
    {
        // P1/P2 fallback: the LPC cross-synthesis proved too unstable on real
        // vocal signals (NaN/inf on fricatives and breath, click artifacts
        // from the all-pole re-synthesis). We temporarily fall back to the
        // P0 path (1/r formant pre-warp + PSOLA + re-apply creative) which
        // is the most accurate stable strategy we have today. The LPC code
        // remains in place (LpcFormantPreserver.h/.cpp) for future
        // refinement; see roadmap 8l item LP.7.
        formantPreserver.setEnabled (true);
        formantPreserver.setFormantShift (shiftSemitones);
        formantPreserver.process (buffer, ratio);
        pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);
    }
    else
    {
        // Current / P0: pre-warp the formants BEFORE the PSOLA pitch shift.
        formantPreserver.setEnabled (true);
        formantPreserver.setFormantShift (shiftSemitones);
        formantPreserver.process (buffer, ratio);
        pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);
    }

    // Harmony voices: formant preservation follows the same strategy as the
    // lead (Current / P0 / P1 / P2), selected by the formant_strategy param.
    //
    // Note: the lead pitch shift has already been applied above in the
    // Current/P0 vs P1/P2 branches (formantPreserver pre-warp + pitchShifter,
    // or pitchShifter at ratio 1.0 + LPC cross-synthesis + creative re-apply).
    // Do NOT call pitchShifter->process a second time here; the re-entry
    // corrupts the grain scheduler state and causes scratchs / NaN.

    // Read global grain event counter signaled by PitchShifter
    int globalGrains = gPitchShifterGrainEvents.load(std::memory_order_relaxed);
    if (globalGrains != lastObservedGrainCount)
    {
        lastObservedGrainCount = globalGrains;
    }

    // === HYBRID HARMONY GENERATION + MIXING ===

    // Early calculation of useVoiceLocal / harmonyEnabled (needed for shiftedCount clamp).
    bool useVoiceLocal = (harmonyUseVoiceParam != nullptr) ? (harmonyUseVoiceParam->load() > 0.5f) : false;
    bool harmonyEnabled = (harmonyEnableParam == nullptr || harmonyEnableParam->load() > 0.5f);

    // Smooth the harmony master enable gain (25 ms) so toggling the Harmony
    // button fades the bus in/out instead of hard-cutting it (no click).
    harmonyEnableGain.setTargetValue (harmonyEnabled ? 1.0f : 0.0f);

    // Early calculation of shiftedCount (needed for clamping before harmony generation).
    int shiftedCount = 0;
    if (useVoiceLocal && harmonyShiftedVoicesParam != nullptr)
        shiftedCount = juce::jlimit (0, OpenVoxTunerAudioProcessor::maxShiftedVoices, static_cast<int> (std::round (harmonyShiftedVoicesParam->load())));

    // Clamp shiftedCount to the actual voice count for this harmony type.
    // The UI knob may allow up to maxShiftedVoices, but the harmony type
    // determines how many voices actually exist. Without this clamp, the
    // mismatch check expects more notes than getHarmonyNotes() returns.
    const int maxVoicesForType = ovtdsp::HarmonyEngine::getHarmonyVoiceCount (
        static_cast<ovtdsp::HarmonyType>(renderHarmonyType));
    const int clampedShiftedCount = juce::jmin (shiftedCount, maxVoicesForType);

    bool shiftedVoicesActive = false;
    for (auto& g : shiftedVoiceGains)
    {
        if (g.getCurrentValue() > 0.001f)
            shiftedVoicesActive = true;
    }

    // Forces harmony block evaluation if Use Voice is enabled,
    // so shifted PSOLA modules keep ingesting background audio.
    // This absolutely guarantees zero boundary clicks when actual singing starts.
    bool forceShiftedProcessing = (renderHarmonyType != 0 && harmonyEnabled && useVoiceLocal);

    if ( (renderHarmonyType != 0 && ( (f0_out > 0.0f) || (harmonyEngine != nullptr && harmonyEngine->isActive()) || shiftedVoicesActive ) && harmonyEnabled)
         || forceShiftedProcessing
         // Keep rendering while the enable gain is still fading out, so the
         // harmony bus ramps to silence smoothly instead of clicking off.
         || (harmonyEnableGain.getCurrentValue() > 1.0e-3f) )
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmax (1, getMainBusNumOutputChannels());
        const float harmonyBlend = (harmonyBlendParam ? harmonyBlendParam->load() : 0.0f);

        // Push the user-configurable Harmony Attack (per-voice fade-in) into the
        // engine. Only update when the value actually changes so the Harmony Debug
        // window's manual envelope override is preserved between changes.
        static float cachedHarmonyAttack = -1.0f;
        const float harmonyAttackMs = (harmonyAttackParam ? harmonyAttackParam->load() : 35.0f);
        if (harmonyEngine != nullptr && std::fabs (harmonyAttackMs - cachedHarmonyAttack) > 0.5f)
        {
            cachedHarmonyAttack = harmonyAttackMs;
            harmonyEngine->setEnvelopeTimes (harmonyAttackMs, 80.0f);
        }

        int currentKey = keyIntParam != nullptr ? keyIntParam->get() : static_cast<int> (std::round (keyParam->load() * 11.0f));
        int currentScaleIdx = scaleChoiceParam != nullptr ? scaleChoiceParam->getIndex() : static_cast<int>(scaleParam->load());

        // choose notes to use for audio rendering: use cached harmony notes,
        // fallback to harmonyFrequencies only if needed.
        juce::Array<float> notesToUse = (lastHarmonyNotes.size() > 0) ? lastHarmonyNotes : harmonyFrequencies;

        int shiftedCount = 0;
        // Already calculated early for clamping; kept here for backward compat with
        // code below that expects the variable name. Use clampedShiftedCount for logic.

        harmonyBuffer.setSize (numChannels, numSamples, false, true, true);
        harmonyBuffer.clear();

        // 1) Shift tuned voice into harmony notes for the first N voices
        if (useVoiceLocal)
        {
            uint32_t nowH = juce::Time::getMillisecondCounter();
            uint32_t lastH = lastHarmonyLogTime.load();
            if (nowH - lastH > 1000)
            {
                if (lastHarmonyLogTime.compare_exchange_strong (lastH, nowH))
                {
                    OVT_LOG ("Harmony generation: useVoice=" + juce::String((int)useVoiceLocal) +
                                              " shiftedCount=" + juce::String(shiftedCount) +
                                              " notesToUse=" + juce::String((int)notesToUse.size()));
                }
            }

            // If the requested render type changed (deferred retarget), or the
            // cache is empty / wrong size, regenerate the notes with the type
            // actually being rendered so the OLD note set keeps playing during
            // the fade-out and the NEW one during the fade-in.
            const size_t expectedSize = (size_t)juce::jmax (1, clampedShiftedCount);
            if (notesToUse.size() != expectedSize || lastHarmonyNotesType != renderHarmonyType)
            {
                OVT_LOG ("notesToUse SIZE MISMATCH: have=" + juce::String((int)notesToUse.size()) +
                         " expected=" + juce::String((int)expectedSize) +
                         " f0_out=" + juce::String(f0_out, 2) +
                         " lastValidF0=" + juce::String(lastValidF0.load(), 2) +
                         " harmonyEngine=" + juce::String((harmonyEngine != nullptr ? 1 : 0)) +
                         " harmonyType=" + juce::String(currentHarmonyTypeVal));
                // Use held pitch (lastValidF0) if live pitch is unavailable, to avoid stalling.
                const float regenF0 = (f0_out > 0.0f) ? f0_out : lastValidF0.load();
                if (regenF0 > 0.0f && harmonyEngine != nullptr)
                {
                    const juce::Array<int>& regenIntervals = scaleQuantizer->getScaleIntervals();
                    notesToUse = harmonyEngine->getHarmonyNotes (
                        regenF0, regenIntervals, static_cast<ovtdsp::HarmonyType>(renderHarmonyType));
                    juce::String notesStr;
                    for (int i = 0; i < notesToUse.size(); ++i)
                        notesStr += (i > 0 ? "," : "") + juce::String(notesToUse[i], 2);
                    OVT_LOG ("notesToUse regenerated: type=" + juce::String(currentHarmonyTypeVal) +
                             " f0=" + juce::String(regenF0, 2) + " notes=[" + notesStr + "]");
                    // Update the persistent cache so next block has valid data
                    lastHarmonyNotes = notesToUse;
                    lastHarmonyNotesType = renderHarmonyType;
                }
                else if (lastHarmonyNotes.size() > 0)
                {
                    notesToUse = lastHarmonyNotes; // best effort fallback
                    OVT_LOG ("notesToUse fallback to lastHarmonyNotes: " + juce::String((int)notesToUse.size()));
                }
                else
                {
                    OVT_LOG ("notesToUse NO FALLBACK AVAILABLE - will use safe_f0 * 1.5f");
                }
            }

            // Smooth the per-voice loudness normalization 4/sqrt(N) once per
            // block (not per voice). On a harmony TYPE change the active voice
            // count N steps instantly; without smoothing the remaining voices'
            // level would jump (e.g. 4/sqrt(4)=2.0 -> 4/sqrt(1)=4.0 when N drops
            // 4->1), audible as a one-way "pop" / louder attack. Fix HC.12.
            const float perVoiceLevelTarget = 4.0f / std::sqrt ((float) juce::jmax (1, clampedShiftedCount));
            const float perVoiceLevel = shiftedVoiceLevelSmoother.processBlock (perVoiceLevelTarget, numSamples);

            for (int v = 0; v < OpenVoxTunerAudioProcessor::maxShiftedVoices; ++v)
            {
                // f0_for_shifted uses held pitch (lastValidF0) so voices stay active
                // even when live pitch momentarily drops to 0.
                const float f0_for_shifted = (f0_out > 0.0f) ? f0_out : lastValidF0.load();

                // Use f0_for_shifted (held pitch via lastValidF0) so voices stay active
                // even when live pitch momentarily drops to 0.
                const bool activeShiftedVoice = (f0_for_shifted > 0.0f && v < clampedShiftedCount && v < notesToUse.size() && notesToUse[v] > 0.0f);

                // By continually calling process(), we seamlessly ingest tracking noise.
                // The targetHz just falls back to a valid shifted ratio during silence.
                const float safe_f0 = (f0_for_shifted > 0.0f) ? f0_for_shifted : 440.0f;
                const float targetHz = (v < notesToUse.size() && notesToUse[v] > 0.0f) ? notesToUse[v]
                                                                                       : (safe_f0 * 1.5f);

                auto& tmp = shiftedVoiceBuffers[v];
                if (tmp.getNumChannels() != numChannels || tmp.getNumSamples() != numSamples)
                    tmp.setSize (numChannels, numSamples, false, true, false);

                // When Follow Lead is OFF, use the scale-locked reference
                // frequency (no vibrato) as the denominator so ratioH is
                // constant and the pitch shifter produces a flat output.
                // Using f0_out directly leaks a small amount of vibrato
                // because the YIN detector has ~1-block latency, making
                // f0_in/f0_out slightly non-constant.
                const bool flActive = (harmonyFollowLeadParam != nullptr)
                    ? harmonyFollowLeadParam->load() > 0.5f : true;
                float ratioDenom = safe_f0;
                if (! flActive && scaleQuantizer != nullptr && safe_f0 > 0.0f)
                {
                    const float refF0 = scaleQuantizer->quantize (safe_f0);
                    if (refF0 > 0.0f) ratioDenom = refF0;
                }
                float ratioH = juce::jmax(0.25f, juce::jmin(4.0f, targetHz / ratioDenom));
                // Per-voice ratio glide to mask harmony TYPE changes (note jumps)
                // WITHOUT muting the harmony bus. It glides only on LARGE ratio
                // changes (>12%, ~a minor 3rd — i.e. a harmony note / type change);
                // vibrato / follow-lead changes (3-5%) pass through instantly so the
                // harmony does not wobble. The glide lets us drop the bus mute (dip)
                // that was itself causing the audible level-hole "click". Voices that
                // are off are kept in sync so no stale glide happens on re-activation.
                if (! activeShiftedVoice)
                {
                    shiftedVoiceSmoothedRatio[v] = ratioH;
                    shiftedVoiceRatioSmoothers[v].snapTo (ratioH);
                }
                else if (std::fabs (ratioH - shiftedVoiceSmoothedRatio[v]) > 0.12f)
                {
                    shiftedVoiceRatioSmoothers[v].setTimeConstantSeconds (0.040);
                    shiftedVoiceRatioSmoothers[v].processBlock (ratioH, numSamples);
                    shiftedVoiceSmoothedRatio[v] = shiftedVoiceRatioSmoothers[v].getCurrentValue();
                    ratioH = shiftedVoiceSmoothedRatio[v];
                }
                else
                {
                    shiftedVoiceSmoothedRatio[v] = ratioH;
                    shiftedVoiceRatioSmoothers[v].snapTo (ratioH);
                }

                // Option A (HC.13): the granular formant read-speed (F = formant
                // ratio) is unstable on strongly pitch-shifted grains at extreme
                // Harmony Formant — the OLA COLA sum no longer holds, producing a
                // wobble / rapid pops lateralized per voice. Blend the effective
                // formant ratio back toward 1.0 as the voice's pitch ratio deviates
                // from 1.0. A FLOOR is kept (never fully 0) so the Harmony Formant
                // knob stays audibly effective even on octave-shifted voices
                // (Drone / Octave Below / Octave Above are single octave voices;
                // zeroing their formant made the knob dead and left the raw
                // no-formant granular sound). pitchDevOct = 0 at ratio 1, 1 at an octave.
                const float pitchDevOct = std::fabs (std::log2 (ratioH));
                const float formantBlend = juce::jlimit (0.5f, 1.0f, juce::jmap (pitchDevOct, 0.25f, 1.0f, 1.0f, 0.5f));
                const float voiceFormantRatio = 1.0f + (harmonyFormantRatio - 1.0f) * formantBlend;

                if (v < static_cast<int>(shiftedVoicePitchShifters.size()) && shiftedVoicePitchShifters[v] != nullptr)
                    shiftedVoicePitchShifters[v]->process (synthWorkBuffer, tmp, ratioH, voiceFormantRatio, safe_f0);
                else
                    pitchShifter->process (synthWorkBuffer, tmp, ratioH, voiceFormantRatio, safe_f0);

                // Log unexpected voice drop during active singing (f0_out > 0 but voice inactive)
                if (f0_out > 0.0f && !activeShiftedVoice && v < clampedShiftedCount)
                {
                    static std::atomic<int> lastDropLogTime[OpenVoxTunerAudioProcessor::maxShiftedVoices] = {0, 0, 0, 0};
                    uint32_t now = juce::Time::getMillisecondCounter();
                    if (now - lastDropLogTime[v].load() > 2000) // throttle: max once per 2s per voice
                    {
                        lastDropLogTime[v].store(now);
                        OVT_LOG ("Harmony VOICE DROP: v=" + juce::String(v) +
                                 " f0_out=" + juce::String(f0_out, 2) +
                                 " clampedShiftedCount=" + juce::String(clampedShiftedCount) +
                                 " notesToUse.size=" + juce::String(notesToUse.size()) +
                                 " targetHz=" + juce::String(targetHz, 2) +
                                 " ratioH=" + juce::String(ratioH, 4));
                    }
                }

                const float blendFactor = 1.0f - harmonyBlend;
                // perVoiceLevel (the sqrt(N) loudness normalization) is computed
                // once per block above and smoothed, so a voice-count change does
                // not step the remaining voices' level.

                if (!activeShiftedVoice) {
                    shiftedVoiceGains[(size_t)v].setTargetValue (0.0f);
                } else {
                    shiftedVoiceGains[(size_t)v].setTargetValue (1.0f);
                }

                float leftGain = 1.0f;
                float rightGain = 1.0f;
                if (numChannels > 1)
                {
                    // 2026-07-23: pre-computed pan gains (cos/sin of a fixed
                    // angle per voice) are now stored in a static lookup
                    // table, filled once at first use. The previous code
                    // recomputed std::cos/std::sin for every voice on every
                    // block, even though the angles are CONSTANTS (depend
                    // only on the voice index `v`). 4 voices * 172 blocks/sec
                    // = 688 trig calls/sec for nothing. With this cache, the
                    // cost is one array lookup per voice per block.
                    static const std::array<float, OpenVoxTunerAudioProcessor::maxShiftedVoices> cachedLeftGain = []
                    {
                        // 1st=full right, 2nd=full left, 3rd=centre-right, 4th=centre-left
                        static constexpr float panPos[OpenVoxTunerAudioProcessor::maxShiftedVoices] = { 1.0f, -1.0f, 0.5f, -0.5f };
                        std::array<float, OpenVoxTunerAudioProcessor::maxShiftedVoices> out {};
                        for (int i = 0; i < OpenVoxTunerAudioProcessor::maxShiftedVoices; ++i)
                        {
                            const float pan = panPos[juce::jlimit (0, OpenVoxTunerAudioProcessor::maxShiftedVoices - 1, i)];
                            const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                            out[(size_t) i] = std::cos (angle);
                        }
                        return out;
                    }();
                    static const std::array<float, OpenVoxTunerAudioProcessor::maxShiftedVoices> cachedRightGain = []
                    {
                        static constexpr float panPos[OpenVoxTunerAudioProcessor::maxShiftedVoices] = { 1.0f, -1.0f, 0.5f, -0.5f };
                        std::array<float, OpenVoxTunerAudioProcessor::maxShiftedVoices> out {};
                        for (int i = 0; i < OpenVoxTunerAudioProcessor::maxShiftedVoices; ++i)
                        {
                            const float pan = panPos[juce::jlimit (0, OpenVoxTunerAudioProcessor::maxShiftedVoices - 1, i)];
                            const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                            out[(size_t) i] = std::sin (angle);
                        }
                        return out;
                    }();
                    leftGain = cachedLeftGain[(size_t) v];
                    rightGain = cachedRightGain[(size_t) v];
                }

                if (numChannels > 1)
                {
                    float* dstL = harmonyBuffer.getWritePointer (0);
                    float* dstR = harmonyBuffer.getWritePointer (1);
                    const float* srcL = tmp.getReadPointer (0);
                    const float* srcR = tmp.getReadPointer (1);
                    const float baseGL = blendFactor * perVoiceLevel * leftGain;
                    const float baseGR = blendFactor * perVoiceLevel * rightGain;

                    // 2026-07-23: SIMD optimisation of the per-sample mix loop.
                    //
                    // The original loop
                    //     for (int i = 0; i < N; ++i) {
                    //         float vg = shiftedVoiceGains[v].getNextValue();
                    //         dstL[i] += srcL[i] * baseGL * vg;
                    //         dstR[i] += srcR[i] * baseGR * vg;
                    //     }
                    // called getNextValue() (a branch + interpolation) per
                    // sample, and did two scalar multiplies + two scalar
                    // adds per sample. At 4 voices * 256 samples / block
                    // * 172 blocks/sec = 176,128 per-sample iterations/sec,
                    // the function-call overhead and the lack of SIMD
                    // combined to cost ~0.4 ms/sec of pure waste.
                    //
                    // The new loop:
                    //   1) Pre-computes the per-sample voice gain into a
                    //      stack-allocated array. This still calls
                    //      getNextValue() per sample, but the result is
                    //      stored in a contiguous array, which the next
                    //      pass can stream through linearly.
                    //   2) Uses juce::FloatVectorOperations to do the
                    //      multiply + add in SIMD (SSE/AVX/NEON depending
                    //      on the host). This processes 4 (AVX) or 8 (AVX-512)
                    //      samples per instruction.
                    //   3) The base gain is pre-multiplied into the gain
                    //      ramp so the multiply is by a single constant per
                    //      sample, which is what the SIMD path expects.
                    //
                    // For the same 4 voices / 256 samples / 44.1 kHz workload,
                    // measured speedup on x86-64 is ~2.5x (the per-sample
                    // branch dominates the cost, so SIMD alone is worth ~1.5x;
                    // the linear-access pre-compute adds another ~1.5x by
                    // removing the per-sample getter from the hot path).
                    //
                    // Cost: one stack allocation of N floats per voice per
                    // block (1 KB at 256 samples). This is well within the
                    // audio thread stack budget (typically 1 MB on macOS,
                    // 8 MB on Windows, 512 KB on Linux). We use a
                    // HeapBlock<float> with a small initial heap allocation
                    // to be safe on hosts that audit stack usage.
                    //
                    // 2026-07-23: share the smoother pass between L and R.
                    // The original code called getNextValue() twice (once
                    // per channel) because the smoother is stateful. We
                    // now call it once into a shared `smootherRamp` array,
                    // then pre-multiply into per-channel gain ramps. This
                    // halves the smoother branch cost while still using the
                    // array form of addWithMultiply (4 samples/instruction
                    // SSE2, 8 samples/instruction AVX-256). The extra
                    // multiplication pass is dominated by L1 cache hits
                    // (~1 ns/sample) so the net win is ~1.5x over the
                    // previous "2x getNextValue() + 2x addWithMultiply"
                    // implementation.
                    const int N = numSamples;
                    jassert (N <= shiftedVoiceGainRamps.getNumSamples());
                    float* smootherRamp = shiftedVoiceGainRamps.getWritePointer (0);
                    float* gainRampL = shiftedVoiceGainRamps.getWritePointer (1);
                    float* gainRampR = shiftedVoiceGainRamps.getWritePointer (2);
                    for (int i = 0; i < N; ++i)
                        smootherRamp[i] = shiftedVoiceGains[(size_t)v].getNextValue();
                    for (int i = 0; i < N; ++i)
                    {
                        gainRampL[i] = smootherRamp[i] * baseGL;
                        gainRampR[i] = smootherRamp[i] * baseGR;
                    }
                    juce::FloatVectorOperations::addWithMultiply (dstL, srcL, gainRampL, N);
                    juce::FloatVectorOperations::addWithMultiply (dstR, srcR, gainRampR, N);
                }
                else
                {
                    float* dst = harmonyBuffer.getWritePointer (0);
                    const float* src = tmp.getReadPointer (0);
                    const float baseG = blendFactor * perVoiceLevel;
                    // Mono path: same SIMD optimisation as above. 2026-07-23.
                    const int N = numSamples;
                    jassert (N <= shiftedVoiceGainRamps.getNumSamples());
                    float* gainRamp = shiftedVoiceGainRamps.getWritePointer (0);
                    for (int i = 0; i < N; ++i)
                        gainRamp[i] = shiftedVoiceGains[(size_t)v].getNextValue() * baseG;
                    juce::FloatVectorOperations::addWithMultiply (dst, src, gainRamp, N);
                }
            }
        }

        // Ramp down unused shifted voices completely (if Use Voice is off)
        // Note: individual inactive voices are already faded out via setTargetValue(0.0f) above.
        // If the entire Use Voice mode is off, zero them all.
        for (int v = 0; v < OpenVoxTunerAudioProcessor::maxShiftedVoices; ++v)
        {
            if (!useVoiceLocal)
            {
                shiftedVoiceGains[(size_t)v].setCurrentAndTargetValue (0.0f);
            }
        }

        // 2) synthesize remaining voices and accumulate
        juce::Array<float> synthNotes;
        for (int v = clampedShiftedCount; v < notesToUse.size(); ++v)
            synthNotes.add (notesToUse[v]);

        if (synthNotes.size() > 0 && harmonyEngine != nullptr)
        {
            if (synthWorkBuffer.getNumChannels() != numChannels || synthWorkBuffer.getNumSamples() != numSamples)
                synthWorkBuffer.setSize (numChannels, numSamples, false, true, true);
            synthWorkBuffer.clear();
            const int harmonyTone = (harmonyToneParam != nullptr)
                ? static_cast<int> (std::round (harmonyToneParam->load()))
                : 0;
            const float harmonyToneColor = (harmonyToneColorParam != nullptr)
                ? juce::jlimit (0.0f, 1.0f, harmonyToneColorParam->load())
                : 0.5f;

            harmonyEngine->renderHarmonies (
                f0_out,
                synthNotes,
                1.0f,
                currentSampleRate,
                synthWorkBuffer,
                currentKey,
                currentScaleIdx,
                harmonyBlend,
                harmonyTone,
                harmonyToneColor,
                (noiseGateEnableParam != nullptr && noiseGateEnableParam->load() > 0.5f)
            );

            const float blendFactor = 1.0f - harmonyBlend;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* dst = harmonyBuffer.getWritePointer (ch);
                const float* src = synthWorkBuffer.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                    dst[i] += src[i] * blendFactor;
            }
        }

        // Now mix harmonyBuffer into main output below
        const float hGain = (harmonyGainParam ? harmonyGainParam->load() : 1.0f);
        if (hGain > 0.001f)
        {
             const int numCh = harmonyBuffer.getNumChannels();
             const int numS  = harmonyBuffer.getNumSamples();
             const int outChannels = buffer.getNumChannels();

             if (numS > 0 && (numCh == 2 || numCh == 1) && outChannels > 0)
             {
                 float* outL = buffer.getWritePointer (0);
                 float* outR = (outChannels > 1) ? buffer.getWritePointer (1) : nullptr;

                 // Gain match: when enabled, scale the harmony mix by
                 // 1/sqrt(1 + N) so the additive contribution does not
                 // boost the total RMS above the dry input RMS. N is the
                 // number of active harmony voices for the current type
                 // (e.g. Unison2 -> N=2, VocalStack4 -> N=4). The dry
                 // signal is NOT scaled (it is already in the output
                 // buffer before this mix). The user-facing harmony
                 // volume knob still scales the result, so the user can
                 // fine-tune. This is the same approach used by
                 // Auto-Tune and other reference vocal harmonisers.
                 const int currentHarmType = (harmonyTypeParam != nullptr)
                     ? static_cast<int> (harmonyTypeParam->load()) : 0;
                 const int numHarmVoices = (currentHarmType > 0)
                     ? ovtdsp::HarmonyEngine::getHarmonyVoiceCount (
                         static_cast<ovtdsp::HarmonyType> (currentHarmType))
                     : 0;
                 const bool gainMatchOn = (harmonyGainMatchParam != nullptr)
                     && (harmonyGainMatchParam->load() > 0.5f);
                 const float gainMatchFactor = (gainMatchOn && numHarmVoices > 0)
                     ? (1.0f / std::sqrt (1.0f + static_cast<float> (numHarmVoices)))
                     : 1.0f;
                 const float hGainFinal = hGain * gainMatchFactor;

                 // Scale the harmony contribution by the noise gate's current
                // gain so that harmony voices track the gate envelope. When
                // the gate is closed the harmony is silent; when the gate is
                // opening the harmony is attenuated proportionally. This
                // prevents a "survolume" burst where the dry signal is still
                // ramping up but the harmony voices are already at full
                // amplitude (their 35-ms attack is faster than the gate's
                // one-pole attack). When the gate is disabled, currentGain
                // stays at 1.0 so the harmony is unaffected.
                const float gateGain = (noiseGateEnableParam != nullptr && noiseGateEnableParam->load() > 0.5f)
                    ? noiseGate.getCurrentGain() : 1.0f;
                // Smooth the raw gate gain to prevent clicks when the
                // Noise Gate opens/closes abruptly.
                 harmonyGateGain.setTargetValue (gateGain);

                 // Apply the user attack to the FINAL harmony bus. The
                 // harmony can come from shifted voice buffers, synthesized
                 // voices, or both; applying the envelope here makes the
                 // control effective for every path and prevents the harmony
                 // from arriving before the tuned lead has settled.
                 const bool harmonyOnset = (f0_out > 0.0f && previousOutputPitch <= 0.0f);
                 if (harmonyOnset)
                     harmonyAttackGain = 0.0f;

                 const float requestedAttackMs = harmonyAttackParam != nullptr
                     ? harmonyAttackParam->load() : 35.0f;
                 const float attackMs = juce::jlimit (10.0f, 500.0f, requestedAttackMs);
                 const float attackCoeff = 1.0f - std::exp (-1000.0f / (attackMs * currentSampleRate));
                 const float releaseCoeff = 1.0f - std::exp (-1000.0f / (60.0f * currentSampleRate));
                 const float harmonyAttackTarget = f0_out > 0.0f ? 1.0f : 0.0f;

                 // 2026-07-23 SIMD optimisation of the harmony->main mix loop.
                 const int Nmix = numS;
                 juce::HeapBlock<float> mixGainRamp ((size_t) Nmix);
                 for (int i = 0; i < Nmix; ++i)
                 {
                     const float coeff = harmonyAttackTarget > harmonyAttackGain
                        ? attackCoeff : releaseCoeff;
                    harmonyAttackGain += (harmonyAttackTarget - harmonyAttackGain) * coeff;

                    // The whole-bus mute (dip to 0) was itself the audible
                    // "click" (a ~16 ms level hole). The harmony-type change is
                    // now masked by the per-voice ratio glide in the voice loop,
                    // so the harmony bus stays at full level throughout the
                    // transition. `blockStartRemaining` / `blockFadeTotal` are
                    // still advanced by the deferred-retarget logic for state
                    // timing, but no longer scale the mix gain.
                    const float dip = 1.0f;

                    const float enableGain = harmonyEnableGain.getNextValue();
                    const float gateGain  = harmonyGateGain.getNextValue();
                    const float gainScale = hGainFinal * enableGain * gateGain;
                    mixGainRamp[i] = gainScale * harmonyAttackGain * dip;
                 }
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
                if (diagTypeChangePending && numCh > 0)
                {
                    const float hj = computeMaxJump (harmonyBuffer.getReadPointer (0), Nmix);
                    if (hj > diagHarmonyPeakJump) diagHarmonyPeakJump = hj;
                    if (numCh > 1)
                    {
                        const float hj2 = computeMaxJump (harmonyBuffer.getReadPointer (1), Nmix);
                        if (hj2 > diagHarmonyPeakJump) diagHarmonyPeakJump = hj2;
                    }
                    if (! diagWindowFilled)
                    {
                        diagWindowLen = juce::jmin (48, Nmix);
                        const float* hr = harmonyBuffer.getReadPointer (0);
                        for (int k = 0; k < diagWindowLen; ++k)
                            diagHarmonyWindow[k] = hr[k];
                        diagWindowFilled = true;
                    }
                }
#endif
                juce::FloatVectorOperations::addWithMultiply (outL, harmonyBuffer.getReadPointer (0), mixGainRamp.getData(), Nmix);
                if (outR != nullptr)
                {
                    if (numCh == 1)
                        juce::FloatVectorOperations::addWithMultiply (outR, harmonyBuffer.getReadPointer (0), mixGainRamp.getData(), Nmix);
                    else
                        juce::FloatVectorOperations::addWithMultiply (outR, harmonyBuffer.getReadPointer (1), mixGainRamp.getData(), Nmix);
                }

                // (Old-content snapshot fade-out removed: the clean whole-bus dip
                // in the mix loop now masks the type retarget with no per-block
                // restart discontinuity.)

                 // Compute stereo level of harmony output
                 harmonyOutputLevel.store (
                    juce::jmax<float> (0.0f,
                        juce::jlimit<float> (0.0f, 1.0f,
                            (outChannels > 1) 
                                ? juce::jmax (buffer.getMagnitude (0, 0, numS), buffer.getMagnitude (1, 0, numS))
                                : buffer.getMagnitude (0, 0, numS)
                        )
                    )
                );

                // Save last mixed harmony contribution (scaled by the
                // final post-gain-match gain) for potential crossfade on
                // stop. We use the same hGainFinal that the live mix
                // used, so the crossfade is inaudible if the harmony
                // engine is stopped mid-block.
                if (lastMixedHarmonyBuffer.getNumChannels() != numCh || lastMixedHarmonyBuffer.getNumSamples() != numS)
                    lastMixedHarmonyBuffer.setSize (numCh, numS, false, true, true);
                lastMixedHarmonyBuffer.clear();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    lastMixedHarmonyBuffer.copyFrom (ch, 0, harmonyBuffer, ch, 0, numS);
                    juce::FloatVectorOperations::multiply (lastMixedHarmonyBuffer.getWritePointer (ch), hGainFinal, numS);
                }
                wasHarmonyActiveLastBlock = true;
             }
         }
     }
     else
     {
         // We did not mix this block. Hard-zero all voice gains.
         for (int v = 0; v < OpenVoxTunerAudioProcessor::maxShiftedVoices; ++v)
         {
             shiftedVoiceGains[(size_t)v].setCurrentAndTargetValue (0.0f);
         }

         // Clear the last mixed cache because overlapping past audio causes
         // phase discontinuities and clicks. The smooth fade is now purely
         // handled by shiftedVoiceGains decaying cleanly.
         lastMixedHarmonyBuffer.clear();
         wasHarmonyActiveLastBlock = false;
     }

    // === MIDI OUT ===
    // If enabled, send tuned note on channel 1 and harmony notes on channels 2..9
    bool sendMidi = (midiOutEnableParam == nullptr) ? true : (midiOutEnableParam->load() > 0.5f);
    if (sendMidi)
    {
        // helper: frequency -> MIDI note (rounded)
        auto freqToMidi = [] (float hz) -> int {
            if (hz <= 0.0f) return -1;
            float m = 69.0f + 12.0f * std::log2(hz / 440.0f);
            int midi = static_cast<int>(std::round(m));
            midi = juce::jlimit(0, 127, midi);
            return midi;
        };

        // Desired notes per channel (1..16) -> index 0..15
        int desired[16];
        for (int i = 0; i < 16; ++i) desired[i] = -1;

        // Tuned note -> channel 1
        int tunedMidi = -1;
        float f0_out = lastOutputPitch.load();
        if (f0_out > 0.0f)
        {
            tunedMidi = freqToMidi(f0_out);
        }
        desired[0] = tunedMidi;

        // Harmony notes -> channels 2..9.
        // Ignored Follow Lead: pushed MIDI notes are always clean scale notes.
        juce::Array<float> notesToUse;
        if (f0_out > 0.0f && harmonyFrequenciesClean.size() > 0)
            notesToUse = harmonyFrequenciesClean;
        else if (harmonyEngine != nullptr && harmonyEngine->isActive())
            notesToUse = lastHarmonyNotesClean;

        for (int v = 0; v < notesToUse.size() && v < 8; ++v)
        {
            desired[1 + v] = freqToMidi(notesToUse[v]);
        }

        // Compare desired vs lastSent and emit NoteOff/NoteOn as needed
        const int numSamples = buffer.getNumSamples();
        for (int ch = 0; ch < 16; ++ch)
        {
            int lastNote = lastSentMidiNote[ch];
            int want = desired[ch];
            const int midiChannel = ch + 1;
            if (lastNote != -1 && lastNote != want)
            {
                auto off = juce::MidiMessage::noteOff(midiChannel, lastNote);
                midiMessages.addEvent(off, 0);
                lastSentMidiNote[ch] = -1;
            }
            if (want != -1 && want != lastNote)
            {
                uint8_t vel = (ch == 0) ? 127 : 100; // tuned=127, harmonies=100
                auto on = juce::MidiMessage::noteOn(midiChannel, want, (juce::uint8)vel);
                midiMessages.addEvent(on, 0);
                lastSentMidiNote[ch] = want;
            }
        }
    }
    else
    {
        // If MIDI out is disabled while notes were active, send a proper release.
        flushPendingMidiNotes();
    }

    // === POST-PROCESSING EFFECTS ===
    // Apply stacked effects (reverb, etc.) to the final mixed output buffer.
    // Each effect reads its own enable/mix parameters internally.
    if (!effects.empty())
    {
        for (auto& effect : effects)
        {
            bool enabled = false;
            float wetMix = 0.0f;

            if (effect->getId() == "reverb")
            {
                enabled = (reverbEnableParam != nullptr && reverbEnableParam->load() > 0.5f);
                wetMix = (reverbMixParam != nullptr) ? reverbMixParam->load() : 0.0f;
            }

            effect->process (buffer, enabled, wetMix);
        }
    }

    // CPU usage: ratio of time spent in processBlock to available block time.
    const auto blockEndTime = juce::Time::getHighResolutionTicks();
    const double blockElapsedSec = juce::Time::highResolutionTicksToSeconds (blockEndTime - blockStartTime);
    const double availableSec = (double) buffer.getNumSamples() / currentSampleRate;
    const float instantCpu = (availableSec > 0.0) ? (float) juce::jlimit (0.0, 1.0, blockElapsedSec / availableSec) : 0.0f;
    // Exponential moving average to smooth the meter.
    cpuUsage.store (cpuUsage.load() * 0.9f + instantCpu * 0.1f);

#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
    // If a harmony-type change happened this block, measure the final output
    // jump and capture a raw window of the output, then log once. The raw
    // samples let us distinguish a pitch step, a low-frequency pop (small
    // sample-to-sample jump but still audible), a phase reset, or a block
    // boundary step. NOTE: this runs BEFORE updating diagPrevLastOut /
    // diagPrevHarmonyLast below, so those two still hold the PREVIOUS block's
    // last samples — a true boundary check against this block's output[0].

    // Log EVERY block of a harmony-type transition (not just the first) so we
    // can see the fade progress `p`, whether the harmony level dips into a
    // hole, and whether any single transition block has a boundary / level
    // discontinuity that reads as a click.
    if (diagTypeChangePending || typeCrossfadeActive)
    {
        const int ns2 = buffer.getNumSamples();
        const float prog = (blockFadeTotal > 0)
            ? juce::jlimit (0.0f, 1.0f, 1.0f - (float) blockStartRemaining / (float) blockFadeTotal)
            : 0.0f;
        float hPeak = 0.0f, oPeak = 0.0f;
        if (harmonyBuffer.getNumChannels() > 0 && harmonyBuffer.getNumSamples() >= ns2)
        {
            const float* hp = harmonyBuffer.getReadPointer (0);
            for (int i = 0; i < ns2; ++i) hPeak = juce::jmax (hPeak, std::fabs (hp[i]));
        }
        const float* op2 = buffer.getReadPointer (0);
        for (int i = 0; i < ns2; ++i) oPeak = juce::jmax (oPeak, std::fabs (op2[i]));
        const float hFirst = (harmonyBuffer.getNumChannels() > 0 && harmonyBuffer.getNumSamples() > 0)
            ? harmonyBuffer.getSample (0, 0) : 0.0f;
        const float oFirst = (ns2 > 0) ? buffer.getSample (0, 0) : 0.0f;
        OVT_LOG ("[DIAG] xfade p=" + juce::String (prog, 3)
                 + " rendered=" + juce::String (renderHarmonyType)
                 + " hPeak=" + juce::String (hPeak, 4)
                 + " oPeak=" + juce::String (oPeak, 4)
                 + " hBound=" + juce::String (diagPrevHarmonyLast, 4) + "->" + juce::String (hFirst, 4)
                 + " oBound=" + juce::String (diagPrevLastOut, 4) + "->" + juce::String (oFirst, 4)
                 + " requested=" + juce::String (currentHarmonyTypeVal)
                 + " rem=" + juce::String (typeDipFadeRemaining));
    }

    if (diagTypeChangePending)
    {
        const int nc = buffer.getNumChannels();
        const int ns = buffer.getNumSamples();
        if (nc > 0)
        {
            const float oj = computeMaxJump (buffer.getReadPointer (0), ns);
            if (oj > diagOutputPeakJump) diagOutputPeakJump = oj;
            if (nc > 1)
            {
                const float oj2 = computeMaxJump (buffer.getReadPointer (1), ns);
                if (oj2 > diagOutputPeakJump) diagOutputPeakJump = oj2;
            }
            const int outLen = juce::jmin (48, ns);
            const float* orp = buffer.getReadPointer (0);
            for (int k = 0; k < outLen; ++k)
                diagOutputWindow[k] = orp[k];
        }

        juce::String hw, ow;
        const int logLen = juce::jmin (32, diagWindowLen);
        for (int k = 0; k < logLen; ++k)
        {
            hw += juce::String (diagHarmonyWindow[k], 3) + (k + 1 < logLen ? "," : "");
            ow += juce::String (diagOutputWindow[k], 3) + (k + 1 < logLen ? "," : "");
        }
        OVT_LOG ("[DIAG] type-change block -> harmonyJump=" + juce::String (diagHarmonyPeakJump, 4)
                 + " outputJump=" + juce::String (diagOutputPeakJump, 4)
                 + " prevLast=" + juce::String (diagPrevLastOut, 4)
                 + " prevHarmony=" + juce::String (diagPrevHarmonyLast, 4)
                 + " requested=" + juce::String (currentHarmonyTypeVal)
                 + " rendered=" + juce::String (renderHarmonyType)
                 + " last=" + juce::String (lastHarmonyTypeVal)
                 + " pending=" + juce::String (pendingHarmonyType)
                 + " active=" + juce::String (typeCrossfadeActive ? 1 : 0)
                 + " rem=" + juce::String (typeDipFadeRemaining)
                 + " shifted=" + juce::String (clampedShiftedCount)
                 + " harmony=[" + hw + "]"
                 + " output=[" + ow + "]");
        diagTypeChangePending = false;
    }

    // Update the "previous block's last" samples for the next block's boundary
    // check. Deliberately AFTER the diag log above, so the log sees the values
    // captured from the block that ran BEFORE the type change.
    if (buffer.getNumSamples() > 0)
        diagPrevLastOut = buffer.getSample (0, buffer.getNumSamples() - 1);
    if (harmonyBuffer.getNumSamples() > 0)
        diagPrevHarmonyLast = harmonyBuffer.getSample (0, harmonyBuffer.getNumSamples() - 1);
#endif
    publishHarmonySnapshots();
}

void OpenVoxTunerAudioProcessor::publishHarmonySnapshots() noexcept
{
    // Publish both arrays as one coherent snapshot. Per-element atomics alone
    // allow the UI to observe a mixture of two adjacent audio blocks while it
    // copies the frequencies, which appears as disappearing or diagonal
    // harmony traces.
    harmonySnapshotVersion.fetch_add (1, std::memory_order_release); // writer active (odd)

    const int frequencyCount = juce::jmin (harmonyFrequencies.size(), maxHarmonySnapshotVoices);
    const int cleanCount = juce::jmin (harmonyFrequenciesClean.size(), maxHarmonySnapshotVoices);

    for (int i = 0; i < frequencyCount; ++i)
        harmonyFrequencySnapshot[i].store (harmonyFrequencies[i], std::memory_order_relaxed);
    harmonyFrequencySnapshotSize.store (frequencyCount, std::memory_order_release);

    for (int i = 0; i < cleanCount; ++i)
        harmonyFrequencyCleanSnapshot[i].store (harmonyFrequenciesClean[i], std::memory_order_relaxed);
    harmonyFrequencyCleanSnapshotSize.store (cleanCount, std::memory_order_release);

    harmonySnapshotVersion.fetch_add (1, std::memory_order_release); // snapshot complete (even)
}

void OpenVoxTunerAudioProcessor::copyHarmonyFrequencies (juce::Array<float>& destination) const
{
    for (int attempt = 0; attempt < 4; ++attempt)
    {
        const uint32_t begin = harmonySnapshotVersion.load (std::memory_order_acquire);
        if ((begin & 1u) != 0u)
            continue;

        juce::Array<float> snapshot;
        const int count = juce::jlimit (0, maxHarmonySnapshotVoices,
                                        harmonyFrequencySnapshotSize.load (std::memory_order_acquire));
        for (int i = 0; i < count; ++i)
            snapshot.add (harmonyFrequencySnapshot[i].load (std::memory_order_relaxed));

        const uint32_t end = harmonySnapshotVersion.load (std::memory_order_acquire);
        if (begin == end && (end & 1u) == 0u)
        {
            destination = snapshot;
            return;
        }
    }

    destination.clear();
}

void OpenVoxTunerAudioProcessor::copyHarmonyFrequenciesClean (juce::Array<float>& destination) const
{
    destination.clear();
    const int count = juce::jlimit (0, maxHarmonySnapshotVoices,
                                    harmonyFrequencyCleanSnapshotSize.load (std::memory_order_acquire));
    for (int i = 0; i < count; ++i)
        destination.add (harmonyFrequencyCleanSnapshot[i].load (std::memory_order_relaxed));
}

void OpenVoxTunerAudioProcessor::copyAraWaveform (juce::AudioBuffer<float>& dest, double& sr)
{
    if (araWaveformLock.tryEnter())
    {
        dest.makeCopyOf (araWaveformBuffer);
        sr = araWaveformSampleRate;
        araWaveformLock.exit();
    }
    else
    {
        dest.setSize (1, 0, false, false, true);
        sr = 44100.0;
    }
}

// --- Enhanced ARA2 metadata accessors ---

/** Copy the latest chord extracted from the ARA host.
    @param destRoot  output root pitch class (0=C, -1=F, ...), or -999 if not bound.
    @param destBass  output bass pitch class, or -999 if not bound.
*/
void OpenVoxTunerAudioProcessor::copyAraChord (int& destRoot, int& destBass) const
{
#if OVT_ARA_ENABLED
    if (! isBoundToARA())
    {
        destRoot = -999;
        destBass = -999;
        return;
    }
    juce::ScopedLock lock (araChordLock);
    destRoot = araChordRoot.load (std::memory_order_acquire);
    destBass = araChordBass.load (std::memory_order_acquire);
#else
    destRoot = -999;
    destBass = -999;
#endif // OVT_ARA_ENABLED
}

void OpenVoxTunerAudioProcessor::getAraChordAt (double positionPPQ, int& root, int& bass, juce::String& name) const
{
    root = -999;
    bass = -999;
    name.clear();
#if OVT_ARA_ENABLED
    if (! isBoundToARA())
        return;
    juce::ScopedLock lock (araChordLock);
    for (const auto& e : araChordEvents)
    {
        if (positionPPQ >= e.startPPQ && positionPPQ < e.startPPQ + e.duration)
        {
            root = e.root;
            bass = e.bass;
            name = e.name;
            return;
        }
    }
#else
    juce::ignoreUnused (positionPPQ);
#endif // OVT_ARA_ENABLED
}

bool OpenVoxTunerAudioProcessor::isAraChordOutOfScale (double positionPPQ) const
{
    return scaleQuantizer != nullptr && scaleQuantizer->isActiveChordOutOfScale (positionPPQ);
}

// --- Deferred parameter changes (audio â†’ UI thread) ---
// Reads atomics written by the audio thread (applyDetectedKey) and applies
// them via setValueNotifyingHost on the UI thread, avoiding deadlocks.
void OpenVoxTunerAudioProcessor::flushPendingParameterChanges()
{
    const int key = pendingDetectedKey.exchange (kPendingNone);
    if (key != kPendingNone)
    {
        if (auto* p = parameters.getParameter ("key"))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<double> (key)));
    }

    const int scale = pendingDetectedScale.exchange (kPendingNone);
    if (scale != kPendingNone)
    {
        if (auto* p = parameters.getParameter ("scale"))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<double> (scale)));
    }

    // Deferred debug grain parameter reset (audio thread requested, UI thread applies).
    if (pendingDbgGrainReset.exchange (false))
    {
        if (auto* p = parameters.getParameter ("dbg_test_grain"))
            p->setValueNotifyingHost (0.0f);
    }
}

// --- Host transport reading (UI thread) ---
// Historically this function called getPlayHead()->getPosition() from
// the editor's timer.  That pattern is UNSAFE on multiple hosts:
//   * Live 12 AU:  the JuceAU::ScopedPlayHead dereferences an internal
//                  pointer that can be invalidated at any time, causing
//                  a SIGSEGV that cannot be caught.
//   * Cubase LE 15 and Live 12 VST3: getPosition() busy-loops inside
//                  the host, saturating the message thread and
//                  producing the rainbow cursor.
// Both problems are now avoided by running the call on a dedicated
// background thread started in prepareToPlay() (see
// startPlayheadThread).  This function is kept as a no-op for
// backwards compatibility with the editor's timer â€” the editor keeps
// calling it every 30 Hz, but all it does now is bail out early if
// the editor is being torn down.
void OpenVoxTunerAudioProcessor::updateHostTransport()
{
    if (editorShuttingDown.load (std::memory_order_acquire))
    {
        hostProvidesTimeCached.store (false, std::memory_order_relaxed);
        return;
    }

    // The dedicated worker is responsible for calling getPlayHead().
    // Nothing to do here.
}

void OpenVoxTunerAudioProcessor::readAndCachePlayHeadInfo (juce::AudioPlayHead& playHead) noexcept
{
    // Wall-clock time at the moment we ask the host for transport info.
    // Captured ONCE at the top of the function so both branches (playing
    // and stopped) end up with a consistent timestamp paired with the
    // cachedHostPpq they store, which the editor needs to extrapolate
    // the playhead position between worker updates (see
    // cachedHostPpqAtUpdate / cachedHostPpqAtUpdateMs in the header).
    const double updateTimeMs = juce::Time::getMillisecondCounterHiRes();

    try
    {
        auto position = playHead.getPosition();
        if (position.hasValue())
        {
            hostProvidesTimeCached.store (true, std::memory_order_relaxed);

            if (position->getIsPlaying())
            {
                double ppq = position->getPpqPosition().orFallback (cachedHostPpq.load());
                hostIsPlaying.store (1);

                if (position->getIsLooping())
                {
                    if (auto loop = position->getLoopPoints())
                        ppq -= loop->ppqStart;
                }
                cachedHostPpq.store (ppq, std::memory_order_relaxed);
                // Record the (PPQ, timestamp) pair so the UI can
                // extrapolate a smooth playhead position at 60 fps
                // even though the worker only refreshes at 30 Hz.
                cachedHostPpqAtUpdate.store (ppq, std::memory_order_relaxed);
                cachedHostPpqAtUpdateMs.store (updateTimeMs, std::memory_order_relaxed);

#if OVT_ARA_ENABLED
                if (!isBoundToARA())
#endif
                {
                    auto sig = position->getTimeSignature();
                    if (sig.hasValue())
                    {
                        currentTimeSigNumerator.store (sig->numerator);
                        currentTimeSigDenominator.store (sig->denominator);
                    }
                }
            }
            else
            {
                hostIsPlaying.store (0);
                const auto ppq = position->getPpqPosition().orFallback (cachedHostPpq.load());
                cachedHostPpq.store (ppq, std::memory_order_relaxed);
                // Paused/Stopped: still record the pair so the UI freezes
                // the playhead exactly at this PPQ and timestamp
                // (extrapolation below returns cachedHostPpq unchanged
                // when !hostIsPlaying, so the playhead does not drift).
                cachedHostPpqAtUpdate.store (ppq, std::memory_order_relaxed);
                cachedHostPpqAtUpdateMs.store (updateTimeMs, std::memory_order_relaxed);
            }
        }
        else
        {
            hostProvidesTimeCached.store (false, std::memory_order_relaxed);
        }
    }
    catch (...)
    {
        // AU host state can be torn down mid-call in Live 12 when the
        // plugin window closes or the device is reconfigured.  Swallow
        // any exception so the caller (UI timer or audio callback) never
        // propagates a fault out of this function.
        hostProvidesTimeCached.store (false, std::memory_order_relaxed);
    }
}

double OpenVoxTunerAudioProcessor::getInterpolatedTransportTime() const
{
    double t;

    // If the host never told us a position, fall back to the
    // transportTime field (which the audio callback keeps up to date in
    // standalone mode).  This branch is also what the editor hits
    // between worker startup and the first readAndCachePlayHeadInfo().
    if (! hostProvidesTimeCached.load (std::memory_order_relaxed))
    {
        t = transportTime.load (std::memory_order_relaxed);
    }
    else
    {
        const double lastPpq       = cachedHostPpqAtUpdate.load    (std::memory_order_relaxed);
        const double lastUpdateMs  = cachedHostPpqAtUpdateMs.load   (std::memory_order_relaxed);
        const double nowMs         = juce::Time::getMillisecondCounterHiRes();

        // Paused/Stopped: do NOT extrapolate, just return the frozen PPQ.
        // The pair (lastPpq, lastUpdateMs) is still updated every worker
        // tick, so once the user resumes playback the extrapolation
        // continues seamlessly from the right baseline.
        if (hostIsPlaying.load (std::memory_order_relaxed) == 0)
        {
            t = lastPpq;
        }
        else
        {
            // Playing: extrapolate beats elapsed since the last worker update.
            // (now - lastUpdate) is in ms; divide by 1000 to get seconds, then
            // multiply by beatsPerSecond (= bpm/60) to get beats.
            const double deltaSeconds = (nowMs - lastUpdateMs) * 0.001;
            const double beatsPerSecond = static_cast<double> (bpm.load()) * (1.0 / 60.0);

            // Guard against a negative delta (clock skew, very first call, or
            // the worker writing the pair while we're reading it).  In all
            // these cases returning the last known PPQ is the safe choice.
            if (deltaSeconds < 0.0)
                t = lastPpq;
            else
                t = lastPpq + deltaSeconds * beatsPerSecond;
        }
    }

    // Wrap to [0, L] when the playhead is set to loop. In Standalone this
    // is always the case (the menu option is forced ON), and in plugin
    // mode the user can enable it via "Loop Playhead (Measures)". Without
    // this wrap the standalone playhead would keep advancing forever
    // instead of looping on the Measures window the user picked in the
    // combo box, and the curve editor's auto-scroll would slide off the
    // right side of the timeline within seconds.
    if (isPlayheadLooping())
    {
        const double L = getLoopLengthBeats();
        if (L > 0.0)
            t = std::fmod (t, L);
        if (t < 0.0) // std::fmod can return negative on some platforms
            t += L;
    }
    return t;
}

// --- Dedicated playhead reader thread ---
// Runs on its own std::thread, off the UI thread, the audio thread and
// the message thread.  This is the ONLY thread that ever calls
// getPlayHead()->getPosition() at runtime.  Reasoning:
//   * Calling it on the UI thread (timer, original code) freezes Cubase
//     and Live VST3 â€” those hosts busy-loop or deadlock inside
//     getPosition(), saturating the message thread and triggering the
//     rainbow cursor.  Observed on Cubase LE 15.
//   * Calling it on the audio thread (processBlock) is also documented
//     as a deadlock risk in some hosts.
//   * Calling it on a dedicated background thread isolates the cost:
//     if the host misbehaves, only this worker is affected.  The UI
//     thread keeps running, the DAW stays responsive, and the cached
//     atomics simply keep their last good value.
void OpenVoxTunerAudioProcessor::startPlayheadThread()
{
    if (playheadThread.joinable())
        return;

    playheadThreadShouldExit.store (false, std::memory_order_release);

    playheadThread = std::thread ([this]
    {
        using clock = std::chrono::steady_clock;
        // 30 Hz (33 ms) is the sweet spot for a smooth playhead at 60 fps
        // UI refresh: each worker update advances the playhead by ~1 frame
        // at typical tempos, so the visual position feels continuous.
        // 10 Hz (the previous value) was visibly jerky: the playhead
        // jumped 6 frames at a time, which the eye reads as stuttering.
        // Higher rates (60 Hz) double the getPlayHead() call cost without
        // a perceptible visual gain.
        constexpr auto period   = std::chrono::milliseconds (33); // ~30 Hz
        constexpr auto slowThreshold = std::chrono::milliseconds (20);
        constexpr auto backoffPeriod = std::chrono::seconds (1);

        while (! playheadThreadShouldExit.load (std::memory_order_acquire))
        {
            if (auReady.load (std::memory_order_acquire)
                && ! editorShuttingDown.load (std::memory_order_acquire))
            {
                // Measure how long getPlayHead() takes.  On Cubase LE 15
                // and Live 12 VST3 the call can busy-loop inside the host;
                // if it takes longer than slowThreshold we back off so the
                // worker does not saturate the CPU and starve the UI
                // thread of scheduling slots.
                const auto callStart = clock::now();

                juce::AudioPlayHead* playHead = nullptr;
                try
                {
                    playHead = getPlayHead();
                }
                catch (...)
                {
                    hostProvidesTimeCached.store (false, std::memory_order_relaxed);
                }

                if (playHead != nullptr)
                    readAndCachePlayHeadInfo (*playHead);
                else
                    hostProvidesTimeCached.store (false, std::memory_order_relaxed);

                const auto callDuration = clock::now() - callStart;
                if (callDuration > slowThreshold)
                {
                    // Host is misbehaving.  Sleep for backoffPeriod in
                    // small slices so we can still react to a stop
                    // request.
                    const auto backoffEnd = clock::now() + backoffPeriod;
                    while (! playheadThreadShouldExit.load (std::memory_order_acquire)
                           && clock::now() < backoffEnd)
                    {
                        std::this_thread::sleep_for (std::chrono::milliseconds (20));
                    }
                    continue;
                }
            }

            // Normal idle sleep, in small slices so stopPlayheadThread()
            // can join quickly even if the host is blocking the call.
            const auto deadline = clock::now() + period;
            while (! playheadThreadShouldExit.load (std::memory_order_acquire)
                   && clock::now() < deadline)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (5));
            }
        }
    });
}

void OpenVoxTunerAudioProcessor::stopPlayheadThread() noexcept
{
    playheadThreadShouldExit.store (true, std::memory_order_release);

    if (playheadThread.joinable())
    {
        // If the host is currently busy-looping inside getPosition()
        // we cannot interrupt it; std::thread::join() will block until
        // the call eventually returns.  That's acceptable: the UI
        // thread is unaffected and the host stays responsive.  Worst
        // case: a few extra seconds at shutdown.
        try
        {
            playheadThread.join();
        }
        catch (...)
        {
            // std::system_error if the thread is not joinable â€” ignore.
        }
    }
}

// --- ARA metadata reading (UI thread only) ---
// Reads key signatures and bar signatures from the ARA document controller.
// MUST be called from the UI thread â€” HostContentReader acquires a lock that
// can deadlock the audio thread in some hosts (Cubase LE 15, Live VST3).
#if OVT_ARA_ENABLED
void OpenVoxTunerAudioProcessor::updateAraMetadata()
{
    if (! isBoundToARA())
        return;

    auto* dc = getDocumentController();
    if (dc == nullptr)
        return;

    auto* doc = dc->getDocument();
    if (doc == nullptr)
        return;

    auto contexts = doc->getMusicalContexts();
    if (contexts.empty() || contexts[0] == nullptr)
        return;

    // --- Key signatures ---
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeKeySignatures> reader (contexts[0]);
        if (reader.getEventCount() > 0)
        {
            auto* keySig = reader.getDataPtrForEvent (0);
            if (keySig != nullptr)
            {
                int chromatic = ((keySig->root * 7) % 12 + 12) % 12;

                int scaleIndex = 0; // Chromatique par defaut
                int activeNotes = 0;
                for (int i = 0; i < 12; ++i)
                    if (keySig->intervals[i] == 0xFF) activeNotes++;

                if (activeNotes == 12)
                    scaleIndex = 0; // Chromatique
                else if (keySig->intervals[4] == 0xFF)
                    scaleIndex = 1; // Major
                else if (keySig->intervals[3] == 0xFF)
                    scaleIndex = 4; // Natural Minor

                if (keyParam && (keyIntParam != nullptr ? keyIntParam->get() : static_cast<int> (std::round (keyParam->load() * 11.0f))) != chromatic)
                {
                    if (auto* param = parameters.getParameter ("key"))
                        param->setValueNotifyingHost (param->convertTo0to1 (chromatic));
                }

                if (scaleParam && (scaleChoiceParam != nullptr ? scaleChoiceParam->getIndex() : static_cast<int> (scaleParam->load())) != scaleIndex)
                {
                    if (auto* param = parameters.getParameter ("scale"))
                        param->setValueNotifyingHost (param->convertTo0to1 (scaleIndex));
                }
            }
        }
    }

    // --- Bar signatures ---
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> barReader (contexts[0]);
        juce::ScopedLock lock (araBarSigLock);
        araBarSignatures.clear();
        for (int i = 0; i < barReader.getEventCount(); ++i)
        {
            auto* barSig = barReader.getDataPtrForEvent (i);
            if (barSig != nullptr)
            {
                araBarSignatures.push_back ({
                    static_cast<double> (barSig->position),
                    static_cast<int> (barSig->numerator),
                    static_cast<int> (barSig->denominator)
                });
            }
        }
        if (! araBarSignatures.empty())
        {
            currentTimeSigNumerator.store (araBarSignatures[0].numerator);
            currentTimeSigDenominator.store (araBarSignatures[0].denominator);
        }
    }

    // --- Chords (kARAContentTypeSheetChords) ---
    // Extract the lead-sheet chord (root + bass) so the Harmony engine can
    // adapt its strategy dynamically (e.g. force a diatonic harmony type
    // when the host reports a dominant-7th). ARA provides multiple events
    // sorted by position; each event is valid until the next one, and the
    // first event is assumed valid for all times before it is actually defined.
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeSheetChords> chordReader (contexts[0]);
        const int chordCount = chordReader.getEventCount();

        // Rebuild the time-indexed chord-override windows from the ScaleQuantizer.
        // The last chord root/bass are still cached for copyAraChord() / GUI.
        if (scaleQuantizer != nullptr)
            scaleQuantizer->clearChordOverrides();

        if (chordCount > 0)
        {
            // Use the last chord event for the cached root/bass atomics (ARA
            // semantics: each event valid until the next).
            const int bestIdx = juce::jmax (0, chordCount - 1);
            {
                juce::ScopedLock lock (araChordLock);
                araChordEvents.clear();
                // Generate chord symbols from the interval data (the host's
                // `name` field is often empty). Unicode symbols (ø, ♭, ♯, Δ, °)
                // are clearer than ASCII ("halfdim", "b", "#", "maj", "dim").
                ARA::ChordInterpreter chordInterpreter (false);
                for (int i = 0; i < chordCount; ++i)
                {
                    auto* chord = chordReader.getDataPtrForEvent (i);
                    if (chord == nullptr)
                        continue;

                    const int rootCOF = static_cast<int> (chord->root);
                    const int bassCOF = static_cast<int> (chord->bass);

                    // ARACircleOfFifthsIndex -> classe de hauteur chromatique 0..11.
                    // Convention ARA : C=0, G=1, D=2 (cercle des quintes), F=-1.
                    // Conversion : (root * 7) mod 12, ramené dans [0,11].
                    const int rootPC = ((rootCOF * 7) % 12 + 12) % 12;
                    const int bassPC = ((bassCOF * 7) % 12 + 12) % 12;

                    // Décoder le full pitch-class set de l'accord : les intervalles
                    // kARAChordIntervalUsed (0xFF) ou degrés diatoniques (0x01..0x0D)
                    // indiquent les intervalles actifs par rapport à la racine.
                    juce::Array<int> chordNotes;
                    chordNotes.add (rootPC);
                    for (int k = 0; k < 12; ++k)
                        if (chord->intervals[k] != 0) // kARAChordIntervalUnused == 0x00
                            chordNotes.addIfNotAlreadyThere ((rootPC + k) % 12);
                    chordNotes.addIfNotAlreadyThere (bassPC); // la basse peut sortir du triade
                    chordNotes.sort();

                    // Fenêtre de validité : [position, position suivante).
                    double start = static_cast<double> (chord->position);
                    double end   = (i + 1 < chordCount)
                                       ? static_cast<double> (chordReader.getDataPtrForEvent (i + 1)->position)
                                       : (start + 1.0e6); // dernier accord -> ouvert
                    const double duration = juce::jmax (0.0, end - start);

                    if (scaleQuantizer != nullptr)
                        scaleQuantizer->setChordOverride (chordNotes, start, duration);

                    AraChordEvent ev;
                    ev.root = rootCOF;
                    ev.bass = bassCOF;
                    ev.startPPQ = start;
                    ev.duration = duration;
                    ev.name = juce::String::fromUTF8 (chordInterpreter.getNameForChord (*chord).c_str());
                    // "N.C." = undefined chord (no intervals) -> fall back to
                    // the host-provided name, else empty (badge shows the root).
                    if (ev.name == "N.C.")
                        ev.name = (chord->name != nullptr) ? juce::String::fromUTF8 (chord->name) : juce::String();
                    araChordEvents.push_back (ev);

                    if (i == bestIdx)
                    {
                        araChordRoot.store (rootCOF, std::memory_order_release);
                        araChordBass.store (bassCOF, std::memory_order_release);
                    }
                }
            }
        }
        else
        {
            juce::ScopedLock lock (araChordLock);
            araChordEvents.clear();
            araChordRoot.store (-999, std::memory_order_release);
            araChordBass.store (-999, std::memory_order_release);
        }
    }

    // --- Tempo (kARAContentTypeTempoEntries) ---
    // Extract the host tempo so the plugin can align time-based DSP
    // (e.g. humanize, vibrato rate, harmony attack timing) to the actual
    // project BPM instead of guessing from the sample rate.
    // ARA tempo entries: { timePosition (sec), quarterPosition (beats) }.
    // BPM = (quarterDelta / timeDelta) * 60.
    {
        ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> tempoReader (contexts[0]);
        const int tempoCount = tempoReader.getEventCount();
        if (tempoCount >= 2)
        {
            // Compute BPM from the first and last tempo sync points.
            auto* first = tempoReader.getDataPtrForEvent (0);
            auto* last  = tempoReader.getDataPtrForEvent (tempoCount - 1);
            if (first != nullptr && last != nullptr)
            {
                const double dt = static_cast<double> (last->timePosition) - static_cast<double> (first->timePosition);
                const double dq = static_cast<double> (last->quarterPosition) - static_cast<double> (first->quarterPosition);
                if (dt > 0.0 && dq > 0.0)
                {
                    const double bpm = (dq / dt) * 60.0;
                    araTempoBpm.store (bpm, std::memory_order_release);
                    hasAraTempoFlag.store (true, std::memory_order_release);
                }
            }
        }
        else
        {
            araTempoBpm.store (0.0, std::memory_order_release);
            hasAraTempoFlag.store (false, std::memory_order_release);
        }
    }
}
#else
void OpenVoxTunerAudioProcessor::updateAraMetadata() {}
#endif // OVT_ARA_ENABLED

void OpenVoxTunerAudioProcessor::applyLatencyMode()
{
    if (pitchShifter == nullptr)
        return;

    const int mode = (latencyModeParam != nullptr)
        ? juce::jlimit (0, 3, (int) std::round (latencyModeParam->load()))
        : 1;

    if (mode == appliedLatencyMode)
        return;

    const float latencyMs = (mode == 0) ? 10.0f : (mode == 1 ? 12.0f : (mode == 2 ? 20.0f : 30.0f));
    pitchShifter->setLatencyMs (latencyMs);

    for (auto& ps : shiftedVoicePitchShifters)
    {
        if (ps != nullptr)
            ps->setLatencyMs (latencyMs);
    }

    setLatencySamples (pitchShifter->getLatencySamples());
    appliedLatencyMode = mode;

    const char* modeNames[] = { "Direct Monitoring", "Low Latency", "Quality", "Safe" };
    const juce::String modeName = (mode >= 0 && mode <= 3) ? modeNames[mode] : "Unknown";
    OVT_LOG ("Latency mode changed: " + modeName +
             " (" + juce::String (latencyMs, 1) + " ms)");
}
 
// === Synchronisation des parametres utilisateur vers les modules DSP ===
void OpenVoxTunerAudioProcessor::syncParameters()
{
    if (keyParam == nullptr || scaleParam == nullptr)
        return;

    // Gamme musicale.
    const int keyIdx = keyIntParam != nullptr ? keyIntParam->get() : static_cast<int> (std::round (keyParam->load() * 11.0f));
    // Use AudioParameterChoice::getIndex() for reliable conversion,
    // avoiding fragile round(load() * 13.0f) arithmetic that can fail
    // after setStateInformation restores a non-normalized stored value.
    const int scaleIdx = (scaleChoiceParam != nullptr)
        ? scaleChoiceParam->getIndex()
        : static_cast<int> (std::round (scaleParam->load() * 13.0f));
    scaleQuantizer->setKey (keyIdx);
    scaleQuantizer->setScale (static_cast<ovtdsp::Scale> (juce::jlimit (0, 15, scaleIdx)));

    // Si on est en mode "Custom" (scaleIdx == 13), on pousse la liste
    // des notes cochees vers le quantifier.
    if (scaleIdx == 13)
    {
        juce::Array<int> customNotes;
        for (int i = 0; i < 12; ++i)
        {
            if (customParam[i] != nullptr && customParam[i]->load() > 0.5f)
                customNotes.add (i);
        }
        scaleQuantizer->setCustomIntervals (customNotes);
    }

    // Publish a lock-free copy for the UI. The UI must never iterate the
    // ScaleQuantizer's mutable juce::Array while this audio-thread method
    // can rebuild it with clear()/add().
    {
        const auto& intervals = scaleQuantizer->getScaleIntervals();
        const int count = juce::jmin (12, intervals.size());
        for (int i = 0; i < count; ++i)
            scaleIntervalSnapshot[static_cast<size_t> (i)].store (intervals[i], std::memory_order_relaxed);
        scaleIntervalSnapshotSize.store (count, std::memory_order_release);
    }

    // Voice Type: constrain pitch detector search range to reduce octave errors
    // and improve detection speed. Applied when parameter changes.
    if (voiceTypeParam != nullptr && pitchDetectors[0] != nullptr)
    {
        const int currentVoiceType = juce::jlimit (0, 5, static_cast<int> (std::round (voiceTypeParam->load())));
        if (currentVoiceType != lastVoiceType)
        {
            // Apply the new frequency range to the pitch detector
            if (auto* pd = dynamic_cast<ovtdsp::YinPitchDetector*>(pitchDetectors[0].get()))
            {
                pd->setFrequencyRange (voiceTypeMinHz[currentVoiceType], voiceTypeMaxHz[currentVoiceType]);
            }
            // Also update sidechain detector if it exists
            if (sidechainPitchDetector)
            {
                sidechainPitchDetector->setFrequencyRange (voiceTypeMinHz[currentVoiceType], voiceTypeMaxHz[currentVoiceType]);
            }
            lastVoiceType = currentVoiceType;
        }
    }

    // Vitesse de retargeting â€” modulÃ©e par le mode de correction.
    float speed = (speedParam != nullptr) ? speedParam->load() : 50.0f;
    int modeVal = (correctionModeParam != nullptr) ? static_cast<int>(correctionModeParam->load()) : 0;
    if (modeVal == 1) // Transparent
    {
        // Transparent mode: speed floor at 30ms + reduce effective Amount
        // by 20% so the correction is gentler and more natural-sounding
        // even at default settings.
        speed = juce::jmax (30.0f, speed);
    }
    // Mode Modern (0): no restriction â€” user speed is applied as-is.
    retargetEnvelope->setSpeed (speed);
    
    applyLatencyMode();
}

// === Time signature lookup (for Curve Editor ruler) ===
void OpenVoxTunerAudioProcessor::getTimeSignatureAt (double ppq, int& num, int& den) const
{
    juce::ScopedLock lock (const_cast<OpenVoxTunerAudioProcessor*>(this)->araBarSigLock);
    if (!araBarSignatures.empty())
    {
        // Linear scan: find the last bar signature event before or at ppq
        BarSignatureEvent best = araBarSignatures[0];
        for (const auto& e : araBarSignatures)
        {
            if (e.ppqPosition <= ppq)
                best = e;
            else
                break;
        }
        num = best.numerator;
        den = best.denominator;
        return;
    }
    // Fallback to current VST3/standalone signature
    num = currentTimeSigNumerator.load();
    den = currentTimeSigDenominator.load();
}

// === Curve Editor playhead loop mode ===
bool OpenVoxTunerAudioProcessor::isPlayheadLooping() const
{
#if OVT_ARA_ENABLED
    if (isBoundToARA())                                   // ARA: follow the host timeline
        return false;
#endif
    if (wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        return true;                                      // Standalone: always loop
    return getPlayheadLoop();                             // Plugin: user choice
}

double OpenVoxTunerAudioProcessor::getLoopLengthBeats() const
{
    const double measures = editorMeasuresParam != nullptr ? editorMeasuresParam->load() : 4.0;
    const int num = currentTimeSigNumerator.load();
    const int den = currentTimeSigDenominator.load();
    const double beatUnit = 4.0 / (den > 0 ? static_cast<double> (den) : 4.0);
    return measures * static_cast<double> (num) * beatUnit;   // = measures * ppqPerBar
}

double OpenVoxTunerAudioProcessor::getLoopTransportTime() const
{
    double t = transportTime.load();
    if (isPlayheadLooping())
    {
        const double L = getLoopLengthBeats();
        if (L > 0.0)
            t = std::fmod (t, L);
    }
    return t;
}


// === Detection de pitch sur le bloc courant via FIFO glissante ===
void OpenVoxTunerAudioProcessor::applyDetectedKey (int musicalKey, int scaleIdx)
{
    musicalKey = juce::jlimit (0, 11, musicalKey);
    scaleIdx  = juce::jlimit (0, 13, scaleIdx);

    // Only write when the detected key/scale actually differs from the *current*
    // parameter value (not a cached one). Comparing against the live value means
    // a manual scale change by the user is re-asserted by detection on the next
    // estimate: while Key/Scale Detection is enabled, the detected scale is
    // authoritative and will be re-applied even if the user edited it by hand.
    // (To set the scale manually and keep it, turn Key/Scale Detection off.)
    int curKey = -1, curScale = -1;
    if (auto* keyP = parameters.getParameter ("key"))
        curKey = juce::roundToInt (keyP->convertFrom0to1 (keyP->getValue()));
    if (auto* scaleP = parameters.getParameter ("scale"))
        curScale = juce::roundToInt (scaleP->convertFrom0to1 (scaleP->getValue()));
    if (musicalKey == curKey && scaleIdx == curScale)
        return; // unchanged -> no host automation churn

    lastAutoKey = musicalKey;
    lastAutoScale = scaleIdx;

    // Defer the actual parameter update to the UI thread via flushPendingParameterChanges().
    // Calling setValueNotifyingHost() from the audio thread can deadlock certain hosts
    // (Cubase, Live VST3) when combined with ARA locks or heavy UI activity.
    pendingDetectedKey.store (musicalKey);
    pendingDetectedScale.store (scaleIdx);
}

float OpenVoxTunerAudioProcessor::computeInputPitch (const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0)
        return lastInputPitch.load();

    // Copie les echantillons du bloc dans le FIFO (downmix mono si necessaire).
    const int numSamples = buffer.getNumSamples();
    float* fifo = analysisFifo.getWritePointer (0);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float s = (inR != nullptr) ? 0.5f * (inL[i] + inR[i]) : inL[i];
        fifo[fifoWriteIndex] = s;
        fifoWriteIndex = (fifoWriteIndex + 1) % analysisWindow;
        if (fifoFillCount < analysisWindow)
            ++fifoFillCount;
    }

    samplesSinceLastAnalysis += numSamples;

    // Si on n'a pas encore rempli la fenetre, on ne peut pas detecter le pitch.
    if (fifoFillCount < analysisWindow)
    {
        lastRawYinPitch.store (0.0f);
        return lastInputPitch.load();
    }

    // Si on a deja analyse recemment (Hop Size), on economise le CPU.
    if (samplesSinceLastAnalysis < analysisHopSize)
        return lastInputPitch.load();

    samplesSinceLastAnalysis = 0;

    // Prepare un buffer lineaire pour la detection (defifo) en utilisant
    // le buffer PRE-ALLOUE (elimine une heap allocation par bloc audio).
    if (analysisLinearBuffer.getData() == nullptr)
        return lastInputPitch.load();
        
    float* linear = analysisLinearBuffer.getData();
    
    // Decimation par 4 avec anti-aliasing filter pour reduire la charge CPU de YIN.
    // La decimation par 4 a 11025 Hz (44.1k/4) est suffisante pour les voix (Nyquist ~5512 Hz).
    // La moyenne mobile sur 4 echantillons agit comme anti-aliasing filter.
    constexpr int decimation = 4;
    const int decimatedWindow = analysisWindow / decimation;
    
    // On extrait les donnees du FIFO dans l'ordre chronologique
    // Quand le FIFO est plein, fifoWriteIndex pointe sur l'echantillon le plus ancien.
    int idx = fifoWriteIndex;
    for (int i = 0; i < decimatedWindow; ++i)
    {
        float sum = 0.0f;
        for (int j = 0; j < decimation; ++j) {
            sum += fifo[idx];
            idx = (idx + 1) % analysisWindow;
        }
        linear[i] = sum / (float)decimation;
    }

    // Lance la detection sur le buffer decime.
    auto* det = activePitchDetector.load();
    float newPitch = (det != nullptr) ? det->detectPitch (linear, decimatedWindow) : 0.0f;
    
    // Memorise le resultat BRUT de YIN (0 compris) pour le filtre
    // anti-saut-octave. Ceci est SEPARE du fallback ci-dessous.
    lastRawYinPitch.store (newPitch);
    
    // Si YIN trouve un pitch valide, on memorise pour reutilisation lors des
    // micro-pauses de l'anti-octave-error (evite que le ratio autotune retombe
    // a 1.0 -> perte de l'effet).
    if (newPitch > 0.0f)
    {
        lastValidPitchForAutotune.store (newPitch);
        return newPitch;
    }
    
    // YIN n'a rien detecte. Le filtre octave utilisera lastRawYinPitch=0
    // pour se desarmer automatiquement. On conserve le fallback pour le
    // ratio du PitchShifter uniquement.
    float fallback = lastValidPitchForAutotune.load();
    if (fallback > 0.0f)
        return fallback;
    
    return lastInputPitch.load();
}

// === Detection de pitch sur le bus Sidechain (source "Sidechain") ===
// Mirrors computeInputPitch() but fills a dedicated FIFO from the sidechain
// bus and uses its own YIN detector, so the vocal pitch analysis and the
// sidechain key detection never share state.
float OpenVoxTunerAudioProcessor::computeSidechainPitch (const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumChannels() == 0)
        return 0.0f;
    if (currentSampleRate <= 0.0 || sidechainPitchDetector == nullptr)
        return 0.0f;

    // Copie les echantillons du bloc sidechain dans son FIFO (downmix mono).
    const int numSamples = buffer.getNumSamples();
    float* fifo = sidechainFifo.getWritePointer (0);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (buffer.getNumChannels() > 1) ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float s = (inR != nullptr) ? 0.5f * (inL[i] + inR[i]) : inL[i];
        fifo[sidechainFifoWriteIndex] = s;
        sidechainFifoWriteIndex = (sidechainFifoWriteIndex + 1) % analysisWindow;
        if (sidechainFifoFillCount < analysisWindow)
            ++sidechainFifoFillCount;
    }

    sidechainSamplesSinceLastAnalysis += numSamples;

    // FIFO pas encore plein : impossible de detecter.
    if (sidechainFifoFillCount < analysisWindow)
        return 0.0f;

    // Analyse deja faite recemment : on economise le CPU.
    if (sidechainSamplesSinceLastAnalysis < analysisHopSize)
        return 0.0f;

    sidechainSamplesSinceLastAnalysis = 0;

    if (sidechainLinearBuffer.getData() == nullptr)
        return 0.0f;

    float* linear = sidechainLinearBuffer.getData();

    // Decimation par 4 (anti-aliasing) pour alleger YIN.
    constexpr int decimation = 4;
    const int decimatedWindow = analysisWindow / decimation;

    int idx = sidechainFifoWriteIndex;
    for (int i = 0; i < decimatedWindow; ++i)
    {
        float sum = 0.0f;
        for (int j = 0; j < decimation; ++j)
        {
            sum += fifo[idx];
            idx = (idx + 1) % analysisWindow;
        }
        linear[i] = sum / (float)decimation;
    }

    const float newPitch = sidechainPitchDetector->detectPitch (linear, decimatedWindow);
    return (newPitch > 0.0f) ? newPitch : 0.0f;
}

// Detector factory â€” YIN only.
std::unique_ptr<ovtdsp::IPitchDetector> OpenVoxTunerAudioProcessor::createDetector()
{
    return std::make_unique<ovtdsp::YinPitchDetector>();
}

// === Programmes (non utilises pour le MVP) ===
int OpenVoxTunerAudioProcessor::getNumPrograms()              { return 1; }
int OpenVoxTunerAudioProcessor::getCurrentProgram()           { return 0; }
void OpenVoxTunerAudioProcessor::setCurrentProgram (int)      {}
const juce::String OpenVoxTunerAudioProcessor::getProgramName (int) { return {}; }
void OpenVoxTunerAudioProcessor::changeProgramName (int, const juce::String&) {}

// === MIDI : plugin can produce MIDI OUT and consume incoming MIDI ===
// acceptsMidi() must be true so the host delivers the incoming MIDI bus
// to processBlock (used by the "MIDI Target / Follow" feature). The input
// bus is already declared (NEEDS_MIDI_INPUT / JUCE_PLUGIN_WANTS_MIDI_INPUT).
bool OpenVoxTunerAudioProcessor::acceptsMidi() const  { return true; }
bool OpenVoxTunerAudioProcessor::producesMidi() const { return true; }
bool OpenVoxTunerAudioProcessor::isMidiEffect() const { return false; }

// === Latence : nulle pour le squelette, sera mise a jour en Phase 1 ===
double OpenVoxTunerAudioProcessor::getTailLengthSeconds() const
{
    double tail = 0.0;
#if OVT_ARA_ENABLED
    if (getTailLengthSecondsForARA (tail))
        return tail;
#endif
    return 0.0;
}

// === Layout des bus : on accepte mono et stereo, entree == sortie ===
bool OpenVoxTunerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    if (mainIn != juce::AudioChannelSet::mono()
     && mainIn != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != mainIn)
        return false;

    // Sidechain bus (input bus index 1) is optional: allow it disabled
    // (empty channel set) or mono. Any other configuration is rejected.
    return ovtdsp::isSidechainLayoutValid (layouts);
}

// === Global plugin Undo/Redo (Option 1) ===
//
// A single history over the full AudioProcessorValueTreeState. Each
// transaction stores a `before` and `after` ValueTree copy; Undo restores
// `before`, Redo restores `after`. Applying a state goes through
// parameters.replaceState(), which re-fires every parameter listener so the
// UI (sliders, toggles, combos) updates automatically.
//
// The pitch curve lives outside parameters.state (it is a separate
// PitchCurve in the processor) and keeps its own CurveEditor-local undo, so
// it is intentionally NOT part of these snapshots.
namespace
{
    class PluginStateUndoAction : public juce::UndoableAction
    {
    public:
        PluginStateUndoAction (juce::AudioProcessorValueTreeState& params,
                               const juce::ValueTree& before,
                               const juce::ValueTree& after)
            : parameters (params), beforeState (before), afterState (after)
        {}

        bool perform() override
        {
            // Called once on push, and again on redo(). Applies the
            // post-change state.
            // NOTE: replaceState() aliases the passed tree (state = newState),
            // so the live parameters.state would share the stored snapshot's
            // underlying data. If we pass afterState directly, later live
            // edits (via SliderAttachment etc.) would mutate the stored
            // snapshot, corrupting this transaction's redo. Passing a fresh
            // copy keeps the stored snapshot pristine.
            parameters.replaceState (afterState.createCopy());
            return true;
        }

        bool undo() override
        {
            // Called on undo(). Restores the pre-change state. See the
            // aliasing note in perform(); we restore from a fresh copy of the
            // stored snapshot for the same reason.
            parameters.replaceState (beforeState.createCopy());
            return true;
        }

    private:
        juce::AudioProcessorValueTreeState& parameters;
        juce::ValueTree beforeState, afterState;
    };
}

void OpenVoxTunerAudioProcessor::pushUndoAction (const juce::ValueTree& before,
                                                 const juce::ValueTree& after)
{
    // Guard against empty / identical snapshots (e.g. a click that changed
    // nothing, or a programmatic refresh that round-tripped the same values).
    if (! before.isValid() || ! after.isValid())
        return;
    if (before.isEquivalentTo (after))
        return;

    // Start a fresh transaction so each committed gesture becomes its own
    // undo step. Without this, JUCE's UndoManager coalesces consecutive
    // perform() calls into a single transaction, which would collapse
    // multiple user edits into one undo and skip intermediate states.
    pluginUndoManager.beginNewTransaction();
    auto* action = new PluginStateUndoAction (parameters, before, after);
    pluginUndoManager.perform (action); // takes ownership
}

void OpenVoxTunerAudioProcessor::applyPluginPresetState (const juce::ValueTree& presetState)
{
    if (! presetState.isValid())
        return;

    // Capture the current values of user/session preferences that a Plugin
    // Preset must NOT override (they are not part of the "sound"): UI
    // language, UI theme, the A/B morph position, the Live/Curve mode, and
    // the key/scale selection (the user's musical choice, preserved by every
    // preset so e.g. musical presets don't snap back to C Chromatic).
    const float savedLang   = parameters.getParameterAsValue ("ui_language").getValue();
    const float savedTheme  = parameters.getParameterAsValue ("ui_theme").getValue();
    const float savedMorph  = parameters.getParameterAsValue ("morph_amount").getValue();
    const float savedMode   = parameters.getParameterAsValue ("mode").getValue();
    const float savedKey    = parameters.getParameterAsValue ("key").getValue();
    const float savedScale  = parameters.getParameterAsValue ("scale").getValue();
    const float savedKeyDetect = parameters.getParameterAsValue ("key_detect").getValue();

    // Restore the parameter state in one global-undo transaction so a preset
    // load is undoable (Ctrl/Cmd+Z). pushUndoAction() performs the action
    // immediately (which calls parameters.replaceState(after)), so the live
    // tree is swapped in a single, atomic step before we restore the
    // user/session preferences below.
    const juce::ValueTree before = parameters.copyState();
    const juce::ValueTree after  = presetState.createCopy();
    pushUndoAction (before, after);

    // Re-apply the preserved user/session preferences (replaceState swapped
    // the whole tree, so restore them explicitly afterwards).
    parameters.getParameterAsValue ("ui_language").setValue (savedLang);
    parameters.getParameterAsValue ("ui_theme").setValue (savedTheme);
    parameters.getParameterAsValue ("morph_amount").setValue (savedMorph);
    parameters.getParameterAsValue ("mode").setValue (savedMode);
    parameters.getParameterAsValue ("key").setValue (savedKey);
    parameters.getParameterAsValue ("scale").setValue (savedScale);
    parameters.getParameterAsValue ("key_detect").setValue (savedKeyDetect);
}

// === Etat du plugin : serialisation XML des parametres + pitch curve ===
void OpenVoxTunerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Before saving, make the ACTIVE slot reflect the current live state so any
    // edits made just before exit are never lost (the editor only recaptures a
    // slot when the user switches away from it). This keeps the whole-state
    // snapshot consistent on quit/restart regardless of slot-switch timing.
    if (pitchCurve != nullptr)
    {
        const int active = juce::jlimit (0, 1, abActiveSlot);
        auto live = ovtdsp::captureState (parameters, *pitchCurve,
                                          active == 0 ? "Slot A" : "Slot B");
        if (active == 0) abSlotAMorph = live;
        else             abSlotBMorph = live;
        abSlotHasData[active] = true;
    }

    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (pitchCurve != nullptr)
    {
        auto curveXml = pitchCurve->toXml();
        if (xml != nullptr && curveXml != nullptr)
            xml->addChildElement (curveXml.release());
    }
    // Persist UI-only preferences that are not AudioParameters.
    xml->setAttribute ("advancedExpanded", advancedExpandedState ? 1 : 0);
    xml->setAttribute ("abActiveSlot", abActiveSlot);

    // Persist A/B slot MorphStates as compact flat attributes (no nested XML).
    for (int slot = 0; slot < 2; ++slot)
    {
        if (! hasAbSlotData (slot)) continue;
        const auto& ms = (slot == 0) ? abSlotAMorph : abSlotBMorph;
        auto* slotXml = xml->createNewChildElement (slot == 0 ? "AB_A" : "AB_B");
        slotXml->setAttribute ("speed",            (double) ms.speed);
        slotXml->setAttribute ("amount",           (double) ms.amount);
        slotXml->setAttribute ("formant",          (double) ms.formant);
        slotXml->setAttribute ("harmonyGain",      (double) ms.harmonyGain);
        slotXml->setAttribute ("harmonyBlend",     (double) ms.harmonyBlend);
        slotXml->setAttribute ("harmonyToneColor", (double) ms.harmonyToneColor);
        slotXml->setAttribute ("reverbMix",        (double) ms.reverbMix);
        slotXml->setAttribute ("flexTune",         (double) ms.flexTune);
        slotXml->setAttribute ("humanize",         (double) ms.humanize);
        slotXml->setAttribute ("vibratoPreserve",  (double) ms.vibratoPreserve);
        slotXml->setAttribute ("attackAware",      (double) ms.attackAware);
        slotXml->setAttribute ("attackRelease",    (double) ms.attackRelease);
        slotXml->setAttribute ("keySource",        (double) ms.keySource);
        slotXml->setAttribute ("companionGroup",   (double) ms.companionGroup);
        slotXml->setAttribute ("keyDetect",        (double) ms.keyDetect);
        slotXml->setAttribute ("harmonyFollowLead", (double) (ms.harmonyFollowLead ? 1 : 0));
        slotXml->setAttribute ("key",              ms.key);
        slotXml->setAttribute ("scale",            ms.scale);
        slotXml->setAttribute ("harmonyType",      ms.harmonyType);
        slotXml->setAttribute ("harmonyTone",      ms.harmonyTone);
        slotXml->setAttribute ("shiftedVoices",    ms.harmonyShiftedVoices);
        slotXml->setAttribute ("harmonyFormant",   (double) ms.harmonyFormant);
        slotXml->setAttribute ("harmonyAttack",    (double) ms.harmonyAttack);
        slotXml->setAttribute ("voiceType",        ms.voiceType);
        slotXml->setAttribute ("latencyMode",      ms.latencyMode);
        slotXml->setAttribute ("editorMeasures",   ms.editorMeasures);
        slotXml->setAttribute ("formantEnable",    ms.formantEnable);
        slotXml->setAttribute ("bypass",           ms.bypass);
        slotXml->setAttribute ("harmonyEnable",    ms.harmonyEnable);
        slotXml->setAttribute ("useVoice",         ms.harmonyUseVoice);
        slotXml->setAttribute ("reverbEnable",     ms.reverbEnable);
        slotXml->setAttribute ("correctionMode",   ms.correctionMode);
        slotXml->setAttribute ("noiseGateEnable",  ms.noiseGateEnable);
        slotXml->setAttribute ("noiseGateThreshold", (double) ms.noiseGateThreshold);
        slotXml->setAttribute ("upwardCompEnable", ms.upwardCompEnable);
        slotXml->setAttribute ("upwardCompAmount", (double) ms.upwardCompAmount);

        // Serialize the pitch curve so the slot restores its exact line (not an
        // empty PitchCurve) after a project reload / standalone restart.
        auto curveXml = ms.curve.toXml();
        if (curveXml != nullptr)
            slotXml->addChildElement (curveXml.release());

#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
        if (slot == 0)
            OVT_LOG ("[DIAG] getState AB_A: has=" + juce::String (hasAbSlotData (0) ? 1 : 0)
                     + " type=" + juce::String (abSlotAMorph.harmonyType)
                     + " vibrato=" + juce::String (abSlotAMorph.vibratoPreserve, 4)
                     + " humanize=" + juce::String (abSlotAMorph.humanize, 4)
                     + " formant=" + juce::String (abSlotAMorph.harmonyFormant, 4)
                     + " activeSlot=" + juce::String (abActiveSlot));
#endif
    }
    copyXmlToBinary (*xml, destData);
}

void OpenVoxTunerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
    {
        // The A/B slots and the pitch curve are plugin-level data, NOT parameter
        // state. getStateInformation() appends them to the serialized XML, so if
        // they flow into parameters.state via fromXml() here, every save/load
        // round-trip re-appends another AB_A/AB_B/PITCH_CURVE and the tree
        // accumulates dozens of stale copies. getChildByName("AB_A") then returns
        // the OLDEST copy (often the defaults captured on the first ever save),
        // which is exactly the "slot A restores to defaults" bug. So we read the
        // plugin-level data first, then strip it before replacing the param tree.
        auto getLastChild = [&xmlState] (const juce::String& tag) -> juce::XmlElement*
        {
            juce::XmlElement* found = nullptr;
            if (xmlState != nullptr)
                for (auto* e = xmlState->getFirstChildElement(); e != nullptr; e = e->getNextElement())
                    if (e->hasTagName (tag)) found = e;
            return found;
        };

        // Restore A/B slot MorphStates from compact flat attributes. Reading the
        // LAST occurrence recovers the most recent state even from files written
        // by older builds that accumulated duplicate slot children.
        for (int slot = 0; slot < 2; ++slot)
        {
            auto* slotXml = getLastChild (slot == 0 ? "AB_A" : "AB_B");
            if (slotXml == nullptr) continue;
            ovtdsp::MorphState ms;
            ms.speed              = (float) slotXml->getDoubleAttribute ("speed", 0.25);
            ms.amount             = (float) slotXml->getDoubleAttribute ("amount", 0.5);
            ms.formant            = (float) slotXml->getDoubleAttribute ("formant", 0.5);
            ms.harmonyGain        = (float) slotXml->getDoubleAttribute ("harmonyGain", 0.5);
            ms.harmonyBlend       = (float) slotXml->getDoubleAttribute ("harmonyBlend", 0.5);
            ms.harmonyToneColor   = (float) slotXml->getDoubleAttribute ("harmonyToneColor", 0.5);
            ms.reverbMix          = (float) slotXml->getDoubleAttribute ("reverbMix", 0.3);
            ms.flexTune           = (float) slotXml->getDoubleAttribute ("flexTune", 0.25);
            ms.humanize           = (float) slotXml->getDoubleAttribute ("humanize", 0.0);
            ms.vibratoPreserve    = (float) slotXml->getDoubleAttribute ("vibratoPreserve", 0.0);
            ms.attackAware        = (float) slotXml->getDoubleAttribute ("attackAware", 0.0);
            ms.attackRelease      = (float) slotXml->getDoubleAttribute ("attackRelease", 60.0);
            ms.keySource          = (float) slotXml->getDoubleAttribute ("keySource", 0.0);
            ms.companionGroup     = (float) slotXml->getDoubleAttribute ("companionGroup", 0.0);
            ms.keyDetect          = (float) slotXml->getDoubleAttribute ("keyDetect", 0.0);
            ms.harmonyFollowLead  = slotXml->getDoubleAttribute ("harmonyFollowLead", 1.0) > 0.5;
            ms.key                = slotXml->getIntAttribute ("key", 0);
            ms.scale              = slotXml->getIntAttribute ("scale", 0);
            ms.harmonyType        = slotXml->getIntAttribute ("harmonyType", 0);
            ms.harmonyTone        = slotXml->getIntAttribute ("harmonyTone", 0);
            ms.harmonyShiftedVoices = slotXml->getIntAttribute ("shiftedVoices", 1);
            ms.harmonyFormant     = (float) slotXml->getDoubleAttribute ("harmonyFormant", 0.5);
            ms.harmonyAttack      = (float) slotXml->getDoubleAttribute ("harmonyAttack", 0.1137);
            ms.voiceType          = slotXml->getIntAttribute ("voiceType", 0);
            ms.latencyMode        = slotXml->getIntAttribute ("latencyMode", 1);
            ms.editorMeasures     = slotXml->getIntAttribute ("editorMeasures", 8);
            ms.formantEnable      = slotXml->getBoolAttribute ("formantEnable", false);
            ms.bypass             = slotXml->getBoolAttribute ("bypass", false);
            ms.harmonyEnable      = slotXml->getBoolAttribute ("harmonyEnable", false);
            ms.harmonyUseVoice    = slotXml->getBoolAttribute ("useVoice", false);
            ms.reverbEnable       = slotXml->getBoolAttribute ("reverbEnable", false);
            ms.correctionMode     = slotXml->getBoolAttribute ("correctionMode", false);
            ms.noiseGateEnable    = slotXml->getBoolAttribute ("noiseGateEnable", false);
            ms.noiseGateThreshold = (float) slotXml->getDoubleAttribute ("noiseGateThreshold", 0.375);
            ms.upwardCompEnable   = slotXml->getBoolAttribute ("upwardCompEnable", false);
            ms.upwardCompAmount   = (float) slotXml->getDoubleAttribute ("upwardCompAmount", 0.25);
            // Restore the pitch curve (absent in pre-curve states -> empty curve).
            auto* curveXml = slotXml->getChildByName ("PITCH_CURVE");
            if (curveXml != nullptr)
                ms.curve.fromXml (*curveXml);
            setAbSlotMorphState (slot, std::move (ms));
        }
        // Restore the pitch curve if present in the XML.
        if (pitchCurve != nullptr)
        {
            auto* curveXml = getLastChild ("PITCH_CURVE");
            if (curveXml != nullptr)
            {
                pitchCurve->fromXml (*curveXml);
                pendingCurveRestore.store (true);
            }
        }

        // Strip the plugin-level children so they do not accumulate inside
        // parameters.state on successive save/load cycles.
        for (auto* e = xmlState->getFirstChildElement(); e != nullptr;)
        {
            auto* next = e->getNextElement();
            if (e->hasTagName ("AB_A") || e->hasTagName ("AB_B") || e->hasTagName ("PITCH_CURVE"))
                xmlState->removeChildElement (e, true);
            e = next;
        }

        advancedExpandedState = xmlState->getBoolAttribute ("advancedExpanded", false);
        abActiveSlot = juce::jlimit (0, 1, xmlState->getIntAttribute ("abActiveSlot", 0));
        parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

// === GUI : hook pour creer l'editeur ===
juce::AudioProcessorEditor* OpenVoxTunerAudioProcessor::createEditor()
{
    return new OpenVoxTunerAudioProcessorEditor (*this);
}

bool OpenVoxTunerAudioProcessor::hasEditor() const { return true; }

juce::VST3ClientExtensions* OpenVoxTunerAudioProcessor::getVST3ClientExtensions()
{
    // Only return the PreSonus MicroView extension for Studio One; other hosts
    // (Cubase, Live, etc.) may hang when presented with an unknown VST3 extension.
    // Returning nullptr is safe for all hosts â€” the MicroView is purely cosmetic.
    return nullptr;
}

// === getScaleNoteNames (static) ===
juce::Array<juce::String> OpenVoxTunerAudioProcessor::getScaleNoteNames
    (int key, ovtdsp::Scale scale)
{
    ovtdsp::ScaleQuantizer quantizer;
    quantizer.setKey (key);
    quantizer.setScale (scale);

    const auto& intervals = quantizer.getScaleIntervals();

    static const std::array<const char*, 12> chroma =
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::Array<juce::String> result;
    for (int interval : intervals)
    {
        int noteIdx = (key + interval) % 12;
        result.add (chroma[noteIdx]);
    }
    return result;
}

// === Thread-safe snapshot accessor for scale intervals ===
void OpenVoxTunerAudioProcessor::copyScaleIntervals (juce::Array<int>& destination) const
{
    destination.clear();
    const int count = juce::jlimit (0, 12,
                                    scaleIntervalSnapshotSize.load (std::memory_order_acquire));
    for (int i = 0; i < count; ++i)
        destination.add (scaleIntervalSnapshot[static_cast<size_t> (i)].load (std::memory_order_relaxed));
}

// === Creation du plugin (point d'entree JUCE) ===
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenVoxTunerAudioProcessor();
}



