// UpwardCompressor.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


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
        const double envTau  = 0.020; // 20 ms gain-envelope smoothing (downward)
        const double gainAttackTau = 0.030; // 30 ms gain-envelope attack (upward)
        rmsCoeff = 1.0f - std::exp (-1.0 / (rmsTau * sr));
        envCoeff = 1.0f - std::exp (-1.0 / (envTau * sr));
        gainAttackCoeff = 1.0f - std::exp (-1.0 / (gainAttackTau * sr));
        const double bypassTau = 0.025; // 25 ms enable/disable ramp
        bypassCoeff = 1.0f - std::exp (-1.0 / (bypassTau * sr));
        reset();
        // Pivot follows the signal level with a VERY SLOW time constant (2 s).
        // It represents the long-term average level, not the instantaneous
        // level. Quiet passages (below the pivot) get boosted; loud passages
        // (at/above the pivot) stay at unity. The 2 s TC ensures the pivot
        // doesn't track individual words or notes, only the overall level.
        const double pivotTau = 2.0; // 2 seconds - very slow
        pivotCoeff = 1.0f - std::exp (-1.0 / (pivotTau * sr));
        reset();
    }

    void reset()
    {
        rmsState = 0.0f;
        pivotState = 0.0f;
        gainEnv  = 1.0f;
        bypassGain = enabled ? 1.0f : 0.0f;
    }

        void setEnabled (bool e) { enabled = e; }

        /** Upward amount in 0..1. 0 = bypass behaviour (gain stays unity),
         *  1 = strongest upward lift. */
        void setAmount (float a)
        {
            amount = juce::jlimit (0.0f, 1.0f, a);
            // Map amount to an upward ratio: 1.0 (no lift) .. 8.0 (strong).
            upwardRatio = 1.0f + amount * 7.0f;
        }

        bool isEnabled() const { return enabled; }

        void process (juce::AudioBuffer<float>& buffer)
    {
        // Smooth enable/bypass so toggling Compress on/off fades the processed
        // signal in/out instead of hard-switching (no click). When bypassed and
        // the fade has fully completed, skip all work.
        const float bypassTarget = (enabled && amount > 0.0f) ? 1.0f : 0.0f;
        if (! enabled && amount <= 0.0f && bypassGain < 1.0e-4f)
            return;
        if (enabled && amount <= 0.0f)
        {
            // Effect on but amount is zero -> still ramp bypass in (harmless)
            // and do nothing else; keep detectors warm.
            bypassGain += (bypassTarget - bypassGain) * bypassCoeff;
            return;
        }

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

                // Pivot = VERY slow reference level (2 seconds). It represents
                // the long-term average level of the signal. Quiet passages
                // (below the pivot) are boosted; loud passages (at/above the
                // pivot) stay at unity. The slow pivot ensures the compressor
                // acts on dynamics, not on instantaneous level.
                pivotState += (inst - pivotState) * pivotCoeff;

                // Pivot floor (near-silence): skip compression to avoid
                // amplifying noise.
                const float pivot = juce::jmax (pivotState, 1.0e-4f);

                // Upward gain is computed from the SMOOTHED level detector
                // (rmsState), NOT the raw instantaneous level. Using the raw
                // instantaneous level meant the gate's abrupt opening front
                // (silence -> full signal in ~5 ms) produced a momentary
                // inst/pivot spike that was then amplified by up to 4x and held
                // for ~20-40 ms by the gain envelope, i.e. a louder attack
                // transient whenever Gate + Compress were both on. rmsState has
                // a built-in attack so it does not see that raw front.
                const float level = rmsState;
                float gain;
                if (level < pivot)
                    gain = std::pow (level / pivot, 1.0f / upwardRatio - 1.0f);
                else
                    gain = 1.0f;
                if (! std::isfinite (gain) || gain < 1.0f) gain = 1.0f;
                // Safety ceiling: 8x matches the max upward ratio (8:1).
                gain = juce::jmin (gain, 8.0f);

                // Smooth the gain envelope. Use an asymmetric time constant:
                // the attack (gain rising toward target) is slower (30 ms) so a
                // sudden target jump still ramps up gently instead of
                // instantaneously amplifying the attack transient; the release
                // (gain falling) stays at 20 ms.
                const float gCoeff = (gain > gainEnv) ? gainAttackCoeff : envCoeff;
                gainEnv += (gain - gainEnv) * gCoeff;

                // Crossfade between bypass (unity) and the compressed signal
                // using the smoothed enable gain, so toggling Compress on/off
                // produces no click.
                bypassGain += (bypassTarget - bypassGain) * bypassCoeff;
                const float effectiveGain = 1.0f + (gainEnv - 1.0f) * bypassGain;

                for (int c = 0; c < ch; ++c)
                    buffer.setSample (c, i, buffer.getSample (c, i) * effectiveGain);
            }
        }

    private:
        bool  enabled = false;
        float amount = 0.0f;        // user parameter 0..1
        float upwardRatio = 1.0f;   // derived from amount
        float rmsState = 0.0f;      // running RMS level (linear)
        float pivotState = 0.0f;     // reference level for upward gain (slow symmetric lag)
        float gainEnv  = 1.0f;       // smoothed applied gain
        float rmsCoeff = 0.0f;
        float envCoeff = 0.0f;
        float gainAttackCoeff = 0.0f;
        float pivotCoeff = 0.0f;
        float bypassGain = 1.0f;   // smoothed enable/bypass gain (1=active, 0=bypassed)
        float bypassCoeff = 0.0f;  // 25 ms ramp for click-free enable/disable
        double sampleRate = 44100.0;
    };
}



