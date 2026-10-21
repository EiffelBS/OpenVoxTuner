// YinPitchDetector.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include "IPitchDetector.h"
#include <juce_core/juce_core.h>

namespace ovtdsp
{

/**
 * YIN-based pitch detector.
 *
 * Algorithm steps:
 *   1) Difference function    d(tau) = sum (x[j] - x[j+tau])^2
 *   2) Cumulative mean normalized difference d'(tau)
 *   3) Find first minimum below threshold
 *   4) Parabolic interpolation for sub-sample precision
 *   5) Anti-octave-error correction via octave continuity
 *   6) Median filtering for outlier rejection
 *
 * Limitations:
 *   - Detection range: 30 - 1000 Hz (configurable in prepare)
 *   - Requires input buffer >= 2 * maxLag samples
 *   - Mono only (single channel)
 */
class YinPitchDetector : public IPitchDetector
{
public:
    YinPitchDetector();
    ~YinPitchDetector() override;

    // == IPitchDetector interface ==
    void prepare (double sampleRate, int blockSize) override;
    void reset() override;
    float detectPitch (const float* samples, int numSamples) override;
    void setThreshold (float t) override { threshold = juce::jlimit (0.01f, 0.99f, t); }
    float getThreshold() const override { return threshold; }
    juce::String getName() const override { return "YIN"; }

    /** Update the detection frequency range at runtime (no-op if sampleRate==0).
        Used by the Voice Type selector to constrain the YIN search range to a
        vocal register (Bass, Tenor, Soprano, etc.). Recomputes the lag bounds
        and reallocates the working buffer if the new maxLag exceeds the current
        allocation. Safe to call from the audio thread; the worst case is a
        single heapBlock grow. */
    void setFrequencyRange (float minHz, float maxHz);

    /** Selects offline (batch) analysis instead of the live real-time mode.
        Default is false, i.e. the live behaviour below is untouched.

        Offline mode changes two things, both required when the detector is
        driven frame by frame over a whole recording instead of once per audio
        block:
          - The two anti-octave heuristics are disabled. They exist because the
            live path feeds a 4x-decimated 1024-sample window in which YIN's
            first minimum can be the 2nd harmonic; "Case A" then tests tau*2 to
            recover the fundamental. Over a full-band, non-decimated window the
            first minimum is already the fundamental, so that correction reports
            every sustained tone exactly one octave low (verified on pure tones
            and on harmonic-rich, vibrato-modulated voice-like signals).
          - The internal 5-frame median filter is bypassed: in batch mode the
            caller controls the frame track (and applies its own median per
            detected note), and the live median would smear note boundaries by
            two hops.
        Both behaviours are only enabled by explicit callers; the real-time
        detection path keeps its current defaults. */
    void setOfflineMode (bool offline) { offlineMode = offline; }
    bool isOfflineMode() const { return offlineMode; }

private:
    /** Core YIN computation (steps 1-2). */
    float computeYin (const float* samples, int numSamples);

    /** Median filter for outlier rejection. */
    float getMedianFiltered (float newValue);

    double sampleRate = 44100.0;

    // Detection frequency range.
    float freqMinHz = 30.0f;
    float freqMaxHz = 1000.0f;

    // Clarity threshold (YIN cumulative mean normalized difference).
    float threshold = 0.05f;

    // Offline/batch analysis: see setOfflineMode().
    bool offlineMode = false;

    // Working buffers.
    juce::HeapBlock<float> yinBuffer;
    int bufferSize = 0;
    int maxLag = 0;

    // Anti-octave error: median filter + octave continuity.
    static constexpr int MEDIAN_SIZE = 5;
    float history[MEDIAN_SIZE] = { 0.0f };
    int historyIdx = 0;
    float lastValidPitch = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YinPitchDetector)
};

} // namespace ovtdsp


