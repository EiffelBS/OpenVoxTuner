// SwipePitchDetector.h
// Pitch detection using a SWIPE'-inspired spectral algorithm.
// Implements IPitchDetector interface for interchangeability with YIN.
//
// SWIPE' (Sawtooth Waveform Inspired Pitch Estimator) by
// Arturo Camacho & John G. Harris, IEEE Trans. ASLP, 2008.
//
// This implementation follows the SWIPE' principle:
//   1) Compute the power spectrum of the signal (FFT + magnitude)
//   2) For each candidate pitch, correlate the spectrum with a
//      sawtooth-wave kernel at that pitch
//   3) Pick the pitch with highest correlation
//   4) Refine via parabolic interpolation
//
// Advantages over YIN:
//   - More robust on breathy/creaky voices
//   - Fewer octave errors (spectral structure is unambiguous)
//   - Better noise rejection (only harmonics are evaluated)
//   - Lower false-positive rate (no pitch reported when uncertain)
//
// Trade-offs:
//   - Higher CPU (~2-3x YIN at equivalent resolution)
//   - Slightly higher latency (one FFT frame)
//   - Requires power-of-2 FFT size

#pragma once

#include "IPitchDetector.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>

namespace atdsp
{

/**
 * SWIPE'-inspired spectral pitch detector.
 *
 * Detection range: 30 - 1000 Hz.
 * CPU: ~2-3x YIN (one FFT + spectral correlations per candidate).
 * Latency: 1 FFT frame (typically ~23ms at 44.1 kHz with 1024 samples).
 */
class SwipePitchDetector : public IPitchDetector
{
public:
    SwipePitchDetector();
    ~SwipePitchDetector() override;

    // == IPitchDetector interface ==
    void prepare (double sampleRate, int blockSize) override;
    void reset() override;
    float detectPitch (const float* samples, int numSamples) override;
    void setThreshold (float t) override { threshold = juce::jlimit (0.01f, 0.99f, t); }
    float getThreshold() const override { return threshold; }
    juce::String getName() const override { return "SWIPE'"; }

private:
    /** Build a sawtooth-wave kernel for a given candidate frequency. */
    void buildKernel (float freq, float* kernel, int fftSize);

    /** Compute SWIPE' harmonic correlation between spectrum magnitude and kernel. */
    float computeCorrelation (const float* spectrum, const float* kernel, int halfSize, float signalEnergy);

    /** Parabolic interpolation around a peak. */
    float interpolatePeak (float y1, float y2, float y3);

    double sampleRate = 44100.0;
    float threshold = 0.12f;

    // FFT engine (JUCE) — HeapBlock guarantees 16-byte alignment needed by
    // performRealOnlyForwardTransform (SIMD). std::vector does NOT guarantee this.
    int fftSize = 1024;
    int fftOrder = 10;
    std::unique_ptr<juce::dsp::FFT> fft;
    juce::HeapBlock<float> fftWindow;   // for windowing
    juce::HeapBlock<float> fftRealBuffer; // real-only FFT: [2 * fftSize] (packed + scratch)
    juce::HeapBlock<float> spectrumMag; // magnitude spectrum (halfSize + 1)

    // Kernel cache (rebuilt on prepare, reused per detectPitch).
    int numCandidates = 0;
    juce::HeapBlock<float> kernelCache; // flattened: [numCandidates][halfSize+1]
    float* kernelCachePtrs[300];        // pointers into kernelCache
    float candidateFreqs[300];          // candidate pitch frequencies (Hz)

    // Lag buffer for framing (input accumulation when block < fftSize).
    juce::HeapBlock<float> ringBuffer;
    int ringWritePos = 0;
    int ringSize = 0;

    // Smoothing / stability.
    float lastPitch = 0.0f;
    int stableCount = 0;
    static constexpr int STABLE_THRESHOLD = 3;

    // Kernel configuration: limit to first 6 harmonics (SWIPE' style).
    // Using more harmonics causes octave-up false positives because the
    // kernel at 2xf0 matches the voice's even harmonics.
    static constexpr int MAX_KERNEL_HARMONICS = 6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SwipePitchDetector)
};

} // namespace atdsp