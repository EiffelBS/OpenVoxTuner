// LpcFormantPreserver.h
// LPC cross-synthesis formant preservation (Phase: P1/P2 of the formant study).
//
// This module performs a true source/filter decoupling by LPC cross-synthesis
// (Almeida & Tribolet style), applied AFTER the PSOLA pitch shifter:
//
//   1) Analyse the post-shift (transposed) signal  -> LPC a_shifted.
//   2) Whiten it:  e[n] = A_shifted(z) * x[n]
//                  (excitation / source, at the NEW pitch). NOTE the PLUS sign.
//   3) Re-synthesize through the ORIGINAL envelope:  y[n] = (1/A_orig(z)) * e[n]
//                  (formants moved back to their natural place).
//
// The reference (pre-shift) signal supplies a_orig; the post-shift signal
// supplies a_shifted. The result keeps the shifted pitch but the original
// formants, which is exactly what the pre-warp biquad bank can only approximate.
//
// Modes:
//   C0       = P1: plain cross-synthesis (fixed order, no pre-emphasis).
//   C1Hybrid = P2: adds pre-emphasis, temporal LPC coefficient interpolation
//              (C1, reduces distortion at large upshifts), and a hybrid fallback
//              that bypasses LPC on unvoiced / near-silent / unstable frames
//              (falls back to the already pitch-shifted output).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace ovtdsp
{
    class LpcFormantPreserver
    {
    public:
        enum class Mode
        {
            C0 = 0,       // P1: plain LPC cross-synthesis
            C1Hybrid = 1  // P2: pre-emphasis + LPC interpolation + hybrid fallback
        };

        explicit LpcFormantPreserver (int order = 18);
        ~LpcFormantPreserver();

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void setOrder (int order);

        /// Applies LPC formant preservation IN PLACE on `out`.
        /// @param out       post-pitch-shift signal to be re-colored (modified in place).
        /// @param reference pre-pitch-shift signal carrying the TARGET formants.
        /// @param ratio     transposition ratio (1.0 = passthrough, used only for
        ///                  optional logging / future adaptive behaviour).
        /// @param mode      C0 (P1) or C1Hybrid (P2).
        void process (juce::AudioBuffer<float>& out,
                      const juce::AudioBuffer<float>& reference,
                      float ratio,
                      Mode mode);

    private:
        struct ChannelState
        {
            std::vector<float> xHist;        // whitening history (last P samples of x)
            std::vector<float> yHist;        // re-color history (last P samples of y)
            std::vector<float> aShiftedPrev; // previous-frame a_shifted (C1 blend)
            std::vector<float> aOrigPrev;    // previous-frame a_orig (C1 blend)
            float preEmphZ = 0.0f;           // pre-emphasis one-pole state
            float deEmphZ = 0.0f;            // de-emphasis one-pole state
        };

        std::vector<ChannelState> channels;
        int order = 18;
        double sampleRate = 44100.0;

        // Pre-emphasis coefficient (P2). Boosts high frequencies before LPC so
        // the all-pole model captures the spectral tilt more accurately.
        float preCoef = 0.97f;

        // C1 temporal interpolation blend (0 = keep previous frame, 1 = use current).
        float c1Alpha = 0.5f;

        // Bandwidth expansion for LPC stability: a_k *= lambda^k.
        static constexpr float bwLambda = 0.98f;

        // Hybrid fallback thresholds (applied to BOTH P1 and P2 for robustness
        // on real-world signals: fricatives, breath, consonants make the LPC
        // residual energy wildly variable, so a too-permissive gain can mute
        // the output or push it into clipping).
        static constexpr float hybridRmsFloor = 5.0e-3f; // below this -> passthrough
        static constexpr float hybridMaxScale = 2.0f;     // gain explode -> passthrough

        // Per-channel scratch buffers reused across process() calls.
        std::vector<float> autocorrRef;
        std::vector<float> autocorrIn;
        std::vector<float> aOrig;
        std::vector<float> aShifted;

        // Frame scratch buffers (sized to the block size in prepare()).
        // xCopy holds the (optionally pre-emphasized) input so the whitening
        // history can be read while out[] is overwritten in place. yRec holds
        // the pre-de-emphasis re-color output for the recurrence history.
        // xBackup holds the raw input at processChannel entry so the block
        // can be restored verbatim if the LPC chain explodes (instability
        // guard).
        std::vector<float> xCopy;
        std::vector<float> yRec;
        std::vector<float> xBackup;

        // Computes LPC coefficients a[1..P] (size P) for frame x[0..n) via the
        // autocorrelation method + Levinson-Durbin. Returns the prediction error
        // energy (R[0] - sum a_k R[k]) used for residual-gain normalization.
        static float computeLpc (const float* x, int n, int P,
                                  std::vector<float>& a, std::vector<float>& R);

        void processChannel (float* out, const float* ref, int n,
                             ChannelState& s, Mode mode);
    };
}
