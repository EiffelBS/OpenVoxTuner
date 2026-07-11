// YinPitchDetector.h
// Pitch detection using the YIN algorithm (de Cheveigne & Kawahara, 2002).
// Implements the IPitchDetector interface.
//
// This is the original algorithm from PitchDetector, refactored into
// the IPitchDetector hierarchy. The original PitchDetector.h/.cpp files
// are preserved unchanged for reference (see docs/pitch-detection-rollback-guide.md).

#pragma once

#include "IPitchDetector.h"
#include <juce_core/juce_core.h>

namespace atdsp
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

} // namespace atdsp