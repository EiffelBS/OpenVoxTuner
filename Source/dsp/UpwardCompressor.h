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
        const double envTau  = 0.020; // 20 ms gain-envelope smoothing (downward)
        const double gainAttackTau = 0.030; // 30 ms gain-envelope attack (upward)
        rmsCoeff = 1.0f - std::exp (-1.0 / (rmsTau * sr));
        envCoeff = 1.0f - std::exp (-1.0 / (envTau * sr));
        gainAttackCoeff = 1.0f - std::exp (-1.0 / (gainAttackTau * sr));
        const double bypassTau = 0.025; // 25 ms enable/disable ramp
        bypassCoeff = 1.0f - std::exp (-1.0 / (bypassTau * sr));
        reset();
        // Pivot follows the signal level with a SLOW, SYMMETRIC time constant.
        // It must never jump quickly in either direction: a fast attack would
        // momentarily see the gate's opening front as a huge inst/pivot ratio
        // (loud "clac"), and a fast release would let it collapse toward ~0
        // right after the gate opens. A constant slow lag (150 ms) keeps the
        // pivot near the signal's real average so quiet parts are still lifted
        // but no transient spike is ever amplified.
        const double pivotTau = 0.150; // 150 ms slow, symmetric
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
            // Map amount to an upward ratio: 1.0 (no lift) .. 4.0 (strong).
            upwardRatio = 1.0f + amount * 3.0f;
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

                // Pivot = slow, symmetric reference level. Because it lags by a
                // constant 150 ms in both directions, it never spikes on the
                // gate's opening front nor collapses after the gate closes, so
                // the inst/pivot ratio stays bounded and no "clac" is produced.
                pivotState += (inst - pivotState) * pivotCoeff;

                // Pivot floor (near-silence): skip compression to avoid
                // amplifying noise. The slow pivot already sits near the real
                // signal level, so quiet attacks are not multiplied by a tiny
                // pivot.
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
                // Safety ceiling: 4x is plenty for an upward lift and, combined
                // with the slow pivot, guarantees no transient spike survives.
                gain = juce::jmin (gain, 4.0f);

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
