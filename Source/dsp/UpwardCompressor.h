#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace ovtdsp
{
    /**
     * Upward compressor applied to the input audio before pitch detection.
     *
     * Unlike a downward compressor (which tames loud peaks), an upward
     * compressor only *raises* signals sitting below a pivot level, leaving
     * louder material untouched. This lifts quiet/weak vocal passages toward
     * the average level without causing the "pumping" or gain-reduction
     * artefacts associated with standard compression, which is ideal for
     * evening out a vocal before the pitch detector and tuning stage.
     *
     * A single user parameter drives it: the upward amount (0..1). The pivot
     * level follows the running RMS of the signal (auto), so the effect adapts
     * to the source without a manual threshold. Above the pivot the gain is
     * unity; below it, the signal is pushed up by an upward ratio derived from
     * the amount.
     */
    class UpwardCompressor
    {
    public:
        UpwardCompressor() = default;

        void prepare (double sr)
    {
        sampleRate = sr;
        // Smoothing coefficients for the level detector (RMS) and the gain
        // envelope. Time constants chosen to be musical yet artefact-free.
        const double rmsTau  = 0.050; // 50 ms RMS averaging
        const double envTau  = 0.020; // 20 ms gain-envelope smoothing
        rmsCoeff = 1.0f - std::exp (-1.0 / (rmsTau * sr));
        envCoeff = 1.0f - std::exp (-1.0 / (envTau * sr));
        // Pivot follows the signal level with a SLOW release only: it can jump
        // up immediately (when the gate opens) but falls back slowly. This
        // prevents the pivot from collapsing to ~0 right after the gate opens
        // (which would otherwise divide the quiet attack by a tiny pivot and
        // produce a loud "clac").
        const double pivotReleaseTau = 0.300; // 300 ms slow release
        pivotReleaseCoeff = 1.0f - std::exp (-1.0 / (pivotReleaseTau * sr));
        reset();
    }

    void reset()
    {
        rmsState = 0.0f;
        pivotState = 0.0f;
        gainEnv  = 1.0f;
    }

        void setEnabled (bool e) { enabled = e; }

        /** Upward amount in 0..1. 0 = bypass behaviour (gain stays unity),
         *  1 = strongest upward lift. */
        void setAmount (float a)
        {
            amount = juce::jlimit (0.0f, 1.0f, a);
            // Map amount to an upward ratio: 1.0 (no lift) .. 4.0 (strong).
            upwardRatio = 1.0f + amount * 3.0f;
        }

        bool isEnabled() const { return enabled; }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! enabled || amount <= 0.0f)
                return;

            const int N = buffer.getNumSamples();
            const int ch = buffer.getNumChannels();
            if (N == 0 || ch == 0) return;

            // Per-sample processing keeps the RMS detector and gain envelope
            // sample-accurate and continuous across buffers (state persists).
            for (int i = 0; i < N; ++i)
            {
                // Mono RMS detection (uses the same sample across channels by
                // averaging the squared sum).
                float sumSq = 0.0f;
                for (int c = 0; c < ch; ++c)
                {
                    const float s = buffer.getSample (c, i);
                    sumSq += s * s;
                }
                const float inst = std::sqrt (sumSq / (float) ch);
                // Asymmetric smoothing: faster attack, slower release on the
                // level detector so it tracks transients but doesn't flicker.
                const float coeff = (inst > rmsState) ? rmsCoeff * 2.0f : rmsCoeff;
                rmsState += (inst - rmsState) * coeff;

                // Pivot = reference level with SLOW release only. It jumps up
                // immediately to the current level (so it is correct the instant
                // the gate opens) but decays slowly. This keeps the pivot near
                // the signal's real level instead of collapsing to ~0 in the
                // first samples after silence, which is what caused the "clac".
                if (inst > pivotState)
                    pivotState = inst;
                else
                    pivotState += (inst - pivotState) * pivotReleaseCoeff;

                // Pivot floor (near-silence): skip compression to avoid
                // amplifying noise. When the gate has just opened, pivotState is
                // already near the signal level, so quiet attacks are NOT
                // multiplied by a tiny pivot.
                const float pivot = juce::jmax (pivotState, 1.0e-4f);

                // Upward gain for THIS sample's level: if level < pivot, push up.
                // gain = (level / pivot) ^ (1/ratio - 1), clamped >= 1 so we
                // never attenuate (pure upward behaviour).
                float gain;
                if (inst < pivot)
                    gain = std::pow (inst / pivot, 1.0f / upwardRatio - 1.0f);
                else
                    gain = 1.0f;
                if (! std::isfinite (gain) || gain < 1.0f) gain = 1.0f;
                // Safety ceiling so we never explode a signal.
                gain = juce::jmin (gain, 12.0f);

                // Smooth the gain envelope to avoid zipper noise.
                gainEnv += (gain - gainEnv) * envCoeff;

                for (int c = 0; c < ch; ++c)
                    buffer.setSample (c, i, buffer.getSample (c, i) * gainEnv);
            }
        }

    private:
        bool  enabled = false;
        float amount = 0.0f;        // user parameter 0..1
        float upwardRatio = 1.0f;   // derived from amount
        float rmsState = 0.0f;      // running RMS level (linear)
        float pivotState = 0.0f;     // reference level for upward gain (slow release only)
        float gainEnv  = 1.0f;       // smoothed applied gain
        float rmsCoeff = 0.0f;
        float envCoeff = 0.0f;
        float pivotReleaseCoeff = 0.0f;
        double sampleRate = 44100.0;
    };
}
