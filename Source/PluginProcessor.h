// PluginProcessor.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include <thread>
#include "dsp/IPitchDetector.h"
#include "dsp/YinPitchDetector.h"
#include "dsp/ScaleQuantizer.h"
#include "dsp/HarmonyEngine.h"
#include "dsp/PitchShifter.h"
#include "dsp/RetargetEnvelope.h"
#include "dsp/PitchCurve.h"
#include "dsp/IEffect.h"
#include "dsp/NoiseGate.h"
#include "dsp/UpwardCompressor.h"
#include "dsp/PresetMorpher.h"
#include "dsp/VibratoPreserver.h"
#include "dsp/AttackAwareEnv.h"
#include "dsp/BlockAwareOnePole.h"
#include "dsp/KeyDetector.h"
#include "dsp/KeyBridge.h"
#include "dsp/SidechainBusLayout.h"
#include "dsp/FormantPreserver.h"
#include "dsp/LpcFormantPreserver.h"

/**
 * Main class of the audio processor.
 * Inherits from juce::AudioProcessor (JUCE base class for audio plugins).      
 * Responsibilities:
 *   - Receive audio buffers from the host
 *   - Execute the DSP pipeline on each block
 *   - Expose parameters to the host and GUI
 *   - Save/load the plugin state
 */
class OpenVoxTunerAudioProcessor : public juce::AudioProcessor
#if OVT_ARA_ENABLED
                                   , public juce::AudioProcessorARAExtension
#endif
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
    void reset() override;  

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
    float getLastDetectedInputPitch() const { return lastRawYinPitch.load (std::memory_order_relaxed); }

    // True while "Tuning follows MIDI IN" is enabled AND a MIDI note is being
    // held to drive the correction target. Used by the GUI to show a badge.
    bool isMidiTargetActive() const { return midiTargetActive.load(); }

    // Frequency (Hz) of the MIDI note currently driving the correction target
    // when isMidiTargetActive() is true; 0 otherwise. Used by the Curve Editor
    // to draw a target line.
    float getCurrentMidiTargetHz() const { return midiTargetHz.load(); }

    // === Harmony data access for GUI ===
    void copyHarmonyFrequencies (juce::Array<float>& destination) const;
    // Scale-locked harmony notes (Follow Lead NOT applied). Used for MIDI OUT so
    // pushed MIDI notes stay clean, regardless of the Follow Lead toggle.
    void copyHarmonyFrequenciesClean (juce::Array<float>& destination) const;
    float getHarmonyOutputLevel() const { return harmonyOutputLevel.load(); }   

    // === Scale note names (for the editor) ===
    static juce::Array<juce::String> getScaleNoteNames (int key, ovtdsp::Scale scale);

    // Pitch curve access (for the editor).
    ovtdsp::PitchCurve& getPitchCurve() { return *pitchCurve; }
    const ovtdsp::PitchCurve& getPitchCurve() const { return *pitchCurve; }
    bool hasPitchCurve() const { return pitchCurve != nullptr; }      

    // Access sent note for UI
    int getLastSentMidiNoteForChannel (int channel) const;

    // Sets the transport time (from the host).
    void setTransportTime (double seconds) { transportTime.store (seconds); }   
    double getTransportTime() const { return transportTime.load(); }
    bool getIsPlaying() const { return hostIsPlaying.load() != 0; }

    // Standalone transport: when stopped, the timeline (and the curve editor playhead /
    // auto-scroll) freezes so the user can edit the curve. In a DAW the host owns transport.
    void setTransportPlaying (bool p) { transportPlaying.store (p); }
    bool isTransportPlaying() const { return transportPlaying.load(); }

    // Standalone tempo (BPM). In a DAW the tempo comes from the host time signature.
    void setBpm (float b) { bpm.store (juce::jlimit (20.0f, 400.0f, b)); }
    float getBpm() const { return bpm.load(); }
    bool isTimeProvidedByHost() const { return timeProvidedByHost.load(); }
    std::atomic<bool>& getPendingCurveRestore() { return pendingCurveRestore; }

    // Safe typed accessors for key/scale indices (avoids fragile raw-value arithmetic
    // that breaks after setStateInformation in AU).
    int getKeyIndex() const   { return keyIntParam    != nullptr ? keyIntParam->get()       : static_cast<int> (std::round (keyParam->load() * 11.0f)); }
    int getScaleIndex() const { return scaleChoiceParam != nullptr ? scaleChoiceParam->getIndex() : static_cast<int> (std::round (scaleParam->load() * 13.0f)); }

    // Checks if the plugin is bound to the ARA environment (via ARADocumentController)
#if OVT_ARA_ENABLED
    bool isBoundToARA_custom() const { return isBoundToARA(); }
#else
    bool isBoundToARA_custom() const { return false; }
#endif
    bool isStandaloneWrapper() const { return wrapperType == juce::AudioProcessor::wrapperType_Standalone; }

    // Curve Editor playhead loop mode.
    //  - ARA:                follows the host timeline (no internal loop).
    //  - Standalone:         always loops within the Measures window.
    //  - Plugin (VST3/AU):   user choice via the editor_playhead_loop parameter.
    bool isPlayheadLooping() const;
    double getLoopLengthBeats() const;
    double getLoopTransportTime() const;
    bool getPlayheadLoop() const { return editorPlayheadLoopParam != nullptr && editorPlayheadLoopParam->load() > 0.5f; }
    void setPlayheadLoop (bool on) { if (editorPlayheadLoopParam) const_cast<std::atomic<float>*>(editorPlayheadLoopParam)->store (on ? 1.0f : 0.0f); }

    // Parameter tree access (for the editor).
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

    // === Global plugin Undo/Redo (Option 1) ===
    // A single history covering every automatable parameter (the full
    // AudioProcessorValueTreeState). The Curve Editor keeps its own
    // curve-point undo; everything else (sliders, toggles, combos, presets,
    // scale changes) is recorded here. Snapshots are ValueTree copies, so a
    // single UndoManager transaction restores the whole plugin state.
    juce::UndoManager& getUndoManager() { return pluginUndoManager; }

    /// Push one undoable transaction: restore `before` on Undo, `after` on Redo.
    /// `before`/`after` are full ValueTree copies of `parameters.state`
    /// (see AudioProcessorValueTreeState::copyState()). Both states are
    /// captured by the editor at gesture boundaries; this method only builds
    /// the action and performs it.
    void pushUndoAction (const juce::ValueTree& before, const juce::ValueTree& after);

    // === Plugin Presets (separate from Curve Presets) ===
    // A Plugin Preset captures the full plugin parameter state (the
    // AudioProcessorValueTreeState, i.e. every automatable parameter) PLUS a
    // few UI-only preferences, but NEVER the pitch curve (the curve lives
    // outside parameters.state and is managed by the Curve Presets system).
    // This separation keeps the two preset kinds orthogonal: loading a Plugin
    // Preset never touches the curve, and vice-versa.

    /// The factory-default parameter state (captured once at construction,
    /// before any user/DAW change). Used by the "Default" factory preset.
    const juce::ValueTree& getDefaultPluginState() const { return defaultPluginState; }

    /// A deep copy of the current plugin parameter state (no curve). This is
    /// what a "save preset" captures.
    juce::ValueTree getPluginPresetState() const { return const_cast<juce::AudioProcessorValueTreeState&>(parameters).copyState(); }

    /// Apply a plugin preset state. Restores parameters.state via
    /// replaceState() wrapped in one global-undo transaction, so loading a
    /// preset is undoable (Ctrl/Cmd+Z). User/session preferences that must
    /// NOT be overridden by a preset (UI language, theme, morph position,
    /// Live/Curve mode) are preserved afterwards.
    void applyPluginPresetState (const juce::ValueTree& presetState);

    /// Copy the current scale intervals published by the audio thread.
    /// The destination is a private UI-thread snapshot and never aliases the
    /// ScaleQuantizer's mutable Array.
    void copyScaleIntervals (juce::Array<int>& destination) const;

    // VST3 Extension (Micro View, etc.)
    juce::VST3ClientExtensions* getVST3ClientExtensions() override;

    // Enable/disable waveform capture in the audio thread. When the waveform
    // overlay is hidden in the UI, this avoids unnecessary lock contention.
    void setWaveformCaptureEnabled (bool enabled) { waveformCaptureEnabled.store (enabled); }
    bool isWaveformCaptureEnabled() const { return waveformCaptureEnabled.load(); }

    // --- Deferred parameter changes (audio â†’ UI thread) ---
    // Called from the UI thread (editor timerCallback) to apply key/scale
    // changes that were detected on the audio thread.  Using
    // setValueNotifyingHost() from the audio thread can deadlock certain
    // hosts (Cubase, Live VST3) when combined with ARA locks, so we defer
    // the actual parameter update to the UI thread.
    void flushPendingParameterChanges();

    // Read host transport (playhead position, playing state, time signature).
    // MUST be called from the UI thread only â€” getPlayHead()->getPosition()
    // can deadlock Cubase and Live VST3 when called from the audio thread.
    void updateHostTransport();

    // Reads getPlayHead()->getPosition() and refreshes cachedHostPpq /
    // hostIsPlaying / time-signature atomics.  Safe to call from any
    // thread provided the caller has already verified that getPlayHead()
    // is reachable (auReady, !editorShuttingDown, !isStandalone).  All
    // exceptions are swallowed â€” on macOS a torn-down AU can raise an
    // Objective-C++ exception that we never want to propagate out of
    // an audio callback.
    void readAndCachePlayHeadInfo (juce::AudioPlayHead& playHead) noexcept;

    // Editor lifecycle helpers.  The editor sets the shutting-down flag
    // at the very start of its destructor (BEFORE stopTimer()) so that
    // any MessageQueue timer callback still in flight bails out of
    // updateHostTransport() before touching getPlayHead().  Without this
    // guard, Live 12 (AU) crashes inside JuceAU::ScopedPlayHead::
    // getPosition() with a null dereference when the plugin window
    // closes or the device is reconfigured.
    void notifyEditorShuttingDown() noexcept
    {
        editorShuttingDown.store (true, std::memory_order_release);
    }

    bool isEditorShuttingDown() const noexcept
    {
        return editorShuttingDown.load (std::memory_order_acquire);
    }

    // Called by the editor's constructor to clear the shutting-down
    // flag set by the previous editor instance's destructor.  Required
    // because the processor outlives the editor and the flag would
    // otherwise stay stuck at `true` forever after the first close.
    void notifyEditorResumed() noexcept
    {
        editorShuttingDown.store (false, std::memory_order_release);
    }

    // AU-host lifecycle helpers.  getPlayHead()->getPosition() from the
    // UI thread is unsafe on AU: JuceAU::ScopedPlayHead dereferences an
    // internal pointer (to the JuceAU instance) that can be invalidated
    // at any time by the host (device change, sample-rate change,
    // project reload, â€¦).  The resulting SIGSEGV is a Mach exception
    // and CANNOT be caught by try/catch.  We therefore gate every call
    // to getPlayHead() on `auReady`, which is true only between a
    // prepareToPlay() and the next releaseResources() (and false again
    // on destruction).  For AU we additionally skip the call entirely
    // â€” see updateHostTransport().
    void notifyAuReady() noexcept
    {
        auReady.store (true, std::memory_order_release);
    }

    void notifyAuReleased() noexcept
    {
        auReady.store (false, std::memory_order_release);
    }

    bool isAuReady() const noexcept
    {
        return auReady.load (std::memory_order_acquire);
    }

    // Dedicated worker thread that calls getPlayHead()->getPosition() off
    // the UI/audio threads.  Some hosts (Cubase LE 15, Live 12 VST3) busy-
    // loop or deadlock inside this call, which would otherwise saturate
    // the message thread and freeze the host.  The thread is started in
    // prepareToPlay() and joined in releaseResources()/the destructor.
    // Its sole job is to feed the cachedHostPpq / hostIsPlaying / time-
    // signature atomics via readAndCachePlayHeadInfo().
    void startPlayheadThread();
    void stopPlayheadThread() noexcept;

    // Read ARA metadata (key signatures, bar signatures) from the host.
    // MUST be called from the UI thread only (HostContentReader acquires a
    // lock that can deadlock the audio thread in some hosts).
    void updateAraMetadata();

    // Expose envelope control for UI debug
    void setHarmonyEnvelopeTimes (float attackMs, float releaseMs);
    // Force-clear harmony internal cached data (for debug)
    void clearHarmonyCache();
    // Dump VST3 bundle info (debug)
    void dumpVST3BundleInfo();
    // Force-create a test grain in the pitch shifter (debug)
    void forceCreatePitchTestGrain();

    // Time signature access (for the Curve Editor ruler).
    int getCurrentTimeSigNumerator() const { return currentTimeSigNumerator.load(); }
    int getCurrentTimeSigDenominator() const { return currentTimeSigDenominator.load(); }

    // Waveform display type preference (persisted via APVTS).
    int getWaveformDisplayType() const
    {
        if (auto* p = parameters.getParameter ("ui_waveform_type"))
            return juce::jlimit (0, 2, juce::roundToInt (p->getValue() * 2.0f));
        return 1;
    }
    void setWaveformDisplayType (int type)
    {
        if (auto* p = parameters.getParameter ("ui_waveform_type"))
            p->setValueNotifyingHost ((float) juce::jlimit (0, 2, type) / 2.0f);
    }

    // Correction block "Advanced" expand/collapse state (persisted across sessions).
    bool getAdvancedExpanded() const { return advancedExpandedState; }
    void setAdvancedExpanded (bool expanded) { advancedExpandedState = expanded; }

    // Morph slider amount accessors (backed by the automatable "morph_amount" parameter).
    float getMorphAmount() const { return morphAmountParam != nullptr ? morphAmountParam->load (std::memory_order_relaxed) : 0.0f; }
    void setMorphAmount (float v);

    // A/B slot persistence (called by Editor during state save/load).
    void setAbSlotMorphState (int slot, ovtdsp::MorphState ms)
    {
        if (slot == 0) abSlotAMorph = std::move (ms);
        else           abSlotBMorph = std::move (ms);
        abSlotHasData[slot] = true;
    }
    const ovtdsp::MorphState* getAbSlotMorphState (int slot) const
    {
        return abSlotHasData[slot] ? (slot == 0 ? &abSlotAMorph : &abSlotBMorph) : nullptr;
    }
    bool hasAbSlotData (int slot) const { return abSlotHasData[slot]; }
    // Active A/B slot (0 = A, 1 = B), persisted so the editor restarts on the
    // same slot the user left. Without this, the editor resets to "A" while the
    // restored main state reflects the last active slot (often B), which makes
    // slot A appear to contain slot B's content after a restart.
    void setAbActiveSlot (int s) { abActiveSlot = juce::jlimit (0, 1, s); }
    int  getAbActiveSlot() const { return abActiveSlot; }

    // CPU usage meter (0.0 - 1.0) for the editor header display.
    float getCpuUsage() const { return cpuUsage.load(); }

    void getTimeSignatureAt(double ppq, int& num, int& den) const;

    void resetTransportTime() {
        customTimeOffset.store(rawHostTime.load());
        transportTime.store(0.0);
    }

    // Returns the host PPQ position extrapolated from the last worker
    // update to "now".  The worker refreshes cachedHostPpq at 30 Hz, but
    // the editor needs a smooth 60 fps playhead, so we add
    // (now - lastUpdate) * (bpm / 60) to the last known PPQ.  When the
    // host reports !isPlaying we return the last known PPQ unchanged
    // so the playhead freezes.  All operations are atomic and lock-free
    // so this is safe to call from the UI thread (timerCallback).
    double getInterpolatedTransportTime() const;

    // Seeks the transport to an absolute time (ruler-click / programmatic seek).
    // Updates both the standalone clock and the DAW host-time offset so the displayed
    // playhead jumps to `t` in every context.
    void seekToTime (double t) {
        transportTime.store (t);
        customTimeOffset.store (rawHostTime.load() - t);
    }

    // ARA2 waveform overlay accessors (for the editor).
    bool isAraWaveformReady() const { return araWaveformReady.load (std::memory_order_acquire); }
    /// Copy the cached waveform into the provided buffer (thread-safe).
    void copyAraWaveform (juce::AudioBuffer<float>& dest, double& sr);

private:
    // === User parameters (JUCE parameter tree) ===
    // Uses AudioProcessorValueTreeState to expose parameters
    // to the host (automation) and GUI in a synchronized way.
    juce::AudioProcessorValueTreeState parameters;

    // Global plugin Undo/Redo history (Option 1). Owns the ValueTree
    // snapshots pushed by the editor at gesture boundaries.
    juce::UndoManager pluginUndoManager;

    // Factory-default parameter state, captured once at construction (before
    // any user/DAW/setStateInformation change). Backs the "Default" Plugin
    // Preset. Stored as a deep copy so callers never mutate it.
    juce::ValueTree defaultPluginState;

    // Parameter targets (sliders, knobs, etc.).
    std::atomic<float>* speedParam   = nullptr; // Correction speed (ms)        
    std::atomic<float>* amountParam  = nullptr; // Intensity (0-1)
    std::atomic<float>* latencyModeParam = nullptr; // 0=Low Latency, 1=Quality 
    std::atomic<float>* formantParam = nullptr; // Formant shift (-12 to 12)    
    std::atomic<float>* formantEnableParam = nullptr; // Formant On/Off
    std::atomic<float>* formantModeParam = nullptr; // Formant mode: 0=Legacy, 1=MultiFormant
    std::atomic<float>* formantStrategyParam = nullptr; // Formant strategy: 0=Current,1=P0,2=P1,3=P2
    std::atomic<float>* keyParam     = nullptr; // Tonic index (0-11)
    std::atomic<float>* scaleParam   = nullptr; // Mode index (0-5, 5=custom)
    juce::AudioParameterChoice* scaleChoiceParam = nullptr; // Direct accessor for getIndex()
    juce::AudioParameterInt* keyIntParam = nullptr; // Direct accessor for get()   
    std::atomic<float>* bypassParam  = nullptr; // Bypass on/off
    std::atomic<float>* modeParam    = nullptr; // Auto/graphic mode
    std::atomic<float>* engineParam  = nullptr; // Audio engine (0=RubberBand, 1=SoundTouch, 2=PSOLA)
    std::atomic<float>* harmonyTypeParam = nullptr; // Harmony type (0-9)       
    std::atomic<float>* harmonyGainParam = nullptr; // Harmony volume level     
    std::atomic<float>* harmonyBlendParam = nullptr; // Harmony blend (0-1)     
    std::atomic<float>* harmonyAttackParam = nullptr; // Harmony attack (ms) per-voice fade-in 
    std::atomic<float>* harmonyEnableParam = nullptr; // Harmony on/off
    std::atomic<float>* harmonyUseVoiceParam = nullptr; // Use real voice for harmony (bool)
    std::atomic<float>* harmonyShiftedVoicesParam = nullptr; // number of shifted voices (1..4)
    std::atomic<float>* harmonyToneParam = nullptr; // synth harmony tone (choice)
    std::atomic<float>* harmonyToneColorParam = nullptr; // synth harmony tone color (continuous)
    std::atomic<float>* harmonyFollowLeadParam = nullptr; // harmonies follow lead character (bool)
    std::atomic<float>* harmonyGainMatchParam = nullptr; // gain match on/off: scale harmony by 1/sqrt(1+N) to keep total RMS ~ dry
    std::atomic<float>* harmonyFormantParam = nullptr; // Harmony formant shift (-5 to +5 st)
    std::atomic<float>* midiOutEnableParam = nullptr; // MIDI out enable
    std::atomic<float>* midiTargetEnableParam = nullptr; // MIDI target / follow enable
    std::atomic<float>* editorMeasuresParam = nullptr; // Curve Editor measures (1-8)        
    std::atomic<float>* editorPlayheadLoopParam = nullptr; // Curve Editor playhead loop (0=follow host, 1=loop on Measures)
    std::atomic<float>* reverbEnableParam = nullptr; // Reverb on/off
    std::atomic<float>* reverbMixParam = nullptr;   // Reverb wet mix (0-1)
    std::atomic<float>* noiseGateEnableParam = nullptr;    // Noise gate on/off
    std::atomic<float>* noiseGateThresholdParam = nullptr; // Noise gate threshold (dB)
    std::atomic<float>* upwardCompEnableParam = nullptr;   // Upward compressor on/off
    std::atomic<float>* upwardCompAmountParam = nullptr;   // Upward compressor amount (0-1)
    std::atomic<float>* flexTuneParam = nullptr;    // FlexTune deadband (0-100 cents)
    std::atomic<float>* humanizeParam = nullptr;    // Humanize random cents (0-50)
    std::atomic<float>* correctionModeParam = nullptr; // 0=Modern, 1=Transparent        
    std::atomic<float>* vibratoPreserveParam = nullptr; // Vibrato preservation (0-100%)
    std::atomic<float>* attackAwareParam = nullptr;    // Attack-aware correction enable (bool)
    std::atomic<float>* attackReleaseParam = nullptr;  // Attack-aware release time (ms)

    // Key/Scale detection master switch: false = manual key/scale (user drives it),
    // true = detected automatically via the source below. Replaces the old "Manual"
    // choice. Default false (off).
    std::atomic<float>* keyDetectParam = nullptr;
    // Key detection source (used only when keyDetectParam is on):
    // 0=Auto (in-plugin audio analysis), 1=OpenVoxKey (shared bridge from the
    // OpenVoxKey companion detector), 2=Sidechain (analysis of the sidechain input).
    std::atomic<float>* keySourceParam = nullptr;
    std::atomic<float>* companionGroupParam = nullptr; // Companion group (A/B/C/D)

    // "Custom note on/off" parameters (12 booleans, indices 0..11).
    // Stored as 12 separate AudioParameterBool so the host can
    // automate them individually.
    std::atomic<float>* customParam[12] = { nullptr };

    // Pitch detector selection parameter (0=YIN).
    std::atomic<float>* detectorParam = nullptr;

    // Voice Type parameter (0=Universal, 1=Bass, 2=Baritone, 3=Tenor, 4=Alto, 5=Soprano).
    std::atomic<float>* voiceTypeParam = nullptr;

    // Voice Type frequency ranges (Hz). Index matches voice_type parameter:
    // 0=Universal(30-1000), 1=Bass(82.41-329.63), 2=Baritone(110-440),
    // 3=Tenor(130.81-523.25), 4=Alto(174.61-698.46), 5=Soprano(261.63-1046.50)
    static constexpr std::array<float, 6> voiceTypeMinHz = {30.0f, 82.41f, 110.0f, 130.81f, 174.61f, 261.63f};
    static constexpr std::array<float, 6> voiceTypeMaxHz = {1000.0f, 329.63f, 440.0f, 523.25f, 698.46f, 1046.50f};
    int lastVoiceType = 0;

    // === DSP Modules (Phase 1 + 4) ===
    // YIN pitch detector.
    std::unique_ptr<ovtdsp::IPitchDetector> pitchDetectors[1];
    std::atomic<ovtdsp::IPitchDetector*>    activePitchDetector { nullptr };
    int activeDetectorMode = 0;
    std::unique_ptr<ovtdsp::IPitchDetector> createDetector();
    std::unique_ptr<ovtdsp::ScaleQuantizer>    scaleQuantizer;
    std::array<std::atomic<int>, 12> scaleIntervalSnapshot {};
    std::atomic<int> scaleIntervalSnapshotSize { 0 };
    std::unique_ptr<ovtdsp::PitchShifter>      pitchShifter;
    std::unique_ptr<ovtdsp::HarmonyEngine>     harmonyEngine;
    ovtdsp::NoiseGate                           noiseGate;
    ovtdsp::FormantPreserver                    formantPreserver;
    ovtdsp::FormantPreserver                    formantPreserverHarmony;
    // LPC cross-synthesis formant preservation (P1 = C0, P2 = C1Hybrid).
    // One instance for the lead voice (the per-harmony array is declared
    // further below, after maxShiftedVoices, which it is sized by).
    ovtdsp::LpcFormantPreserver                 lpcFormantPreserverLead;
    ovtdsp::UpwardCompressor                    upwardComp;

    std::unique_ptr<ovtdsp::RetargetEnvelope>  retargetEnvelope;
    std::unique_ptr<ovtdsp::PitchCurve>        pitchCurve; // "graphic" mode

    // Post-processing effects (reverb, delay, chorus, etc.).
    // Applied in order after the main pitch-correction + harmony pipeline.
    std::vector<std::unique_ptr<ovtdsp::IEffect>> effects;     

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

    // True while a held MIDI note is actively driving the correction target
    // ("Tuning follows MIDI IN"). Written in processBlock, read by the GUI.
    std::atomic<bool> midiTargetActive { false };

    // Frequency (Hz) of the MIDI note currently driving the target (see getter).
    std::atomic<float> midiTargetHz { 0.0f };

    // A/B comparison slots (persisted in project state).
    ovtdsp::MorphState abSlotAMorph;
    ovtdsp::MorphState abSlotBMorph;
    bool abSlotHasData[2] = { false, false };
    int  abActiveSlot = 0;

    std::atomic<float> lastValidF0 { 440.0f };

    // Last valid pitch snapshot used by autotune to keep the pitch
    // shifter's ratio coherent during transient YIN dropouts
    // (anti-octave-error too aggressive). Updated only when a non-zero
    // pitch is detected. Avoids the "no audible effect" regression.
    std::atomic<float> lastValidPitchForAutotune { 0.0f };

    // Dernier pitch retourne par YIN (0 compris). Utilise par le filtre
    // anti-saut-octave. Separe de lastValidPitchForAutotune qui ne descend
    // jamais a 0 (fallback) et empecherait le reset du filtre.
    std::atomic<float> lastRawYinPitch { 0.0f };

    // Last non-trivial pitch ratio passed to the PitchShifter. Used as
    // a fallback when YIN drops to 0 to avoid the autotune effect
    // collapsing to a 1.0 ratio (pass-through) for a few blocks.
    std::atomic<float> lastRatioSnapshot { 1.0f };

    // Dernier pitch valide apres filtrage anti-saut-d'octave.
    // Si le pitch detecte saute d'un facteur ~2 ou ~0.5 par rapport a
    // cette reference, on conserve l'ancienne valeur. Un compteur de
    // persistence permet de laisser passer les vrais changements de
    // registre vocal apres ~140 ms de detection stable.
    std::atomic<float> lastOctaveValidatedPitch { 0.0f };
    // Compteur de cycles consecutifs ou le filtre a rejete un saut
    // d'octave. Apres OCTAVE_JUMP_PERSISTENCE_THRESHOLD rejets, on
    // accepte le nouveau pitch (vrai changement de registre).
    // 3 rejets ~= 140ms a 44.1kHz (hop=2048 echantillons).
    static constexpr int octaveJumpPersistenceThreshold = 3;
    int octaveJumpRejectionCount = 0;

    // Cached host transport info refreshed from the UI thread
    // (updateHostTransport) to avoid calling getPlayHead() from the audio
    // thread, which can deadlock Cubase and Live VST3.
    std::atomic<double> cachedHostPpq { 0.0 };       // last known host PPQ
    std::atomic<bool>   hostProvidesTimeCached { false }; // host gives transport?
    // True host playback state (1 = playing, 0 = stopped).
    // Propagated to the UI to avoid delta-based heuristics.
    std::atomic<int> hostIsPlaying { 0 };
    // Interpolation support: the editor runs at 30 Hz but renders at 60 fps,
    // so the playhead position must be extrapolated between worker updates
    // to look smooth.  These two atomics capture the PPQ reported by the
    // worker along with the wall-clock time (ms since epoch) at which it
    // was captured.  The editor reads them in its timerCallback and
    // extrapolates using the current bpm.
    std::atomic<double> cachedHostPpqAtUpdateMs { 0.0 }; // juce::Time::getMillisecondCounterHiRes() when cachedHostPpq was last refreshed
    std::atomic<double> cachedHostPpqAtUpdate { 0.0 };    // value of cachedHostPpq at that moment (pre-offset)
    // Standalone-only transport state (see setTransportPlaying / setBpm in the public API).
    std::atomic<bool> transportPlaying { true };
    std::atomic<float> bpm { 120.0f };
    // Flag set when setStateInformation restores a pitch curve that the
    // editor needs to pick up. The processor always outlives the editor,
    // so this works regardless of createEditor() ordering.
    std::atomic<bool> pendingCurveRestore { false };

    // Set to true by the editor's destructor BEFORE stopTimer() to prevent
    // a queued MessageQueue timer callback from invoking getPlayHead()
    // after the AU's internal state has been torn down.  Without this
    // guard, Live 12 (AU) can crash inside JuceAU::ScopedPlayHead::
    // getPosition() with a null pointer dereference when the plugin
    // window is closing or the device is being reconfigured.
    std::atomic<bool> editorShuttingDown { false };

    // Tracks whether the audio side is currently in a stable state
    // (between prepareToPlay() and releaseResources()).  updateHostTransport()
    // refuses to call getPlayHead() while this is false, which is the only
    // way to avoid the Live-12-AU SIGSEGV during device/SR reconfigurations
    // since the resulting fault is not a catchable C++ exception.
    std::atomic<bool> auReady { false };

    // Dedicated playhead-reader worker.  see startPlayheadThread().
    std::thread playheadThread;
    std::atomic<bool> playheadThreadShouldExit { false };

    // Correction block "Advanced" expand/collapse state (persisted across sessions).
    bool advancedExpandedState = false;

    // Morph slider amount (automatable parameter "morph_amount").
    std::atomic<float>* morphAmountParam = nullptr;
    juce::AudioParameterFloat* morphParam = nullptr;

    // === ARA2 Waveform overlay cache ===
    juce::AudioBuffer<float> araWaveformBuffer;
    double araWaveformSampleRate = 44100.0;
    std::atomic<bool> araWaveformReady { false };
    juce::CriticalSection araWaveformLock;
    std::atomic<bool> waveformCaptureEnabled { true };

    // Deferred key/scale changes detected on the audio thread.
    // flushPendingParameterChanges() (UI thread) reads these and calls
    // setValueNotifyingHost() safely.
    static constexpr int kPendingNone = -999;
    std::atomic<int> pendingDetectedKey { kPendingNone };
    std::atomic<int> pendingDetectedScale { kPendingNone };

    // Time signature state (for Curve Editor ruler).
    std::atomic<int> currentTimeSigNumerator { 4 };
    std::atomic<int> currentTimeSigDenominator { 4 };
    struct BarSignatureEvent {
        double ppqPosition;
        int numerator;
        int denominator;
    };
    std::vector<BarSignatureEvent> araBarSignatures;
    juce::CriticalSection araBarSigLock;

    // Harmony state.
    juce::Array<float> harmonyFrequencies;
    juce::Array<float> harmonyFrequenciesClean; // scale-locked, Follow Lead ignored (MIDI OUT)
    juce::Array<float> lastHarmonyNotes; // keep last notes to allow release rendering
    int  lastHarmonyNotesType = -1;      // harmony type that produced lastHarmonyNotes (cache validity)
    juce::Array<float> lastHarmonyNotesClean; // scale-locked cache for MIDI OUT release
    std::atomic<float> harmonyOutputLevel { 0.0f };
    static constexpr int maxHarmonySnapshotVoices = 8;
    std::array<std::atomic<float>, maxHarmonySnapshotVoices> harmonyFrequencySnapshot;
    std::array<std::atomic<float>, maxHarmonySnapshotVoices> harmonyFrequencyCleanSnapshot;
    std::atomic<int> harmonyFrequencySnapshotSize { 0 };
    std::atomic<int> harmonyFrequencyCleanSnapshotSize { 0 };
    std::atomic<uint32_t> harmonySnapshotVersion { 0 };
    void publishHarmonySnapshots() noexcept;
    juce::AudioBuffer<float> harmonyBuffer; // stereo buffer for mixing
    juce::AudioBuffer<float> synthWorkBuffer; // preallocated synth harmony render buffer
    juce::AudioBuffer<float> shiftedVoiceGainRamps; // reusable gain ramps for shifted-voice mixing
    juce::AudioBuffer<float> leadReferenceBuffer; // pre-shift snapshot for LPC formant preservation
    juce::AudioBuffer<float> harmonyWarpBuffer;   // scratch for per-voice P0 formant pre-warp
    // last mixed harmony buffer saved to perform a crossfade on stop
    juce::AudioBuffer<float> lastMixedHarmonyBuffer;
    bool wasHarmonyActiveLastBlock { false };
    // Smoothed master enable gain for the harmony bus. Toggling the Harmony
    // enable button ramps this (instead of hard-cutting the mix) so enabling/
    // disabling harmony produces no click.
    juce::LinearSmoothedValue<float> harmonyEnableGain { 0.0f };
    // Smoothed gate gain for harmony voices â€” prevents clicks when the
    // Noise Gate opens/closes (the raw gateGain jumps instantly).
    juce::LinearSmoothedValue<float> harmonyGateGain { 0.0f };
    // Harmony-type transition dip: when the harmony TYPE changes (morph, preset
    // load), the harmony note set/voice count jump instantly and can click. On a
    // type change we fade the whole harmony mix in from silence over a short,
    // SAMPLE-ACCURATE Hann window (applied per-sample in the mix loop), masking
    // the discontinuity without the block-stepped click a per-block dip causes
    // at large buffer sizes. `typeDipFadeTotalSamples`/`typeDipFadeRemaining`
    // drive the fade; `lastHarmonyTypeVal` detects the change.
    int lastHarmonyTypeVal = -1;       // the harmony type currently being rendered (applied)
    int pendingHarmonyType = -1;       // the requested new type, applied after the fade-out
    int typeDipFadeTotalSamples = 0;   // total crossfade length in samples
    int typeDipFadeRemaining = 0;      // remaining crossfade samples (0 = idle)
    // True while a harmony-type transition runs. The transition is a DEFERRED
    // retarget: the old note set keeps rendering (and fades out) instead of
    // being cut instantly, then the new note set fades in. The whole harmony
    // bus is dipped (fade out -> hold -> fade in) so neither the old-cut nor
    // the new onset is audible. No snapshot is used, so there is no per-block
    // restart discontinuity.
    bool typeCrossfadeActive = false;
    juce::AudioBuffer<float> typeCrossfadeOld;
    // Diagnostic only (OVT_FORCE_LOG): on the block where a harmony-type change
    // is detected, capture a small window of raw harmony-bus and output samples
    // plus the peak sample-to-sample jump, so the actual discontinuity (pitch
    // step, low-frequency pop, phase reset, boundary step) can be inspected.
    bool  diagTypeChangePending = false;
    float diagHarmonyPeakJump = 0.0f;
    float diagOutputPeakJump  = 0.0f;
    float diagHarmonyWindow[48] = { 0.0f };
    float diagOutputWindow[48]  = { 0.0f };
    int   diagWindowLen = 0;
    bool  diagWindowFilled = false;
    float diagPrevLastOut = 0.0f;
    float diagPrevHarmonyLast = 0.0f;
    // Common musical attack envelope applied to the final harmony bus. This
    // covers both shifted-voice and synthesized harmony paths.
    float harmonyAttackGain = 0.0f;
    // Temporary buffers to hold pitch-shifted voices (preallocated)
    std::vector<juce::AudioBuffer<float>> shiftedVoiceBuffers;
    // Dedicated pitch shifters per shifted voice (separate state from main pitchShifter)
    std::vector<std::unique_ptr<ovtdsp::PitchShifter>> shiftedVoicePitchShifters;
    static constexpr int maxShiftedVoices = 4;
    std::array<juce::LinearSmoothedValue<float>, maxShiftedVoices> shiftedVoiceGains;
    // Per-voice ratio glide for the shifted-voice pitch shifters. On a harmony
    // TYPE change the target note (and thus ratioH) jumps; a fast glide masks
    // that pitch step so no bus mute (level hole) is needed. It only glides on
    // LARGE ratio changes (>12% ~ a minor 3rd); vibrato / follow-lead (3-5%)
    // pass through instantly so the harmony never wobbles. (Re-introduced after
    // the earlier >3% threshold caused vibrato wobble and was removed.)
    std::array<ovtdsp::BlockAwareOnePole, maxShiftedVoices> shiftedVoiceRatioSmoothers;
    std::array<float, maxShiftedVoices> shiftedVoiceSmoothedRatio = { 1.0f, 1.0f, 1.0f, 1.0f };
    // Smooths the per-voice loudness normalization 4/sqrt(N). On a harmony TYPE
    // change the active voice count N steps instantly, which would make the
    // remaining voices' level jump (e.g. 4/sqrt(4)=2.0 -> 4/sqrt(1)=4.0 when the
    // count drops 4->1), audible as a one-way "pop" / louder attack. Gliding it
    // over ~40 ms (same TC as the per-voice gain ramps) keeps loudness constant
    // through the transition. Buffer-size independent (Fix AY convention).
    ovtdsp::BlockAwareOnePole shiftedVoiceLevelSmoother;
    // LPC cross-synthesis formant preservation (P1 = C0, P2 = C1Hybrid):
    // one instance per harmony voice (lead instance declared above).
    std::array<ovtdsp::LpcFormantPreserver, maxShiftedVoices> lpcFormantPreserverHarmony;
    // MIDI out state (per-channel last note sent, channels 1..16 mapped to index 0..15)
    int lastSentMidiNote[16] = { -1 };

    // Held incoming MIDI notes (note numbers) used by the "MIDI Target /
    // Follow" feature. Updated only on the audio thread (processBlock), so no
    // lock is needed. The most-recently-pressed note (last in the list)
    // becomes the correction target when the toggle is on.
    juce::Array<int> heldMidiNotes;

    // Random generator for Humanize effect.
    juce::Random random;

    // Block-aware one-pole smoothers for parameters that change every audio
    // block. Replacing the old "y = y*0.95 + x*0.05" form, which is buffer-size
    // dependent (the time constant halves at 128 samples vs. 256 samples).
    // These smoothers compute the alpha from the actual block duration so
    // the time constant is the same in seconds regardless of buffer size.
    ovtdsp::BlockAwareOnePole flexTuneSmoother;     // ~200 ms TC (Fix BC, was 500ms in Fix BB), sees raw 0..1
    // 2026-07-23 (Fix BC): upstream smoother for the FlexTune DEADBAND
    // threshold crossing. The deadband is a step function (0 inside the
    // threshold, smoothstep outside), so when the singer's pitch
    // (modulated by 5Hz vibrato) crosses the threshold back and forth,
    // the deadband output is a 5Hz square wave. The downstream
    // flexTuneSmoother can only attenuate this square wave by ~80%
    // (|H(5Hz)| = 0.20 at TC=500ms), leaving 20% of the step amplitude
    // in `targetRatio`, which is audibly perceived as pops at every
    // deadband crossing. By smoothing the INPUT pitch (f0_in) before
    // the deadband computation, we convert the 5Hz square wave into a
    // 5Hz SINE wave (much smoother), and the deadband output becomes a
    // 5Hz soft transition (no step). The downstream flexTuneSmoother
    // then barely has any work to do (TC=200ms is sufficient).
    // This is the "Option B: double lissage" approach.
    ovtdsp::BlockAwareOnePole f0SmootherForDeadband;  // ~150 ms TC, sees raw f0_in (Hz)
    ovtdsp::BlockAwareOnePole humanizeSmoother;      // ~150 ms TC, sees raw cents
    float currentFlexTuneAmount = 1.0f;              // raw target for flexTuneSmoother
    float currentHumanizeCents = 0.0f;               // raw target for humanizeSmoother

    // 2026-07-23 (Fix AY + Fix AZ): minimum smoothing on the ratio AFTER
    // the RetargetEnvelope. The RetargetEnvelope at Speed=0 is transparent
    // (no smoothing), and at moderate Speed (10-50 ms) the per-block jitter
    // from vibrato preservation (5 Hz, ~5 cents), YIN pitch detection steps
    // (every 2048 samples = 46 ms) and the residual modulation from
    // flexTuneSmoother (TC=200ms) and humanizeSmoother (TC=150ms) reaches
    // the OLA chain as discrete steps. With a grain spacing of ~512
    // samples, even a 0.2% per-block step in `targetRatio` translates to
    // ~1 sample of grain misalignment, which the OLA window cannot absorb
    // cleanly and the user perceives as a "scratch" (most audible with
    // Flex > 0 cents; more pronounced in Modern mode than Transparent mode
    // because Modern preserves the full vibrato amplitude).
    //
    // 2026-07-23 (Fix AZ): TC was raised from 50ms to 80ms. At 50ms the
    // 5Hz vibrato was only reduced by 53% (|H(5Hz)| = 0.53), still leaving
    // ~0.14% residual modulation in the targetRatio that the OLA chain
    // could not fully absorb. At 80ms the 5Hz vibrato is reduced by 70%
    // (|H(5Hz)| = 0.30), bringing the residual modulation below the OLA
    // grain spacing sensitivity threshold (~0.5 sample misalignment). The
    // compounded retargeting time is approximately
    // `max(Speed, 80ms) + 80ms / 2`, still allowing Speed=10ms to be
    // perceptibly faster than Speed=100ms (compounded ~14 vs ~36 blocks to
    // reach 90%).
    ovtdsp::BlockAwareOnePole speedFloor;            // 80 ms TC, on `ratio` after retarget

    // Vibrato preservation: smooths the detected pitch to a center reference
    // (vibrato modulation removed) so the correction can preserve the vibrato.
    ovtdsp::VibratoPreserver vibratoPreserver;

    // Attack-aware correction: a temporal envelope that eases off the correction
    // on note onsets/transients, then ramps back to full correction.
    ovtdsp::AttackAwareEnv attackEnv;

    // Automatic key detection (in-plugin "Auto" source): accumulates a
    // pitch-class profile and estimates the key/scale. The companion plugin
    // publishes to KeyBridge instead; this is only used for the Auto source.
    ovtdsp::KeyDetector keyDetector;
    int lastAutoKey = -1;    // last applied musical key (avoids redundant updates)
    int lastAutoScale = -1;  // last applied scale index

    // Automatic key detection from the Sidechain input bus (key_source ==
    // "Sidechain"): analyses an external signal (e.g. accompaniment) routed to
    // the sidechain bus to estimate the key/scale independently of the voice.
    ovtdsp::KeyDetector sidechainKeyDetector;

    // Debug: remember previous bypass state to log changes
    std::atomic<int> prevBypassState { 0 };
    // Debug: observe grains created by PitchShifter (cross-thread counter)     
    int lastObservedGrainCount = 0;
    // Debug: test grain parameter
    std::atomic<float>* dbgTestGrainParam = nullptr;
    std::atomic<int> prevDbgTestGrain { 0 };
    std::atomic<bool> pendingDbgGrainReset { false }; // deferred reset for UI thread
    // Debug: throttle frequent logs (ms)
    std::atomic<uint32_t> lastProcessLogTime { 0 };
    std::atomic<uint32_t> lastHarmonyLogTime { 0 };
    int appliedLatencyMode = -1;

    // CPU usage tracking (smoothed ratio of processBlock time to available time).
    std::atomic<float> cpuUsage { 0.0f };

    // Silence counter for Sleep mode (CPU saving).
    int silenceSamples = 0;
    int maxSilenceSamples = 44100; // Default 1s at 44.1kHz
    bool harmonyInputGateOpen = false; // hysteresis gate to avoid buzz/click chattering near silence

    // FIFO to accumulate samples for pitch detection.
    // Necessary because the analysis window is larger than the block.
    static constexpr int analysisWindow = 4096;
    static constexpr int analysisHopSize = 2048; // Calculate YIN every ~46ms to halve CPU load
    int samplesSinceLastAnalysis = 0;
    juce::AudioBuffer<float> analysisFifo;
    int fifoWriteIndex = 0;
    int fifoFillCount = 0;

    // PRE-ALLOCATED linear buffer to un-FIFO before YIN detection.
    // Allocated in prepareToPlay() to `analysisWindow` floats. Avoids
    // heap allocation per audio block (otherwise 2.4 MB/s allocations at       
    // 144 samples, major source of glitches).
    juce::HeapBlock<float> analysisLinearBuffer;

    // Dedicated analysis path for the optional Sidechain input bus: a separate
    // FIFO + detector + linear buffer so key detection can run on the sidechain
    // signal without disturbing the main-input pitch analysis above.
    juce::AudioBuffer<float> sidechainFifo;
    int sidechainFifoWriteIndex = 0;
    int sidechainFifoFillCount = 0;
    int sidechainSamplesSinceLastAnalysis = 0;
    juce::HeapBlock<float> sidechainLinearBuffer;
    std::unique_ptr<ovtdsp::YinPitchDetector> sidechainPitchDetector;

    // Updates DSP parameters from the value tree.
    void syncParameters();
    void applyLatencyMode();

    // VST3 Extensions (e.g., Micro View for Studio One)
    std::unique_ptr<juce::VST3ClientExtensions> vst3Extensions;

    // Calculates the input pitch on the current block, returns freq in Hz.     
    float computeInputPitch (const juce::AudioBuffer<float>& buffer);

    // Calculates the pitch of the Sidechain input bus (key_source == "Sidechain"),
    // returns freq in Hz (0 when silent or the FIFO is not yet filled). Uses a
    // dedicated FIFO + detector so it never interferes with the main input.
    float computeSidechainPitch (const juce::AudioBuffer<float>& buffer);

    // Applies a detected (musical) key + scale index to the key/scale
    // parameters, but only when they actually change (avoids spamming host
    // automation every block). Used by the Auto and Companion sources.
    void applyDetectedKey (int musicalKey, int scaleIdx);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenVoxTunerAudioProcessor)   
};



