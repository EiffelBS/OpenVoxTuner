// PyinPitchDetector.h
// Pitch detection using a PYIN-inspired probabilistic approach.
// Implements IPitchDetector for interchangeability with YIN / SWIPE'.
//
// PYIN (Probabilistic YIN): Mauch & Dixon, "PYIN: A Fundamental Frequency
//   Estimator Using Probabilistic Threshold Distributions", ISMIR 2014.
//
// This implementation improves upon standard YIN by:
//   1) Tracking multiple pitch candidates with HMM-like Viterbi smoothing
//   2) Using a probabilistic threshold rather than a fixed one
//   3) Rejecting octave jumps via state continuity cost
//   4) Producing smoother pitch tracks with fewer dropouts

#pragma once

#include "IPitchDetector.h"
#include <juce_core/juce_core.h>

namespace atdsp
{

/**
 * PYIN-inspired probabilistic pitch detector.
 *
 * Key improvements over YIN:
 *   - Multiple pitch candidates tracked per frame (up to 3)
 *   - Viterbi-like transition cost: penalizes large jumps between frames
 *   - Probabilistic threshold: evaluates several tau values, not just the first
 *   - Better octave continuity (self-corrects via state tracking)
 *   - Smoother output (fewer dropouts on noisy/breathy voice)
 *
 * CPU: ~1.5x standard YIN (multiple tau evaluations + Viterbi pass).
 */
class PyinPitchDetector : public IPitchDetector
{
public:
    PyinPitchDetector();
    ~PyinPitchDetector() override;

    // == IPitchDetector interface ==
    void prepare (double sampleRate, int blockSize) override;
    void reset() override;
    float detectPitch (const float* samples, int numSamples) override;
    void setThreshold (float t) override { threshold = juce::jlimit (0.01f, 0.99f, t); }
    float getThreshold() const override { return threshold; }
    juce::String getName() const override { return "PYIN"; }

private:
    /** Core YIN computation (steps 1-2), returns d'(tau) array. */
    void computeYin (const float* samples, int numSamples,
                     float* dPrime, int maxTau, int halfSize);

    /** Extract up to N best pitch candidates from d'(tau). */
    struct Candidate {
        float frequency; // Hz
        float clarity;   // d'(tau) value (lower = better)
        int tau;         // period in samples
    };
    static constexpr int MAX_CANDIDATES = 3;
    int findCandidates (const float* dPrime, int halfSize, int minLag,
                        Candidate* out, int maxOut);

    /** Viterbi step: compute cost for each candidate given previous state.
     *  Returns index of best candidate. */
    int viterbiStep (const Candidate* candidates, int numCandidates,
                     const Candidate* prevCandidates, int prevNum,
                     double* costs, int* backtrace);

    double sampleRate = 44100.0;
    float threshold = 0.05f;
    float freqMinHz = 30.0f;
    float freqMaxHz = 1000.0f;

    // YIN working buffer (HeapBlock for 16-byte aligned allocation).
    juce::HeapBlock<float> yinBuffer;
    int bufferCapacity = 0;
    int maxLag = 0;

    // Viterbi state.
    Candidate prevCandidates[MAX_CANDIDATES];
    int prevNumCandidates = 0;

    // Transition cost scaling factor (higher = smoother tracks).
    float transitionCostScale = 1.5f;

    // Dropout tracking: if no candidate for N frames, reset Viterbi.
    int dropoutFrames = 0;
    static constexpr int DROPOUT_RESET = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PyinPitchDetector)
};

} // namespace atdsp