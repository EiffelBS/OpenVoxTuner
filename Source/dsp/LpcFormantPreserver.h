// LpcFormantPreserver.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



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

        explicit LpcFormantPreserver (int order = 16);
        ~LpcFormantPreserver();

        void prepare (double sampleRate, int maxBlockSize);
        void reset();
        void setOrder (int order);

        /// Analysis hop in milliseconds. Default 10 ms (100 coefficient
        /// resolutions per second). Harmony voices can double it to 20 ms to
        /// halve the analysis cost — the perceived timbre is dominated by the
        /// lead. Takes effect on the next prepare().
        void setHopMs (double hopMs);

        /// Fixed analysis frame length (24 ms) in samples, after prepare().
        int getFrameSamples() const { return frameSamples; }
        int getHopSamples() const { return hopSamples; }

        /// The framing is causal: coefficients are computed from the frame
        /// ending at the CURRENT sample, and applied sample-by-sample from that
        /// sample on. The module adds no latency, so the plugin's
        /// setLatencySamples contract is untouched.
        int getLatencySamples() const { return 0; }

        /// Diagnostics of the last coefficient update, for tests and the bench:
        /// the raw residual ratio gRef/gIn the gate was tested against, the
        /// clamped loudness scale actually applied, and how many hop updates
        /// the instance has performed.
        float getLastResGain() const { return lastResGain; }
        float getLastScale() const { return lastScale; }
        int getHopUpdates() const { return hopUpdates; }
        bool isGatePassive() const { return gatePassive; }

        /// Aggregates over the LEAD-channel hops where the LPC path was active
        /// (the gate did not take the passthrough branch): the mean residual
        /// ratio and the mean applied scale.
        int getActiveHopUpdates() const { return activeHopUpdates; }
        float getMeanResGainActive() const
        {
            return activeHopUpdates > 0 ? static_cast<float> (sumResGainActive / activeHopUpdates) : 0.0f;
        }
        float getMeanScaleActive() const
        {
            return activeHopUpdates > 0 ? static_cast<float> (sumScaleActive / activeHopUpdates) : 0.0f;
        }

        /// Spread of the residual ratio over ALL lead-channel hops: the
        /// hop-to-hop stability of the normalization, which is what the hybrid
        /// gate has to absorb.
        float getMinResGain() const { return minResGain; }
        float getMaxResGain() const { return maxResGain; }
        int getExplosionCount() const { return explosionCount; }

        /// The two prediction-error energies of the last hop, in the
        /// pre-emphasized analysis domain: gRef for the reference frame, gIn
        /// for the whitening-input frame. The scale is built from gIn, so these
        /// two numbers say whether the loudness target is stable.
        float getLastErrRef() const { return lastErrRef; }
        float getLastErrIn() const { return lastErrIn; }
        float getMinErrIn() const { return minErrIn; }
        float getMaxErrIn() const { return maxErrIn; }

        /// Squared impulse-response energy of the re-color filter 1/A_orig(z)
        /// at the last hop, and the raw reference frame energy it is normalized
        /// against.
        float getLastEH() const { return lastEH; }
        float getLastRefEnergy() const { return lastRefEnergy; }

        /// Peak |y| the chain produced before the guard, peak |e| of the whitened
        /// signal, and peak |yRec| of the re-color history. These three say
        /// whether the scale clamp is the binding constraint or the recurrence
        /// itself is ringing.
        float getMaxAbsY() const { return maxAbsY; }
        float getMaxAbsE() const { return maxAbsE; }
        float getMaxAbsYRec() const { return maxAbsYRec; }

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
            // Circular analysis history, sized to frameSamples: the last
            // frameSamples samples of the raw output, of the (optionally
            // pre-emphasized) whitening input, of the pre-de-emphasis
            // re-color output, and of the reference.
            std::vector<float> outRaw;
            std::vector<float> xpe;
            std::vector<float> yRec;
            std::vector<float> refH;
            std::vector<float> refRawH;  // raw reference, for the sliding RMS only

            int pos = 0;          // write index into the circular buffers
            int hopCount = 0;     // samples since the last coefficient update
            double sumOut2 = 0.0; // sliding sum of outRaw^2 over the frame
            double sumRef2 = 0.0; // sliding sum of refRawH^2 over the frame
            double sumOut2Pre = 0.0; // sliding sum of xpe^2 (the chain domain)
            double sumRef2Pre = 0.0; // sliding sum of refH^2 (the chain domain)

            bool gatePass = true; // hybrid fallback: passthrough branch active
            float scale = 1.0f;   // loudness scale for the current hop

            std::vector<float> aOrigPrev;    // previous-hop a_orig (C1 blend)
            std::vector<float> aShiftedPrev; // previous-hop a_shifted (C1 blend)
            std::vector<float> kOrigPrev;    // previous-hop reflection coefficients of the reference
            std::vector<float> kShiftedPrev; // previous-hop reflection coefficients of the input
            float preEmphZ = 0.0f;           // pre-emphasis one-pole state (out)
            float refPreZ = 0.0f;            // pre-emphasis one-pole state (reference)
            float deEmphZ = 0.0f;            // de-emphasis one-pole state
            float ePeakHop = 0.0f;           // peak |e| of the whitened signal since the last hop
            float yPeakHop = 0.0f;           // peak |y| of the raw filter output in the PREVIOUS hop
            float gateMix = 0.0f;            // cross-fade 0 = raw input, 1 = chain output
        };

        std::vector<ChannelState> channels;
        int order = 16;
        double sampleRate = 44100.0;

        // Fixed framing: 24 ms analysis frame, 10 ms hop. The analysis quality
        // is a property of these constants, NOT of the DAW block size.
        static constexpr double frameMs = 24.0;
        double hopMs = 10.0;
        int frameSamples = 1058;
        int hopSamples = 441;

        std::vector<float> hann;     // precomputed window, size frameSamples
        std::vector<float> frameRef; // windowed reference frame scratch
        std::vector<float> frameX;   // windowed whitening-input frame scratch

        // Impulse response of the re-color filter 1/A_orig(z), used to measure
        // the energy gain the cross-synthesis applies to the whitened residual.
        std::vector<float> impulse;
        static constexpr int impulseLength = 128;

        // Pre-emphasis coefficient (P2). Boosts high frequencies before LPC so
        // the all-pole model captures the spectral tilt more accurately.
        float preCoef = 0.97f;

        // C1 temporal interpolation of coefficients between hops (P2 only).
        // 0 = keep the previous hop, 1 = use the current hop.
        float c1Alpha = 0.5f;

        // Bandwidth expansion for LPC stability: a_k *= lambda^k.
        static constexpr float bwLambda = 0.90f;

        // Hybrid fallback thresholds (applied to BOTH P1 and P2 for robustness
        // on real-world signals: fricatives, breath, consonants make the LPC
        // residual energy wildly variable, so a too-permissive gain can mute
        // the output or push it into clipping).
        //
        // The band is on the MEASURED loudness target (the scale that makes the
        // output frame energy equal the reference frame energy). That scale is
        // NOT centered on 1: the re-color filter 1/A_orig(z) has a large energy
        // gain (measured impulse-response energy ~1.4e5, i.e. a gain of ~370),
        // so the loudness-matching scale sits around 0.05. A band of 2.0 put
        // every voiced frame on the passthrough branch. The band is wide, and
        // the real safety is the per-sample explosion guard in processChannel.
        static constexpr float hybridRmsFloor = 5.0e-3f; // below this -> passthrough
        static constexpr float hybridMaxScale = 20.0f;    // gain explode -> passthrough

        // Cross-fade length for the hybrid gate. The gate switches between the
        // raw pitch-shifted input and the re-colored signal, and the two differ
        // by far more than the signal's own sample-to-sample jump, so a hard
        // switch is a step the listener hears as a click. 5 ms is short enough to
        // keep the formant correction tight and long enough to be inaudible.
        static constexpr double gateFadeMs = 5.0;
        int gateFadeSamples = 220; // set in prepare() from gateFadeMs

        // Diagnostics snapshot of the last hop update (channel 0).
        float lastResGain = 1.0f;
        float lastScale = 1.0f;
        int hopUpdates = 0;
        bool gatePassive = true;
        int activeHopUpdates = 0;
        double sumResGainActive = 0.0;
        double sumScaleActive = 0.0;
        float minResGain = 1.0e9f;
        float maxResGain = 0.0f;
        int explosionCount = 0;
        float lastErrRef = 0.0f;
        float lastErrIn = 0.0f;
        float minErrIn = 1.0e9f;
        float maxErrIn = 0.0f;
        float lastEH = 0.0f;
        float lastRefEnergy = 0.0f;
        float maxAbsY = 0.0f;
        float maxAbsE = 0.0f;
        float maxAbsYRec = 0.0f;

        // Per-channel scratch buffers reused across process() calls.
        std::vector<float> autocorrRef;
        std::vector<float> autocorrIn;
        std::vector<float> aOrig;
        std::vector<float> aShifted;
        std::vector<float> kOrig;    // reflection coefficients of the reference frame
        std::vector<float> kShifted; // reflection coefficients of the input frame

        // xBackup holds the raw input at processChannel entry so the block
        // can be restored verbatim if the LPC chain explodes (instability
        // guard). Sized to the DAW block size.
        std::vector<float> xBackup;

        // Computes LPC coefficients a[1..P] (size P) for frame x[0..n) via the
        // autocorrelation method + Levinson-Durbin. Fills K[1..P] with the
        // reflection coefficients (bounded by +-0.999, the stable interpolation
        // space). Returns the prediction error energy.
        static float computeLpc (const float* x, int n, int P,
                                  std::vector<float>& a, std::vector<float>& R,
                                  std::vector<float>& K);

        // Rebuild the direct-form coefficients a[1..P] from a set of reflection
        // coefficients, via the same Levinson recursion the solver uses.
        static void lpcFromRefl (const std::vector<float>& K, int P,
                                  std::vector<float>& a);

        // Recompute the coefficients at a hop boundary from the circular
        // history ending at `cur`, and evaluate the hybrid gate. `isLead`
        // marks channel 0, whose hop is the one the diagnostics getters report.
        void updateHop (ChannelState& s, int cur, Mode mode, bool isLead);

        void processChannel (float* out, const float* ref, int n,
                             ChannelState& s, Mode mode, bool isLead);
    };
}


