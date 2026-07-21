// PluginProcessor.cpp
// Implementation of the audio processor (DSP pipeline Phase 1).

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
#include <algorithm>
#include <juce_data_structures/juce_data_structures.h> // juce::UndoManager, juce::ValueTree

// IMPORTANT: OVT_LOG is wrapped in #if JUCE_DEBUG to match the convention
// used in PitchShifter.cpp. Reason: in Release builds, juce::Logger::writeToLog
// still runs and allocates a juce::String per call (even though
// BufferedFileLogger amortises the disk I/O), and at one of the call sites
// (processBlock line 2091, "MIDI: f0_out=...") the log fires EVERY audio
// block (~100 calls/sec while singing). On real-time-constrained DAWs like
// Studio One, this string allocation + lock contention is enough to push
// the 11.6 ms block deadline (~512 samples at 44.1 kHz) and cause audible
// dropouts — the user reported this regression on 2026-07-17. Keeping the
// log gated to Debug means Release is free of any string work in the hot
// path. To re-enable logging in a Release build for diagnosis, define
// OVT_FORCE_LOG in the build flags (e.g. cmake -DCMAKE_CXX_FLAGS="-DOVT_FORCE_LOG").
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
 #define OVT_LOG(msg) juce::Logger::writeToLog (msg)
#else
 #define OVT_LOG(msg) do { } while (false)
#endif

// Definition of the IID for the IEditControllerExtra interface
#include "pluginterfaces/base/funknown.h"
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
//   second), the per-block callback was taking 1-5 ms just for I/O —
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
//==============================================================================
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

                      // Harmony Enable : master on/off — disabled by default
                      std::make_unique<juce::AudioParameterBool> (
                          "harmony_enable", "Harmony Enable", false),

                      // Harmony Gain : niveau de volume des harmonies — 1.0 by default
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_gain", "Harmony Volume",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.75f),

                      // Harmony Blend : melange voix principale / harmonies — 0.5 by default
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_blend", "Harmony Blend",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f)
                      ,
                      // Use Voice : enabled by default
                      std::make_unique<juce::AudioParameterBool> (
                          "harmony_use_voice", "Use Voice for Harmony", true),
                      // Shifted voices : 4 by default
                      std::make_unique<juce::AudioParameterInt> (
                          "harmony_shifted_voices", "Shifted Voices", 1, 4, 4),
                      std::make_unique<juce::AudioParameterChoice> (
                          "harmony_tone", "Harmony Tone",
                          juce::StringArray { "Choir", "Bright", "Synth Lead", "Strings", "Guitar", "Vocoder-like" }, 0),
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
                      , std::make_unique<juce::AudioParameterBool> (
                            "reverb_enable", "Reverb Enable", false)
                      , std::make_unique<juce::AudioParameterBool> (
                            juce::ParameterID { "noise_gate_enable", 1 }, "Noise Gate", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            juce::ParameterID { "noise_gate_threshold", 1 }, "Gate Threshold",
                            juce::NormalisableRange<float> (-80.0f, 0.0f, 1.0f), -40.0f)
                      // Upward compression: lifts quiet passages before tuning.
                      // Single knob = amount (0..1); pivot follows the signal RMS.
                      , std::make_unique<juce::AudioParameterBool> (
                            "upward_comp_enable", "Upward Comp Enable", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "upward_comp_amount", "Upward Comp Amount",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "reverb_mix", "Reverb Mix",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.30f)
                      // FlexTune: deadband around the target note (0-100 cents)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "flex_tune", "FlexTune",
                            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 10.0f)
                      // Humanize: random pitch fluctuations (0-50 cents)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "humanize", "Humanize",
                            juce::NormalisableRange<float> (0.0f, 50.0f, 1.0f), 40.0f)
                      // Correction mode: Modern (false) / Transparent (true)
                      , std::make_unique<juce::AudioParameterBool> (
                            "correction_mode", "Correction Mode", false)
                      // Vibrato preservation: 0% = classic instantaneous correction,
                      // 100% = correction against the smoothed center pitch so the
                      // vibrato modulation survives.
                      , std::make_unique<juce::AudioParameterFloat> (
                            "vibrato_preserve", "Vibrato Preserve",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f)
                      // Attack-aware correction: enable + release time (ms). When
                      // enabled, the correction is eased off on note onsets/transients.
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

    // Initialise le compteur de persistence anti-saut-octave
    octaveJumpRejectionCount = 0;

    // Retrieves raw pointers to the parameters' atomic values.
    speedParam   = parameters.getRawParameterValue ("speed");
    amountParam  = parameters.getRawParameterValue ("amount");
    latencyModeParam = parameters.getRawParameterValue ("latency_mode");
    formantParam = parameters.getRawParameterValue ("formant");
    formantEnableParam = parameters.getRawParameterValue ("formant_enable");
    formantModeParam = parameters.getRawParameterValue ("formant_mode");
    keyParam     = parameters.getRawParameterValue ("key");
    scaleParam   = parameters.getRawParameterValue ("scale");
    scaleChoiceParam = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("scale"));
    bypassParam = parameters.getRawParameterValue ("bypass");
    modeParam   = parameters.getRawParameterValue ("mode");

    harmonyTypeParam  = parameters.getRawParameterValue ("harmony_type");
    harmonyGainParam  = parameters.getRawParameterValue ("harmony_gain");
    harmonyBlendParam = parameters.getRawParameterValue ("harmony_blend");
    harmonyEnableParam = parameters.getRawParameterValue ("harmony_enable");
    harmonyUseVoiceParam = parameters.getRawParameterValue ("harmony_use_voice");
    harmonyShiftedVoicesParam = parameters.getRawParameterValue ("harmony_shifted_voices");
    harmonyToneParam = parameters.getRawParameterValue ("harmony_tone");
    harmonyToneColorParam = parameters.getRawParameterValue ("harmony_tone_color");
    harmonyFollowLeadParam = parameters.getRawParameterValue ("harmony_follow_lead");
    harmonyGainMatchParam = parameters.getRawParameterValue ("harmony_gain_match");
    midiOutEnableParam = parameters.getRawParameterValue ("midi_out_enable");
    midiTargetEnableParam = parameters.getRawParameterValue ("midi_target_enable");
    dbgTestGrainParam = parameters.getRawParameterValue ("dbg_test_grain");
    morphAmountParam = parameters.getRawParameterValue ("morph_amount");
    morphParam = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter ("morph_amount"));
    editorMeasuresParam = parameters.getRawParameterValue ("editor_measures");
    editorPlayheadLoopParam = parameters.getRawParameterValue ("editor_playhead_loop");
    detectorParam = parameters.getRawParameterValue ("pitch_detector");
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

    // Instantiates DSP modules — YIN pitch detector.
    pitchDetectors[0] = std::make_unique<ovtdsp::YinPitchDetector>();
    activePitchDetector.store (pitchDetectors[0].get());

    // Dedicated YIN detector for the optional Sidechain input bus (used by the
    // "Sidechain" key source). Kept independent from the main-input detector.
    sidechainPitchDetector = std::make_unique<ovtdsp::YinPitchDetector>();
    activeDetectorMode = 0;
    scaleQuantizer   = std::make_unique<ovtdsp::ScaleQuantizer>();
    scaleQuantizer->setScale (ovtdsp::Scale::Chromatic); // Ensure chromatic on first launch
    pitchShifter     = std::make_unique<ovtdsp::PitchShifter>();
    harmonyEngine    = std::make_unique<ovtdsp::HarmonyEngine>();

    retargetEnvelope = std::make_unique<ovtdsp::RetargetEnvelope>();
    pitchCurve       = std::make_unique<ovtdsp::PitchCurve>();
    pitchCurve->loadPreset ("default");

    // Initialize post-processing effects (reverb, etc.)
    effects.push_back (std::make_unique<ovtdsp::ReverbEffect>());
    OVT_LOG ("Effects initialized: " + juce::String (static_cast<int> (effects.size())));

    // Instantiation of the VST3 extension for Fender Studio Pro (Micro View)
    vst3Extensions = std::make_unique<PresonusMicroViewExtension>();

    // Install file logger in Debug AND Release (so we can diagnose drops in the field).
    {
        juce::File logDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("OpenVoxTuner")
                                .getChildFile ("logs");
        logDir.createDirectory();
        juce::File logFile = logDir.getChildFile (
            "ovt_" + juce::Time::getCurrentTime().toISO8601(true).replaceCharacter (':', '-') + ".log");
        juce::Logger::setCurrentLogger (new BufferedFileLogger (logFile));
        OVT_LOG ("OpenVoxTuner log initialized: " + logFile.getFullPathName());
    }

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

OpenVoxTunerAudioProcessor::~OpenVoxTunerAudioProcessor() = default;

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
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());

    // Initialize DSP modules with the current sample rate.
    // Prepare the YIN pitch detector.
    if (pitchDetectors[0] != nullptr)
        pitchDetectors[0]->prepare (sampleRate / 4.0, samplesPerBlock);
    // Reset the latency-mode cache so applyLatencyMode() ALWAYS re-runs
    // setLatencySamples() on this prepareToPlay(), even if the user has
    // not changed the mode since the last prepare. This is needed
    // because some hosts (notably Studio One with VST3) do NOT re-call
    // prepareToPlay() when the user disables and re-enables the insert
    // slot — they only re-read the last reported latency from the
    // plugin. If we early-return inside applyLatencyMode() (which we
    // do in syncParameters() to avoid spamming the host every block),
    // the host keeps showing the previous latency value, which can
    // disagree with the actual mode selected in the plugin UI. Forcing
    // the re-apply here guarantees the host's PDC always matches the
    // plugin's current mode, regardless of whether prepareToPlay() is
    // the only host hook that fires on insert re-enable.
    appliedLatencyMode = -1;
    applyLatencyMode();
    pitchShifter->prepare (sampleRate, samplesPerBlock);
    noiseGate.prepare (sampleRate);
    upwardComp.prepare (sampleRate);
    formantPreserver.prepare (sampleRate, samplesPerBlock);
    
    // Report latency to DAW for automatic compensation (PDC)
    setLatencySamples(pitchShifter->getLatencySamples());

    retargetEnvelope->prepare (sampleRate);
    harmonyEngine->prepare (sampleRate);

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
    synthWorkBuffer.setSize (workCh, samplesPerBlock, false, true, false);
    synthWorkBuffer.clear();
    lastMixedHarmonyBuffer.setSize (workCh, samplesPerBlock, false, true, false);
    lastMixedHarmonyBuffer.clear();
    for (auto& g : shiftedVoiceGains)
    {
        g.reset (sampleRate, 0.02); // 20 ms per-voice smoothing (increased from 10ms to reduce clicks)
        g.setCurrentAndTargetValue (0.0f);
    }

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

    // Configure the plugin latency based on the host's block size.
    // This allows the DAW to compensate for the delay introduced by buffering.
    latencySamples = pitchShifter->getLatencySamples();
    setLatencySamples (latencySamples);

    // Debug: log prepareToPlay info
    OVT_LOG ("prepareToPlay: sampleRate=" + juce::String(sampleRate) +
                              " samplesPerBlock=" + juce::String(samplesPerBlock) +
                              " pitchShifter_latency=" + juce::String(latencySamples));

    // Reset the silence counter
    maxSilenceSamples = static_cast<int>(sampleRate * 0.5); // 500 ms silence tail
}

void OpenVoxTunerAudioProcessor::releaseResources()
{
    releaseResourcesForARA();

    for (int i = 0; i < 1; ++i)
        if (pitchDetectors[i] != nullptr) pitchDetectors[i]->reset();
    if (pitchShifter != nullptr)     pitchShifter->reset();
    if (retargetEnvelope != nullptr) retargetEnvelope->reset();
    for (auto& ps : shiftedVoicePitchShifters)
        if (ps != nullptr)
            ps->reset();

    // Reset MIDI tracking state (host is releasing audio graph)
    for (int ch = 0; ch < 16; ++ch)
        lastSentMidiNote[ch] = -1;
}

// === Routine audio principale (appel bloc par bloc par le host) ===
void OpenVoxTunerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto blockStartTime = juce::Time::getHighResolutionTicks();
    // MIDI out may be produced below if enabled by parameter.

    auto flushPendingMidiNotes = [&midiMessages, this] (const juce::String& reason)
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
            OVT_LOG ("MIDI flush: " + reason);
        }
    };

    // Bypass : on laisse passer l'audio tel quel.
    if (bypassParam != nullptr && bypassParam->load() > 0.5f)
    {
        flushPendingMidiNotes ("bypass active");
        lastInputPitch.store (0.0f);
        lastOutputPitch.store (0.0f);
        lastCentsOffset.store (0.0f);
        return;
    }

    // === PITCH DETECTOR SWITCHING (YIN only — single mode) ===
    // The pitch_detector parameter is read-only (single choice "YIN").
    // No switching needed — always use index 0.

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
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numSamples > 0 && numCh > 0)
        {
            const juce::CriticalSection::ScopedLockType sl (araWaveformLock);
            araWaveformBuffer.setSize (1, numSamples, false, false, true);
            araWaveformBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
            for (int ch = 1; ch < numCh; ++ch)
                araWaveformBuffer.addFrom (0, 0, buffer, ch, 0, numSamples);
            araWaveformBuffer.applyGain (0, 0, numSamples, 1.0f / (float) numCh);
            araWaveformSampleRate = currentSampleRate;
            araWaveformReady = true;
        }
    }

    // === LECTURE DES METADONNEES ARA ===
    // Si ARA est actif, on extrait la tonalite (Key) du projet.
    if (isBoundToARA())
    {
        if (auto* dc = getDocumentController())
        {
            if (auto* doc = dc->getDocument())
            {
                auto contexts = doc->getMusicalContexts();
                if (!contexts.empty() && contexts[0] != nullptr)
                {
                    ARA::PlugIn::HostContentReader<ARA::kARAContentTypeKeySignatures> reader (contexts[0]);
                    if (reader.getEventCount() > 0)
                    {
                        auto* keySig = reader.getDataPtrForEvent(0);
                        if (keySig != nullptr)
                        {
                            // Convertir circle of fifths (-6 a +6) en chromatic (0 a 11)
                            int chromatic = ((keySig->root * 7) % 12 + 12) % 12;
                            
                            // Determiner le type de gamme (Majeur vs Mineur vs Chromatique)
                            int scaleIndex = 0; // Par defaut Chromatique
                            
                            int activeNotes = 0;
                            for (int i = 0; i < 12; ++i) {
                                if (keySig->intervals[i] == 0xFF) activeNotes++;
                            }
                            
                            if (activeNotes == 12) {
                                scaleIndex = 0; // Chromatic
                            } else if (keySig->intervals[4] == 0xFF) {
                                scaleIndex = 1; // Major (index 1 dans la liste)
                            } else if (keySig->intervals[3] == 0xFF) {
                                scaleIndex = 4; // Natural Minor (index 4 dans la liste)
                            }
                            
                            // Mettre a jour les parametres si changement (notifie l'UI et l'hote)
                            if (keyParam && static_cast<int> (std::round (keyParam->load() * 11.0f)) != chromatic)
                            {
                                if (auto* param = parameters.getParameter("key"))
                                    param->setValueNotifyingHost (param->convertTo0to1 (chromatic));
                            }
                            
                            if (scaleParam && static_cast<int>(scaleParam->load()) != scaleIndex)
                            {
                                if (auto* param = parameters.getParameter("scale"))
                                    param->setValueNotifyingHost (param->convertTo0to1 (scaleIndex));
                            }
                        }

                    // Lire les Bar Signatures ARA (time signature) pour le Curve Editor.
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
                        if (!araBarSignatures.empty())
                        {
                            currentTimeSigNumerator.store (araBarSignatures[0].numerator);
                            currentTimeSigDenominator.store (araBarSignatures[0].denominator);
                        }
                    }

    // After mixing, if engine has finished releasing, shifted voices have ramped down
    // and there's no live pitch, clear cached notes so we don't re-trigger residual rendering.
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
                }
            }
        }
    }

    // Synchronise les parametres avec les modules DSP.
    syncParameters();

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
    //    On interroge le host via getPlayHead() qui donne le PPQ (Beats).
    //    IMPORTANT : les appels getPlayHead() et getLoopPoints() sont coûteux
    //    et peuvent bloquer le thread audio (XRuns sur Reaper/FL Studio).
    //    On limite les mises à jour à 1x / 10 ms via cachedTransportTime.
    double currentTime = transportTime.load();
    bool hostProvidesTime = false;
    bool isStandalone = (wrapperType == juce::AudioProcessor::wrapperType_Standalone);

    uint32_t transportNowMs = juce::Time::getMillisecondCounter();
    uint32_t lastUpd = lastTransportTimeUpdateMs.load();
    if (transportNowMs - lastUpd > 10)  // mise à jour au plus 1x / 10 ms
    {
        lastTransportTimeUpdateMs.store (transportNowMs);

        // Voice/silence hysteresis for harmony gate (prevents rapid on/off chattering)
        constexpr float harmonyGateOnThreshold  = 0.0040f;
        constexpr float harmonyGateOffThreshold = 0.0025f;
        if (maxMagnitude >= harmonyGateOnThreshold)
            harmonyInputGateOpen = true;
        else if (maxMagnitude <= harmonyGateOffThreshold)
            harmonyInputGateOpen = false;

        if (auto* playHead = getPlayHead())
        {
            auto position = playHead->getPosition();
            if (position.hasValue() && !isStandalone)
            {
                hostProvidesTime = true;
                if (position->getIsPlaying())
                {
                    double ppq = position->getPpqPosition().orFallback (currentTime);
                    hostIsPlaying.store (1);

                    // getLoopPoints() peut bloquer sur certains hosts (Reaper).
                    // Bypass en Standalone ou si le DAW ne boucle pas.
                    if (position->getIsLooping() && !isStandalone)
                    {
                        if (auto loop = position->getLoopPoints())
                        {
                            ppq -= loop->ppqStart;
                        }
                    }
                    currentTime = ppq;

                    // Lire la time signature du DAW (non-ARA).
                    if (!isBoundToARA())
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
                    currentTime = position->getPpqPosition().orFallback (currentTime);
                }
            }
        }

        cachedTransportTime.store (currentTime);
    }
    else
    {
        // Utilise la valeur en cache pour eviter les appels synchrones
        // au DAW (getPlayHead) qui degraderaient les performances temps reel.
        // En Standalone (aucun host), on se base sur transportTime pour que
        // l'avance de CHAQUE block s'accumule correctement : la cache 10 ms
        // ferait sinon perdre les blocks intermediaires et ralentirait
        // l'horloge (facteur dependant de la taille de block, ~4x a 48 kHz /
        // 120 echantillons). Le mode host garde la valeur en cache.
        currentTime = hostProvidesTime ? cachedTransportTime.load() : transportTime.load();
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
        flushPendingMidiNotes ("sleep mode (silence)");

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
    if (!isBoundToARA())
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
                // it directly. (Multiplying by 3.0f — as if it were normalised — would
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
            // reset the parameter to 0 via the ValueTree so editor sees it cleared
            if (auto* p = parameters.getParameter ("dbg_test_grain"))
                p->setValueNotifyingHost (0.0f);
        }
        prevDbgTestGrain.store(cur);
    }

    // 3) Quantification : auto vs graphic selon le mode.
    float targetRatio = 1.0f;
    float f0_out = f0_in;
    const int mode = (modeParam != nullptr) ? static_cast<int> (modeParam->load()) : 0;

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
            f0_target = scaleQuantizer->quantize (f0_in);
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
        currentFlexTuneAmount = 1.0f;
        // Only apply FlexTune logic when parameter > 0 (deadband enabled)
        // and we have valid pitch data. Use a minimum of 0.5f to avoid
        // floating point precision issues at very small values.
        if (flexTuneCents > 0.5f && f0_in > 0.0f && f0_target > 0.0f)
        {
            // Cents difference: 1200 * log2(ratio) = 1200 * log2(f0_in / f0_target)
            float centsDiff = 1200.0f * std::abs (std::log2 (f0_in / f0_target));
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
                         " f0_out=" + juce::String (f0_out, 2) +
                         " targetRatio=" + juce::String (targetRatio, 3));
            }
        }

        // Smooth FlexTune amount to prevent clicks when knob is adjusted
        // Time constant ~100ms (0.95 = 100ms at 44.1kHz/480 samples)
        smoothedFlexTuneAmount = smoothedFlexTuneAmount * 0.95f + currentFlexTuneAmount * 0.05f;

        f0_out = f0_target;
        targetRatio = f0_target / f0_in;

        // Humanize: add subtle, smoothed pitch fluctuations (in cents).
        // Max range is 0-8 cents (about 1/6 of a semitone) at max setting.
        // The random value is heavily smoothed via a low-pass filter
        // (~100ms time constant) to avoid harsh per-frame jumps.
        float humanizeAmt = (humanizeParam != nullptr) ? humanizeParam->load() : 0.0f;
        if (humanizeAmt > 1.0f && f0_target > 0.0f && f0_target != f0_in)
        {
            float targetCents = (random.nextFloat() - 0.5f) * 2.0f * humanizeAmt * 0.08f;
            // Smooth the random variation (95% previous, 5% new)
            currentHumanizeCents = currentHumanizeCents * 0.95f + targetCents * 0.05f;
            f0_target *= std::pow (2.0f, currentHumanizeCents / 12.0f);
            targetRatio = f0_target / f0_in;
        }
        else
        {
            // Decay the humanize smoothly when not active
            currentHumanizeCents *= 0.95f;
        }

        // === VIBRATO PRESERVATION ===
        // Blend the standard (instantaneous) correction toward a center-based
        // correction. The center pitch (vibratoPreserver.getCenter()) is a
        // low-pass of f0_in, so the vibrato LFO is removed from it. Correcting
        // against the center and re-applying the ratio to the instantaneous
        // pitch keeps the vibrato modulation intact while still snapping the
        // note to the scale. At preserve == 0 this is the classic behaviour.
        float vibratoPreserve = (vibratoPreserveParam != nullptr) ? vibratoPreserveParam->load() : 0.0f;
        if (vibratoPreserve > 0.001f)
        {
            const float center = vibratoPreserver.getCenter();
            if (center > 0.0f)
            {
                // Target for the smoothed center reference, using the same mode
                // logic as the instantaneous path above.
                float f0_target_center = f0_target;
                if (mode == 1 && pitchCurve != nullptr && pitchCurve->getNumPoints() >= 2)
                    f0_target_center = pitchCurve->getPitchAt (currentTransportTime, center);
                else
                    f0_target_center = scaleQuantizer->quantize (center);

                targetRatio = vibratoPreserver.blend (targetRatio, f0_in, f0_target_center, vibratoPreserve);
                f0_target = f0_in * targetRatio;
                f0_out = f0_target; // keep GUI/harmony note in sync with the blended target
            }
        }

        // Calcul de l'offset en cents between pitch d'entree et pitch quantife.
        // Positif = entree trop haute, Negatif = entree trop basse.
        if (f0_target > 0.0f)
            lastCentsOffset.store (ovtdsp::hzToCents (f0_in, f0_target));
        else
            lastCentsOffset.store (0.0f);

        // === HARMONY ENGINE : generate harmonized voices ===
        int currentHarmonyType = (harmonyTypeParam != nullptr) ? static_cast<int>(harmonyTypeParam->load()) : 0;

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
            // Extraire les paramètres pour l'engine d'harmonie
            int currentKey = (keyParam != nullptr) ? static_cast<int> (std::round (keyParam->load() * 11.0f)) : 0;
            int currentScaleIdx = (scaleParam != nullptr) ? static_cast<int>(scaleParam->load()) : 0;

            // Récupère les intervalles de la gamme depuis le quantizer
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

                // Store detected harmony frequencies for the GUI (may include Follow Lead shift)
                harmonyFrequencies.clear();
                for (int i = 0; i < static_cast<int>(notes.size()); ++i)
                {
                    if (notes[i] > 0.0f && harmonyGainParam && harmonyGainParam->load() > 0.001f)
                        harmonyFrequencies.add(notes[i]);
                }

                // Store scale-locked harmony frequencies for MIDI OUT (Follow Lead ignored).
                harmonyFrequenciesClean.clear();
                for (int i = 0; i < static_cast<int>(cleanNotes.size()); ++i)
                {
                    if (cleanNotes[i] > 0.0f && harmonyGainParam && harmonyGainParam->load() > 0.001f)
                        harmonyFrequenciesClean.add(cleanNotes[i]);
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
        amount *= smoothedFlexTuneAmount;
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
                         " smoothedFlexTuneAmount=" + juce::String (smoothedFlexTuneAmount, 3) +
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
        if (attackAwareParam != nullptr && attackReleaseParam != nullptr)
        {
            attackEnv.setEnabled (attackAwareParam->load() > 0.5f);
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
                amount *= attackEnv.process (blockRms, blockDur);
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
    lastOutputPitch.store (f0_out);

    // 4) Lissage temporel du ratio via RetargetEnvelope (Speed).
    const float ratio = retargetEnvelope->processBlock (targetRatio, buffer.getNumSamples());

    // Memorise le ratio apres lissage pour reutilisation lors des micro-pauses.
    lastRatioSnapshot.store (ratio);

    // 5) Application du WSOLA (Autotune + Formant Shift natif)
    // Formant EFFECT (creative ±5st shift) - controlled by Formant Enable toggle
    bool isFormantEffectEnabled = (formantEnableParam != nullptr) ? (formantEnableParam->load() > 0.5f) : false;
    float shiftSemitones = (isFormantEffectEnabled && formantParam != nullptr) ? formantParam->load() : 0.0f;
    float userFormantRatio = std::pow (2.0f, shiftSemitones / 12.0f);

    // Formant PRESERVATION (anti-chipmunk quality) - ALWAYS ON for main voice + harmony "use voice"
    // Update FormantPreserver mode from parameter (set via menu, not UI combo)
    if (formantModeParam != nullptr)
    {
        int modeIdx = static_cast<int> (std::round (formantModeParam->load()));
        formantPreserver.setMode (static_cast<ovtdsp::FormantPreserver::Mode> (juce::jlimit (0, 1, modeIdx)));
    }
    // Always enabled for quality preservation
    formantPreserver.setEnabled (true);
    formantPreserver.setFormantShift (shiftSemitones);

    // Apply formant preservation BEFORE pitch shifting (PSOLA)
    formantPreserver.process (buffer, ratio);

    // Throttled log around pitchShifter call (once per second)
    static std::atomic<uint32_t> lastPitchLogMs { 0 };
    uint32_t nowP = juce::Time::getMillisecondCounter();
    uint32_t lastP = lastPitchLogMs.load();
    if (nowP - lastP > 1000)
    {
        if (lastPitchLogMs.compare_exchange_strong (lastP, nowP))
        {
            OVT_LOG ("calling pitchShifter->process: ratio=" + juce::String(ratio, 6)
                                      + " formantRatio=" + juce::String(userFormantRatio, 6)
                                      + " f0_in=" + juce::String(f0_in, 6));
        }
    }

    // Save input snapshot BEFORE pitchShifter modifies buffer in-place,
    // so shifted harmony voices can read from the original (un-formanted) signal.
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
    }

    pitchShifter->process (buffer, ratio, userFormantRatio, f0_in);

    // Read global grain event counter signaled by PitchShifter
    int globalGrains = gPitchShifterGrainEvents.load(std::memory_order_relaxed);
    if (globalGrains != lastObservedGrainCount)
    {
        lastObservedGrainCount = globalGrains;
    }

    // === HYBRID HARMONY GENERATION + MIXING ===
    int currentHarmonyTypeVal = (harmonyTypeParam != nullptr) ? static_cast<int>(harmonyTypeParam->load()) : 0;

    // Early calculation of useVoiceLocal / harmonyEnabled (needed for shiftedCount clamp).
    bool useVoiceLocal = (harmonyUseVoiceParam != nullptr) ? (harmonyUseVoiceParam->load() > 0.5f) : false;
    bool harmonyEnabled = (harmonyEnableParam == nullptr || harmonyEnableParam->load() > 0.5f);

    // Early calculation of shiftedCount (needed for clamping before harmony generation).
    int shiftedCount = 0;
    if (useVoiceLocal && harmonyShiftedVoicesParam != nullptr)
        shiftedCount = juce::jlimit (0, OpenVoxTunerAudioProcessor::maxShiftedVoices, static_cast<int> (std::round (harmonyShiftedVoicesParam->load())));

    // Clamp shiftedCount to the actual voice count for this harmony type.
    // The UI knob may allow up to maxShiftedVoices, but the harmony type
    // determines how many voices actually exist. Without this clamp, the
    // mismatch check expects more notes than getHarmonyNotes() returns.
    const int maxVoicesForType = ovtdsp::HarmonyEngine::getHarmonyVoiceCount (
        static_cast<ovtdsp::HarmonyType>(currentHarmonyTypeVal));
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
    bool forceShiftedProcessing = (currentHarmonyTypeVal != 0 && harmonyEnabled && useVoiceLocal);

    if ( (currentHarmonyTypeVal != 0 && ( (f0_out > 0.0f) || (harmonyEngine != nullptr && harmonyEngine->isActive()) || shiftedVoicesActive ) && harmonyEnabled)
         || forceShiftedProcessing )
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = juce::jmax (1, getMainBusNumOutputChannels());
        const float harmonyBlend = (harmonyBlendParam ? harmonyBlendParam->load() : 0.0f);

        int currentKey = (keyParam != nullptr) ? static_cast<int>(keyParam->load()) : 0;
        int currentScaleIdx = (scaleParam != nullptr) ? static_cast<int>(scaleParam->load()) : 0;

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

            // Ensure notesToUse is valid and matches expected size.
            // If main harmony generation was skipped but shifted voices run (forceShiftedProcessing),
            // lastHarmonyNotes may be stale or wrong size. Fall back to fresh generation.
            const size_t expectedSize = (size_t)juce::jmax (1, clampedShiftedCount);
            if (notesToUse.size() != expectedSize)
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
                        regenF0, regenIntervals, static_cast<ovtdsp::HarmonyType>(currentHarmonyTypeVal));
                    juce::String notesStr;
                    for (int i = 0; i < notesToUse.size(); ++i)
                        notesStr += (i > 0 ? "," : "") + juce::String(notesToUse[i], 2);
                    OVT_LOG ("notesToUse regenerated: type=" + juce::String(currentHarmonyTypeVal) +
                             " f0=" + juce::String(regenF0, 2) + " notes=[" + notesStr + "]");
                    // Update the persistent cache so next block has valid data
                    lastHarmonyNotes = notesToUse;
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

                float ratioH = juce::jmax(0.25f, juce::jmin(4.0f, targetHz / safe_f0));
                if (v < static_cast<int>(shiftedVoicePitchShifters.size()) && shiftedVoicePitchShifters[v] != nullptr)
                    shiftedVoicePitchShifters[v]->process (synthWorkBuffer, tmp, ratioH, 1.0f, safe_f0);
                else
                    pitchShifter->process (synthWorkBuffer, tmp, ratioH, 1.0f, safe_f0);

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
                // Use a higher base level (4.0 vs 1.05) for shifted voices so that
                // real audio input (typically ~0.2 peak for vocals) produces a
                // comparable output volume to synthesized sine waves (~0.25 peak).
                // The sqrt(N) normalization maintains consistent perceived loudness
                // regardless of the number of active shifted voices.
                const float perVoiceLevel = 4.0f / std::sqrt ((float) juce::jmax (1, clampedShiftedCount));

                if (!activeShiftedVoice) {
                    shiftedVoiceGains[(size_t)v].setTargetValue (0.0f);
                } else {
                    shiftedVoiceGains[(size_t)v].setTargetValue (1.0f);
                }

                float leftGain = 1.0f;
                float rightGain = 1.0f;
                if (numChannels > 1)
                {
                    // 1st=full right, 2nd=full left, 3rd=centre-right, 4th=centre-left
                    static constexpr float panPos[OpenVoxTunerAudioProcessor::maxShiftedVoices] = { 1.0f, -1.0f, 0.5f, -0.5f };
                    const float pan = panPos[juce::jlimit (0, OpenVoxTunerAudioProcessor::maxShiftedVoices - 1, v)];
                    const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
                    leftGain = std::cos (angle);
                    rightGain = std::sin (angle);
                }

                if (numChannels > 1)
                {
                    float* dstL = harmonyBuffer.getWritePointer (0);
                    float* dstR = harmonyBuffer.getWritePointer (1);
                    const float* srcL = tmp.getReadPointer (0);
                    const float* srcR = tmp.getReadPointer (1);
                    const float baseGL = blendFactor * perVoiceLevel * leftGain;
                    const float baseGR = blendFactor * perVoiceLevel * rightGain;

                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float vg = shiftedVoiceGains[(size_t)v].getNextValue();
                        dstL[i] += srcL[i] * baseGL * vg;
                        dstR[i] += srcR[i] * baseGR * vg;
                    }
                }
                else
                {
                    float* dst = harmonyBuffer.getWritePointer (0);
                    const float* src = tmp.getReadPointer (0);
                    const float baseG = blendFactor * perVoiceLevel;
                    for (int i = 0; i < numSamples; ++i)
                    {
                        const float vg = shiftedVoiceGains[(size_t)v].getNextValue();
                        dst[i] += src[i] * baseG * vg;
                    }
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
                harmonyToneColor
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

                 for (int i = 0; i < numS; ++i)
                 {
                     const float hL = harmonyBuffer.getReadPointer (0)[i];
                     const float hR = numCh == 1 ? hL : harmonyBuffer.getReadPointer (1)[i];
                     outL[i] += hL * hGainFinal;
                     if (outR != nullptr)
                         outR[i] += hR * hGainFinal;
                 }

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
            // MIDI pitch log: gated to ~1/sec to avoid allocating a juce::String
            // every audio block (the per-block OVT_LOG was a real-time cost
            // contributor in DAWs like Studio One when the user had features
            // like Flex/Attack active that already stress the audio callback
            // timing).
            static std::atomic<uint32_t> lastMidiF0LogMs { 0 };
            const uint32_t nowM = juce::Time::getMillisecondCounter();
            uint32_t lastM = lastMidiF0LogMs.load();
            if (nowM - lastM > 1000)
            {
                if (lastMidiF0LogMs.compare_exchange_strong (lastM, nowM))
                {
                    OVT_LOG ("MIDI: f0_out=" + juce::String(f0_out, 2) + "Hz midi=" + juce::String(tunedMidi));
                }
            }
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
                OVT_LOG ("MIDI: NoteOff ch=" + juce::String(midiChannel) + " note=" + juce::String(lastNote));
            }
            if (want != -1 && want != lastNote)
            {
                uint8_t vel = (ch == 0) ? 127 : 100; // tuned=127, harmonies=100
                auto on = juce::MidiMessage::noteOn(midiChannel, want, (juce::uint8)vel);
                midiMessages.addEvent(on, 0);
                lastSentMidiNote[ch] = want;
                OVT_LOG ("MIDI: NoteOn  ch=" + juce::String(midiChannel) + " note=" + juce::String(want) + " vel=" + juce::String((int)vel));
            }
        }
    }
    else
    {
        // If MIDI out is disabled while notes were active, send a proper release.
        flushPendingMidiNotes ("MIDI OUT disabled");
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
}

void OpenVoxTunerAudioProcessor::copyAraWaveform (juce::AudioBuffer<float>& dest, double& sr)
{
    const juce::CriticalSection::ScopedLockType sl (araWaveformLock);
    dest.makeCopyOf (araWaveformBuffer);
    sr = araWaveformSampleRate;
}

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
    const int keyIdx = static_cast<int> (std::round (keyParam->load() * 11.0f));
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

    // Vitesse de retargeting — modulée par le mode de correction.
    float speed = (speedParam != nullptr) ? speedParam->load() : 50.0f;
    int modeVal = (correctionModeParam != nullptr) ? static_cast<int>(correctionModeParam->load()) : 0;
    if (modeVal == 1) // Transparent
    {
        // Transparent mode: speed floor at 30ms + reduce effective Amount
        // by 20% so the correction is gentler and more natural-sounding
        // even at default settings.
        speed = juce::jmax (30.0f, speed);
    }
    // Mode Modern (0): no restriction — user speed is applied as-is.
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
    if (isBoundToARA())                                   // ARA: follow the host timeline
        return false;
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

    if (auto* keyP = parameters.getParameter ("key"))
        keyP->setValueNotifyingHost (keyP->convertTo0to1 (static_cast<double> (musicalKey)));
    if (auto* scaleP = parameters.getParameter ("scale"))
        scaleP->setValueNotifyingHost (scaleP->convertTo0to1 (static_cast<double> (scaleIdx)));
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

// Detector factory — YIN only.
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
    if (getTailLengthSecondsForARA (tail))
        return tail;
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
    }
    copyXmlToBinary (*xml, destData);
}

void OpenVoxTunerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
    {
        parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
        waveformDisplayType = xmlState->getIntAttribute ("waveformDisplayType", 1);
        advancedExpandedState = xmlState->getBoolAttribute ("advancedExpanded", false);
        // Restore A/B slot MorphStates from compact flat attributes.
        for (int slot = 0; slot < 2; ++slot)
        {
            auto* slotXml = xmlState->getChildByName (slot == 0 ? "AB_A" : "AB_B");
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
            ms.latencyMode        = slotXml->getIntAttribute ("latencyMode", 1);
            ms.editorMeasures     = slotXml->getIntAttribute ("editorMeasures", 8);
            ms.formantEnable      = slotXml->getBoolAttribute ("formantEnable", false);
            ms.bypass             = slotXml->getBoolAttribute ("bypass", false);
            ms.harmonyEnable      = slotXml->getBoolAttribute ("harmonyEnable", false);
            ms.harmonyUseVoice    = slotXml->getBoolAttribute ("useVoice", false);
            ms.reverbEnable       = slotXml->getBoolAttribute ("reverbEnable", false);
            ms.correctionMode     = slotXml->getBoolAttribute ("correctionMode", false);
            ms.noiseGateEnable    = slotXml->getBoolAttribute ("noiseGateEnable", false);
            ms.noiseGateThreshold = (float) slotXml->getDoubleAttribute ("noiseGateThreshold", 0.667);
            ms.upwardCompEnable   = slotXml->getBoolAttribute ("upwardCompEnable", false);
            ms.upwardCompAmount   = (float) slotXml->getDoubleAttribute ("upwardCompAmount", 0.5);
            // Restore the pitch curve (absent in pre-curve states -> empty curve).
            auto* curveXml = slotXml->getChildByName ("PITCH_CURVE");
            if (curveXml != nullptr)
                ms.curve.fromXml (*curveXml);
            setAbSlotMorphState (slot, std::move (ms));
        }
        // Restore pitch curve if present in the XML.
        if (pitchCurve != nullptr)
        {
            auto* curveXml = xmlState->getChildByName ("PITCH_CURVE");
            if (curveXml != nullptr)
            {
                pitchCurve->fromXml (*curveXml);
                pendingCurveRestore.store (true);
            }
        }
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
    return vst3Extensions.get();
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

// === Public accessor for scale intervals ===
const juce::Array<int>& OpenVoxTunerAudioProcessor::getScaleIntervals() const
{
    static const juce::Array<int> empty;
    return scaleQuantizer != nullptr ? scaleQuantizer->getScaleIntervals() : empty;
}

// === Creation du plugin (point d'entree JUCE) ===
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenVoxTunerAudioProcessor();
}
