// PluginProcessor.h
// Audio processor for the OpenVoxTuner plugin.
// Contains audio logic: DSP pipeline (pitch detection, quantization, shifting).
// The GUI is managed in PluginEditor.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include "dsp/PitchDetector.h"
#include "dsp/ScaleQuantizer.h"
#include "dsp/HarmonyEngine.h"
#include "dsp/PitchShifter.h"
#include "dsp/RetargetEnvelope.h"
#include "dsp/PitchCurve.h"

/**
 * Main class of the audio processor.
 * Inherits from juce::AudioProcessor (JUCE base class for audio plugins).
 * Responsibilities:
 *   - Receive audio buffers from the host
 *   - Execute the DSP pipeline on each block
 *   - Expose parameters to the host and GUI
 *   - Save/load the plugin state
 */
class OpenVoxTunerAudioProcessor : public juce::AudioProcessor,
                                    public juce::AudioProcessorARAExtension
{
public:
    // === Construction / destruction ===
    OpenVoxTunerAudioProcessor();
    ~OpenVoxTunerAudioProcessor() override;

    // === Plugin configuration (before prepareToPlay) ===
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    // === Main audio routine: called for each audio block ===
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // === Plugin information (editable in Projucer) ===
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // === Programs / presets (not implemented for MVP) ===
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    // === Plugin state (save / load) ===
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // === Audio bus layout (inputs / outputs) ===
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // === Latency (reported to host) ===
    // Note: in JUCE 8, getLatencySamples() is NOT virtual, so we can't
    // use 'override'. We use setLatencySamples() to set it.
    int getLatencySamples() const { return latencySamples; }

    // === Editor access (GUI) ===
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // === Pitch data access for GUI ===
    float getCurrentInputPitch() const  { return lastInputPitch.load(); }
    float getCurrentOutputPitch() const { return lastOutputPitch.load(); }
    float getCurrentCentsOffset() const { return lastCentsOffset.load(); }

    // === Harmony data access for GUI ===
    const juce::Array<float>& getHarmonyFrequencies() const { return harmonyFrequencies; }
    float getHarmonyOutputLevel() const { return harmonyOutputLevel.load(); }

    // === Scale note names (for the editor) ===
    static juce::Array<juce::String> getScaleNoteNames (int key, atdsp::Scale scale);

    // Pitch curve access (for the editor).
    atdsp::PitchCurve& getPitchCurve() { return *pitchCurve; }
    const atdsp::PitchCurve& getPitchCurve() const { return *pitchCurve; }

    // Access sent note for UI
    int getLastSentMidiNoteForChannel (int channel) const;

    // Sets the transport time (from the host).
    void setTransportTime (double seconds) { transportTime.store (seconds); }
    double getTransportTime() const { return transportTime.load(); }
    bool isTimeProvidedByHost() const { return timeProvidedByHost.load(); }
    
    // Checks if the plugin is bound to the ARA environment (via ARADocumentController)
    bool isBoundToARA_custom() const { return isBoundToARA(); }
    bool isStandaloneWrapper() const { return wrapperType == juce::AudioProcessor::wrapperType_Standalone; }

    // Parameter tree access (for the editor).
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    // VST3 Extension (Micro View, etc.)
    juce::VST3ClientExtensions* getVST3ClientExtensions() override;

    // Expose envelope control for UI debug
    void setHarmonyEnvelopeTimes (float attackMs, float releaseMs);
    // Force-clear harmony internal cached data (for debug)
    void clearHarmonyCache();
    // Dump VST3 bundle info (debug)
    void dumpVST3BundleInfo();
    // Force-create a test grain in the pitch shifter (debug)
    void forceCreatePitchTestGrain();


    void resetTransportTime() { 
        customTimeOffset.store(rawHostTime.load());
        transportTime.store(0.0); 
    }

private:
    // === User parameters (JUCE parameter tree) ===
    // Uses AudioProcessorValueTreeState to expose parameters
    // to the host (automation) and GUI in a synchronized way.
    juce::AudioProcessorValueTreeState parameters;

    // Parameter targets (sliders, knobs, etc.).
    std::atomic<float>* speedParam   = nullptr; // Correction speed (ms)
    std::atomic<float>* amountParam  = nullptr; // Intensity (0-1)
    std::atomic<float>* latencyModeParam = nullptr; // 0=Low Latency, 1=Quality
    std::atomic<float>* formantParam = nullptr; // Formant shift (-12 to 12)
    std::atomic<float>* formantEnableParam = nullptr; // Formant On/Off
    std::atomic<float>* keyParam     = nullptr; // Tonic index (0-11)
    std::atomic<float>* scaleParam   = nullptr; // Mode index (0-5, 5=custom)
    std::atomic<float>* bypassParam  = nullptr; // Bypass on/off
    std::atomic<float>* modeParam    = nullptr; // Auto/graphic mode
    std::atomic<float>* engineParam  = nullptr; // Audio engine (0=RubberBand, 1=SoundTouch, 2=PSOLA)
    std::atomic<float>* harmonyTypeParam = nullptr; // Harmony type (0-9)
    std::atomic<float>* harmonyGainParam = nullptr; // Harmony volume level
    std::atomic<float>* harmonyBlendParam = nullptr; // Harmony blend (0-1)
    std::atomic<float>* harmonyEnableParam = nullptr; // Harmony on/off
    std::atomic<float>* harmonyUseVoiceParam = nullptr; // Use real voice for harmony (bool)
    std::atomic<float>* harmonyShiftedVoicesParam = nullptr; // number of shifted voices (1..4)
    std::atomic<float>* harmonyToneParam = nullptr; // synth harmony tone (choice)
    std::atomic<float>* harmonyToneColorParam = nullptr; // synth harmony tone color (continuous)
    std::atomic<float>* midiOutEnableParam = nullptr; // MIDI out enable

    // "Custom note on/off" parameters (12 booleans, indices 0..11).
    // Stored as 12 separate AudioParameterBool so the host can
    // automate them individually.
    std::atomic<float>* customParam[12] = { nullptr };

    // === DSP Modules (Phase 1 + 4) ===
    std::unique_ptr<atdsp::PitchDetector>     pitchDetector;
    std::unique_ptr<atdsp::ScaleQuantizer>    scaleQuantizer;
    std::unique_ptr<atdsp::PitchShifter>      pitchShifter;
    std::unique_ptr<atdsp::HarmonyEngine>     harmonyEngine;
    
    std::unique_ptr<atdsp::RetargetEnvelope>  retargetEnvelope;
    std::unique_ptr<atdsp::PitchCurve>        pitchCurve; // "graphic" mode

    // Current correction mode.
    //   0 = auto (standard quantization)
    //   1 = graphic (follows drawn pitch curve)
    std::atomic<int> correctionMode { 0 };

    // Transport time in seconds (to evaluate pitch curve).
    std::atomic<double> transportTime { 0.0 };
    std::atomic<bool> timeProvidedByHost { false };
    std::atomic<double> rawHostTime { 0.0 };
    std::atomic<double> customTimeOffset { 0.0 };

    // Current sample rate.
    double currentSampleRate = 44100.0;

    // Latency in samples (reported to host).
    int latencySamples = 0;

    // Current pitch (for the GUI).
    std::atomic<float> lastInputPitch  { 0.0f };
    std::atomic<float> lastOutputPitch { 0.0f };

    // Cents offset between input pitch and quantized pitch.
    // Positive = input note too high, Negative = too low.
    std::atomic<float> lastCentsOffset { 0.0f };

    std::atomic<float> lastValidF0 { 0.0f };

    // Harmony state.
    juce::Array<float> harmonyFrequencies;
    juce::Array<float> lastHarmonyNotes; // keep last notes to allow release rendering
    std::atomic<float> harmonyOutputLevel { 0.0f };
    juce::AudioBuffer<float> harmonyBuffer; // stereo buffer for mixing
    juce::AudioBuffer<float> synthWorkBuffer; // preallocated synth harmony render buffer
    // last mixed harmony buffer saved to perform a crossfade on stop
    juce::AudioBuffer<float> lastMixedHarmonyBuffer;
    bool wasHarmonyActiveLastBlock { false };
    // Temporary buffers to hold pitch-shifted voices (preallocated)
    std::vector<juce::AudioBuffer<float>> shiftedVoiceBuffers;
    // Dedicated pitch shifters per shifted voice (separate state from main pitchShifter)
    std::vector<std::unique_ptr<atdsp::PitchShifter>> shiftedVoicePitchShifters;
    static constexpr int maxShiftedVoices = 4;
    std::array<juce::LinearSmoothedValue<float>, maxShiftedVoices> shiftedVoiceGains;
    // MIDI out state (per-channel last note sent, channels 1..16 mapped to index 0..15)
    int lastSentMidiNote[16] = { -1 };

    // Debug: remember previous bypass state to log changes
    std::atomic<int> prevBypassState { 0 };
    // Debug: observe grains created by PitchShifter (cross-thread counter)
    int lastObservedGrainCount = 0;
    // Debug: test grain parameter
    std::atomic<float>* dbgTestGrainParam = nullptr;
    std::atomic<int> prevDbgTestGrain { 0 };
    // Debug: throttle frequent logs (ms)
    std::atomic<uint32_t> lastProcessLogTime { 0 };
    std::atomic<uint32_t> lastHarmonyLogTime { 0 };
    int appliedLatencyMode = -1;

    // Silence counter for Sleep mode (CPU saving).
    int silenceSamples = 0;
    int maxSilenceSamples = 44100; // Default 1s at 44.1kHz
    bool harmonyInputGateOpen = false; // hysteresis gate to avoid buzz/click chattering near silence

    // FIFO to accumulate samples for pitch detection.
    // Necessary because the analysis window is larger than the block.
    static constexpr int analysisWindow = 2048;
    static constexpr int analysisHopSize = 1024; // Calculate YIN every ~23ms to halve CPU load
    int samplesSinceLastAnalysis = 0;
    juce::AudioBuffer<float> analysisFifo;
    int fifoWriteIndex = 0;
    int fifoFillCount = 0;

    // PRE-ALLOCATED linear buffer to un-FIFO before YIN detection.
    // Allocated in prepareToPlay() to `analysisWindow` floats. Avoids
    // heap allocation per audio block (otherwise 2.4 MB/s allocations at
    // 144 samples, major source of glitches).
    juce::HeapBlock<float> analysisLinearBuffer;

    // Updates DSP parameters from the value tree.
    void syncParameters();
    void applyLatencyMode();

    // VST3 Extensions (e.g., Micro View for Studio One)
    std::unique_ptr<juce::VST3ClientExtensions> vst3Extensions;

    // Calculates the input pitch on the current block, returns freq in Hz.
    float computeInputPitch (const juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenVoxTunerAudioProcessor)
};
