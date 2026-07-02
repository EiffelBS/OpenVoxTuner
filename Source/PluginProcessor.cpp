// PluginProcessor.cpp
// Implementation of the audio processor (DSP pipeline Phase 1).

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/NoteUtils.h"
#include "dsp/ReverbEffect.h"
#include "external/presonus/ipsleditcontroller.h"
// Generated build info (created by CMake)
#include "BuildInfo.h"
#include "dsp/PitchShifter.h" // for gPitchShifterGrainEvents

#if JUCE_DEBUG
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


// Simple file logger for debug builds: captures Logger::writeToLog messages to a file
class SimpleFileLogger : public juce::Logger
{
public:
    explicit SimpleFileLogger (const juce::File& f)
        : file (f)
    {
        file.deleteFile(); // start fresh
        file.create();
    }

    void logMessage (const juce::String& message) override
    {
        const juce::String line = juce::Time::getCurrentTime().toString(true, true) + " " + message + "\n";
        juce::ScopedLock lock (writeLock);
        file.appendText (line);
#if JUCE_WINDOWS
        // Also emit to the debugger output so DebugView or Visual Studio can catch it
        OutputDebugStringA (line.toUTF8().getAddress());
#endif
    }

private:
    juce::File file;
    juce::CriticalSection writeLock;
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
                       #if ! JucePlugin_IsMidiEffect
                          .withInput  ("Input",  juce::AudioChannelSet::mono(), false)
                          .withOutput ("Output", juce::AudioChannelSet::mono(), false)
                       #endif
                          ),
      parameters (*this, nullptr, juce::Identifier ("OpenVoxTuner"),
                  {
                      // Speed : temps de retargeting en millisecondes (0-200 ms)
                      std::make_unique<juce::AudioParameterFloat> (
                          "speed", "Speed",
                          juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f),
                          50.0f),
                      std::make_unique<juce::AudioParameterChoice> (
                          "latency_mode", "Latency Mode",
                          juce::StringArray { "Low Latency", "Quality", "Safe" }, 1),

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
                              "Power Chord", "Parallel 3rd", "Drone"
                          }, 3),

                      // Harmony Enable : master on/off — disabled by default
                      std::make_unique<juce::AudioParameterBool> (
                          "harmony_enable", "Harmony Enable", false),

                      // Harmony Gain : niveau de volume des harmonies — 1.0 by default
                      std::make_unique<juce::AudioParameterFloat> (
                          "harmony_gain", "Harmony Volume",
                          juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f),

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
                       std::make_unique<juce::AudioParameterBool> (
                          "midi_out_enable", "MIDI Out Enable",
                          // In standalone mode, disable MIDI out by default.
                          ! isStandaloneWrapper())
                      , std::make_unique<juce::AudioParameterBool> (
                          "dbg_test_grain", "Debug Test Grain", false)
                      , std::make_unique<juce::AudioParameterInt> (
                            "editor_measures", "Editor Measures", 1, 32, 4)
                      , std::make_unique<juce::AudioParameterBool> (
                            "auto_scroll", "Auto Scroll", true)
                      , std::make_unique<juce::AudioParameterChoice> (
                            "pitch_detector", "Pitch Detector",
                            juce::StringArray { "YIN", "SWIPE'" }, 0)
                      , std::make_unique<juce::AudioParameterBool> (
                            "reverb_enable", "Reverb Enable", false)
                      , std::make_unique<juce::AudioParameterFloat> (
                            "reverb_mix", "Reverb Mix",
                            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f)
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
    keyParam     = parameters.getRawParameterValue ("key");
    scaleParam   = parameters.getRawParameterValue ("scale");
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
    midiOutEnableParam = parameters.getRawParameterValue ("midi_out_enable");
    dbgTestGrainParam = parameters.getRawParameterValue ("dbg_test_grain");
    editorMeasuresParam = parameters.getRawParameterValue ("editor_measures");
    detectorParam = parameters.getRawParameterValue ("pitch_detector");
    reverbEnableParam = parameters.getRawParameterValue ("reverb_enable");
    reverbMixParam = parameters.getRawParameterValue ("reverb_mix");

    for (int i = 0; i < 12; ++i)
    {
        const juce::String id = "custom" + juce::String (i);
        customParam[i] = parameters.getRawParameterValue (id);
    }

    // Instantiates DSP modules — both YIN and SWIPE' are prepared
    // at startup so switching is instant and lock-free.
    pitchDetectors[0] = createDetector (0); // YIN
    pitchDetectors[1] = createDetector (1); // SWIPE'
    activePitchDetector.store (pitchDetectors[0].get());
    activeDetectorMode = 0;
    scaleQuantizer   = std::make_unique<atdsp::ScaleQuantizer>();
    pitchShifter     = std::make_unique<atdsp::PitchShifter>();
    harmonyEngine    = std::make_unique<atdsp::HarmonyEngine>();

    retargetEnvelope = std::make_unique<atdsp::RetargetEnvelope>();
    pitchCurve       = std::make_unique<atdsp::PitchCurve>();
    pitchCurve->loadPreset ("default");

    // Initialize post-processing effects (reverb, etc.)
    effects.push_back (std::make_unique<atdsp::ReverbEffect>());
    OVT_LOG ("Effects initialized: " + juce::String (static_cast<int> (effects.size())));

    // Instantiation of the VST3 extension for Fender Studio Pro (Micro View)
    vst3Extensions = std::make_unique<PresonusMicroViewExtension>();

    // Install file logger only in Debug builds.
   #if JUCE_DEBUG
    {
        juce::File logFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                 .getChildFile ("OpenVoxTuner.log");
        // Create and set the logger (JUCE takes ownership)
        juce::Logger::setCurrentLogger (new SimpleFileLogger (logFile));
        OVT_LOG ("OpenVoxTuner log initialized: " + logFile.getFullPathName());
    }
   #else
    juce::Logger::setCurrentLogger (nullptr);
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
}

OpenVoxTunerAudioProcessor::~OpenVoxTunerAudioProcessor() = default;

// === Plugin name ===
const juce::String OpenVoxTunerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

// === Audio configuration (before the first processBlock) ===
void OpenVoxTunerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Prepare ARA support
    prepareToPlayForARA (sampleRate, samplesPerBlock, getMainBusNumOutputChannels(), getProcessingPrecision());

    // Initialize DSP modules with the current sample rate.
    // Prepare both detectors so switching is instant and safe.
    for (int i = 0; i < 2; ++i)
    {
        if (pitchDetectors[i] != nullptr)
        {
            int detDecimation = (pitchDetectors[i]->getName() == "SWIPE'") ? 1 : 4;
            pitchDetectors[i]->prepare (sampleRate / detDecimation, samplesPerBlock);
        }
    }
    applyLatencyMode();
    pitchShifter->prepare (sampleRate, samplesPerBlock);
    
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
        g.reset (sampleRate, 0.01); // 10 ms per-voice smoothing
        g.setCurrentAndTargetValue (0.0f);
    }

    // Prepare dedicated pitch shifters for shifted harmony voices
    if (shiftedVoicePitchShifters.size() != OpenVoxTunerAudioProcessor::maxShiftedVoices)
    {
        shiftedVoicePitchShifters.clear();
        shiftedVoicePitchShifters.resize (OpenVoxTunerAudioProcessor::maxShiftedVoices);
        for (auto& ps : shiftedVoicePitchShifters)
            ps = std::make_unique<atdsp::PitchShifter>();
    }
    for (auto& ps : shiftedVoicePitchShifters)
        if (ps != nullptr)
            ps->prepare (sampleRate, samplesPerBlock);


    // Prepare the analysis FIFO for pitch detection.
    analysisFifo.setSize (1, analysisWindow, false, true, false);
    analysisFifo.clear();
    fifoWriteIndex = 0;
    fifoFillCount = 0;

    // Pre-allocate the linear buffer used by computeInputPitch().
    // Eliminates an 8 KB heap allocation on each audio block, a major
    // source of glitches (especially at small buffer sizes, e.g., 144 samples).
    analysisLinearBuffer.allocate (analysisWindow, true);

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
    juce::ScopedNoDenormals;
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

    // === PITCH DETECTOR SWITCHING (lock-free atomic swap) ===
    {
        int requestedMode = (detectorParam != nullptr) ? static_cast<int>(detectorParam->load()) : 0;
        if (requestedMode > 0) requestedMode = 1; // clamp to 0/1 (YIN / SWIPE')
        if (requestedMode != activeDetectorMode)
        {
            auto* newDetector = pitchDetectors[requestedMode].get();
            if (newDetector != nullptr)
            {
                activePitchDetector.store (newDetector);
                activeDetectorMode = requestedMode;
            }
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
                            if (keyParam && static_cast<int>(keyParam->load()) != chromatic)
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
        currentTime = cachedTransportTime.load();
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
          // 120 BPM = 2 beats per second
          currentTime += (static_cast<double>(buffer.getNumSamples()) / getSampleRate()) * 2.0;
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
    float f0_in = computeInputPitch (buffer);
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

        if (mode == 1 && pitchCurve != nullptr && pitchCurve->getNumPoints() >= 2)
        {
            // === Mode GRAPHIC : on suit la pitch curve dessinee ===
            // En Standalone, transportTime continue d'augmenter a l'infini.
            // On boucle sur 16 beats (4 mesures de 4/4).
            double currentTransportTime = std::fmod(currentTime, 16.0);
            f0_target = pitchCurve->getPitchAt (currentTransportTime, f0_in);
        }
        else
        {
            // === Mode AUTO : quantification standard vers la gamme ===
            f0_target = scaleQuantizer->quantize (f0_in);
        }

        f0_out = f0_target;
        targetRatio = f0_target / f0_in;

        // Calcul de l'offset en cents between pitch d'entree et pitch quantife.
        // Positif = entree trop haute, Negatif = entree trop basse.
        if (f0_target > 0.0f)
            lastCentsOffset.store (atdsp::hzToCents (f0_in, f0_target));
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
            int currentKey = (keyParam != nullptr) ? static_cast<int>(keyParam->load()) : 0;
            int currentScaleIdx = (scaleParam != nullptr) ? static_cast<int>(scaleParam->load()) : 0;

            // Récupère les intervalles de la gamme depuis le quantizer
            const juce::Array<int>& intervals = scaleQuantizer->getScaleIntervals();

            // Determine which note set to render: if we have a live output pitch use it,
            // otherwise reuse the lastHarmonyNotes so the engine can render the release.
            juce::Array<float> notes;
            if (f0_out > 0.0f)
            {
                notes = harmonyEngine->getHarmonyNotes (
                    f0_out,
                    intervals,
                    static_cast<atdsp::HarmonyType>(currentHarmonyType)
                );
                // cache notes for potential release rendering
                lastHarmonyNotes = notes;

                // Store detected harmony frequencies for the GUI
                harmonyFrequencies.clear();
                for (int i = 0; i < static_cast<int>(notes.size()); ++i)
                {
                    if (notes[i] > 0.0f && harmonyGainParam && harmonyGainParam->load() > 0.001f)
                        harmonyFrequencies.add(notes[i]);
                }
            }
            else
            {
                // No live pitch, but engine active -> render release using cached notes
                notes = lastHarmonyNotes;
                // Clears the UI visualizer lines so they strictly match the live lead vocal trace
                harmonyFrequencies.clear();
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

        // Application de l'intensite (Amount).
        const float amount = amountParam->load();
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
    bool isFormantEnabled = (formantEnableParam != nullptr) ? (formantEnableParam->load() > 0.5f) : true;
    float shiftSemitones = (isFormantEnabled && formantParam != nullptr) ? formantParam->load() : 0.0f;
    float userFormantRatio = std::pow (2.0f, shiftSemitones / 12.0f);

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
        OVT_LOG ("Observed new PitchShifter grains: total=" + juce::String(globalGrains) +
                                  " (since last=" + juce::String(lastObservedGrainCount) + ")");
        lastObservedGrainCount = globalGrains;
    }

    // === HYBRID HARMONY GENERATION + MIXING ===
    int currentHarmonyTypeVal = (harmonyTypeParam != nullptr) ? static_cast<int>(harmonyTypeParam->load()) : 0;

    bool shiftedVoicesActive = false;
    for (auto& g : shiftedVoiceGains)
    {
        if (g.getCurrentValue() > 0.001f)
            shiftedVoicesActive = true;
    }

    if (f0_out > 0.0f)
        lastValidF0.store (f0_out);

    const float f0_for_shifted = (f0_out > 0.0f) ? f0_out : lastValidF0.load();

    bool useVoiceLocal = (harmonyUseVoiceParam != nullptr) ? (harmonyUseVoiceParam->load() > 0.5f) : false;
    bool harmonyEnabled = (harmonyEnableParam == nullptr || harmonyEnableParam->load() > 0.5f);

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
        if (useVoiceLocal && harmonyShiftedVoicesParam != nullptr)
            shiftedCount = juce::jlimit (0, OpenVoxTunerAudioProcessor::maxShiftedVoices, static_cast<int> (std::round (harmonyShiftedVoicesParam->load())));

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
            for (int v = 0; v < OpenVoxTunerAudioProcessor::maxShiftedVoices; ++v)
            {
                const bool activeShiftedVoice = (f0_out > 0.0f && v < shiftedCount && v < notesToUse.size() && notesToUse[v] > 0.0f);

                // By continually calling process(), we seamlessly ingest tracking noise.
                // The targetHz just falls back to a valid shifted ratio during silence.
                const float safe_f0 = (f0_for_shifted > 0.0f) ? f0_for_shifted : 440.0f;
                const float targetHz = (v < notesToUse.size() && notesToUse[v] > 0.0f) ? notesToUse[v] : (safe_f0 * 1.5f);

                auto& tmp = shiftedVoiceBuffers[v];
                if (tmp.getNumChannels() != numChannels || tmp.getNumSamples() != numSamples)
                    tmp.setSize (numChannels, numSamples, false, true, false);

                float ratioH = juce::jmax(0.25f, juce::jmin(4.0f, targetHz / safe_f0));
                if (v < static_cast<int>(shiftedVoicePitchShifters.size()) && shiftedVoicePitchShifters[v] != nullptr)
                    shiftedVoicePitchShifters[v]->process (synthWorkBuffer, tmp, ratioH, 1.0f, safe_f0);
                else
                    pitchShifter->process (synthWorkBuffer, tmp, ratioH, 1.0f, safe_f0);

                if (v == 0)
                {
                    OVT_LOG ("Shifted voice processed: targetHz=" + juce::String(targetHz) +
                                              " ratioH=" + juce::String(ratioH, 6));
                }

                const float blendFactor = 1.0f - harmonyBlend;
                // Use a higher base level (4.0 vs 1.05) for shifted voices so that
                // real audio input (typically ~0.2 peak for vocals) produces a
                // comparable output volume to synthesized sine waves (~0.25 peak).
                // The sqrt(N) normalization maintains consistent perceived loudness
                // regardless of the number of active shifted voices.
                const float perVoiceLevel = 4.0f / std::sqrt ((float) juce::jmax (1, shiftedCount));

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
        for (int v = shiftedCount; v < notesToUse.size(); ++v)
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

                 for (int i = 0; i < numS; ++i)
                 {
                     const float hL = harmonyBuffer.getReadPointer (0)[i];
                     const float hR = numCh == 1 ? hL : harmonyBuffer.getReadPointer (1)[i];
                     outL[i] += hL * hGain;
                     if (outR != nullptr)
                         outR[i] += hR * hGain;
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

                // Save last mixed harmony contribution (scaled by hGain)
                // for potential crossfade on stop.
                if (lastMixedHarmonyBuffer.getNumChannels() != numCh || lastMixedHarmonyBuffer.getNumSamples() != numS)
                    lastMixedHarmonyBuffer.setSize (numCh, numS, false, true, true);
                lastMixedHarmonyBuffer.clear();
                for (int ch = 0; ch < numCh; ++ch)
                {
                    lastMixedHarmonyBuffer.copyFrom (ch, 0, harmonyBuffer, ch, 0, numS);
                    juce::FloatVectorOperations::multiply (lastMixedHarmonyBuffer.getWritePointer (ch), hGain, numS);
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
            OVT_LOG ("MIDI: f0_out=" + juce::String(f0_out, 2) + "Hz midi=" + juce::String(tunedMidi));
        }
        desired[0] = tunedMidi;

        // Harmony notes -> channels 2..9
        juce::Array<float> notesToUse;
        if (f0_out > 0.0f && harmonyFrequencies.size() > 0)
            notesToUse = harmonyFrequencies;
        else if (harmonyEngine != nullptr && harmonyEngine->isActive())
            notesToUse = lastHarmonyNotes;

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
}

void OpenVoxTunerAudioProcessor::applyLatencyMode()
{
    if (pitchShifter == nullptr)
        return;

    const int mode = (latencyModeParam != nullptr)
        ? juce::jlimit (0, 2, (int) std::round (latencyModeParam->load()))
        : 1;

    if (mode == appliedLatencyMode)
        return;

    const float latencyMs = (mode == 0) ? 12.0f : (mode == 1 ? 20.0f : 30.0f);
    pitchShifter->setLatencyMs (latencyMs);

    for (auto& ps : shiftedVoicePitchShifters)
    {
        if (ps != nullptr)
            ps->setLatencyMs (latencyMs);
    }

    setLatencySamples (pitchShifter->getLatencySamples());
    appliedLatencyMode = mode;

    const juce::String modeName = (mode == 0) ? "Low Latency" : (mode == 1 ? "Quality" : "Safe");
    OVT_LOG ("Latency mode changed: " + modeName +
             " (" + juce::String (latencyMs, 1) + " ms)");
}
 
// === Synchronisation des parametres utilisateur vers les modules DSP ===
void OpenVoxTunerAudioProcessor::syncParameters()
{
    if (keyParam == nullptr || scaleParam == nullptr)
        return;

    // Gamme musicale.
    const int keyIdx = static_cast<int> (keyParam->load());
    const int scaleIdx = static_cast<int> (scaleParam->load());
    scaleQuantizer->setKey (keyIdx);
    scaleQuantizer->setScale (static_cast<atdsp::Scale> (juce::jlimit (0, 15, scaleIdx)));

    // Si on est en mode "Custom" (scaleIdx == 15), on pousse la liste
    // des notes cochees vers le quantifier.
    if (scaleIdx == 15)
    {
        juce::Array<int> customNotes;
        for (int i = 0; i < 12; ++i)
        {
            if (customParam[i] != nullptr && customParam[i]->load() > 0.5f)
                customNotes.add (i);
        }
        scaleQuantizer->setCustomIntervals (customNotes);
    }

    // Vitesse de retargeting.
    if (speedParam != nullptr)
        retargetEnvelope->setSpeed (speedParam->load());
    
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

// === Detection de pitch sur le bloc courant via FIFO glissante ===
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

// === Pitch detector factory (YIN mode=0, SWIPE' mode=1) ===

std::unique_ptr<atdsp::IPitchDetector> OpenVoxTunerAudioProcessor::createDetector (int mode)
{
    if (mode == 1)
        return std::make_unique<atdsp::SwipePitchDetector>();
    return std::make_unique<atdsp::YinPitchDetector>();
}

// === Programmes (non utilises pour le MVP) ===
int OpenVoxTunerAudioProcessor::getNumPrograms()              { return 1; }
int OpenVoxTunerAudioProcessor::getCurrentProgram()           { return 0; }
void OpenVoxTunerAudioProcessor::setCurrentProgram (int)      {}
const juce::String OpenVoxTunerAudioProcessor::getProgramName (int) { return {}; }
void OpenVoxTunerAudioProcessor::changeProgramName (int, const juce::String&) {}

// === MIDI : plugin can produce MIDI OUT ===
bool OpenVoxTunerAudioProcessor::acceptsMidi() const  { return false; }
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
    if (layouts.getMainInputChannelSet()  != juce::AudioChannelSet::mono()
     && layouts.getMainInputChannelSet()  != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

// === Etat du plugin : serialisation XML des parametres + pitch curve ===
void OpenVoxTunerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    // Ajoute la pitch curve comme sous-element si disponible.
    if (pitchCurve != nullptr)
    {
        auto curveXml = pitchCurve->toXml();
        if (xml != nullptr && curveXml != nullptr)
            xml->addChildElement (curveXml.release());
    }
    copyXmlToBinary (*xml, destData);
}

void OpenVoxTunerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
    {
        parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
        // Restaure la pitch curve si présente dans le XML.
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
    (int key, atdsp::Scale scale)
{
    atdsp::ScaleQuantizer quantizer;
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

// === Creation du plugin (point d'entree JUCE) ===
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OpenVoxTunerAudioProcessor();
}
