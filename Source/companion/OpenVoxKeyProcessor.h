// OpenVoxKeyProcessor.h
// Companion key-detection plug-in ("OpenVoxKey").
//
// Placed on an accompaniment (or any harmonic) track, it analyses the audio,
// estimates the musical key/scale with ovtdsp::KeyDetector (Krumhansl-Schmuckler)
// and publishes the result to the process-wide ovtdsp::KeyBridge keyed by a group
// letter (A/B/C/D). The main OpenVoxTuner instance, with Key/Scale Detection
// = "OpenVoxKey" and the matching group, reads that value and applies it.
//
// The plug-in is a transparent effect: it passes the audio through unchanged so
// the accompaniment keeps playing.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/YinPitchDetector.h"
#include "dsp/KeyDetector.h"
#include "dsp/KeyBridge.h"
#include "dsp/ScaleQuantizer.h"

class OpenVoxKeyProcessor : public juce::AudioProcessor
{
public:
    OpenVoxKeyProcessor();
    ~OpenVoxKeyProcessor() override;

    //=== AudioProcessor =====================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    const juce::String getName() const override;
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //=== UI accessors (read by the editor on a timer) ======================
    int  getCurrentDetectedKey()  const { return lastKey.load(); }
    bool isCurrentDetectedMinor() const { return lastMinor.load(); }
    int  getGroup() const { return groupParam != nullptr ? groupParam->getIndex() : 0; }
    juce::AudioParameterChoice* getGroupParameter() const { return groupParam; }

    // Debug/UI: time (seconds, system clock) of the most recent non-silent audio
    // received on the main input bus. The editor uses it to distinguish an
    // active "searching for key" state (audio present, no key yet) from "no
    // signal" (bus silent / track not playing). 0 until first activity.
    double getLastAudioTime() const { return lastAudioTime.load(); }

    // Forces a publish of the last detected key/scale to the shared bridge, even
    // if the detection has not changed since the previous publish. The editor's
    // "Send" button calls this so the user can manually re-sync OpenVoxTuner
    // (e.g. after changing the scale by hand there, which the bridge cannot know
    // about because no new detection was produced).
    void forcePublish() noexcept;

private:
    // Group letter (A/B/C/D) the result is published under.
    juce::AudioParameterChoice* groupParam = nullptr;

    // Pitch detection (YIN) + key detection (Krumhansl-Schmuckler).
    std::unique_ptr<ovtdsp::YinPitchDetector> pitchDetector;
    ovtdsp::KeyDetector keyDetector;

    // Analysis FIFO (mirrors OpenVoxTuner's main-input path but is self-contained).
    static constexpr int analysisWindow = 4096;
    static constexpr int analysisHopSize = 2048;
    int samplesSinceLastAnalysis = 0;
    juce::AudioBuffer<float> analysisFifo;
    int fifoWriteIndex = 0;
    int fifoFillCount = 0;
    juce::HeapBlock<float> analysisLinearBuffer;

    // Last published detection (avoids redundant publishes / bridge churn).
    std::atomic<int>  lastKey { -1 };
    std::atomic<bool> lastMinor { false };

    // Debug/UI: timestamp of the last non-silent audio frame on the main input
    // bus (see getLastAudioTime()). 0 until first activity.
    std::atomic<double> lastAudioTime { 0.0 };

    double currentSampleRate = 44100.0;

    // Detect the pitch of the current block from its main input bus.
    // Returns frequency in Hz (0 when silent / FIFO not yet filled).
    float computePitch (const juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenVoxKeyProcessor)
};
