// DeEsser.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "IEffect.h"

namespace ovtdsp
{
    /**
     * Sibilance suppressor (de-esser) for the corrected lead voice.
     *
     * Why it is built this way:
     *
     * - Band-limited detector. A 2-pole high-pass at fLow followed by a 2-pole
     *   low-pass at fHigh isolates the sibilance band around the centre
     *   frequency (edges at 70 % and 130 % of the centre, so the default 7 kHz
     *   covers the classic 5-8 kHz region with a gentle 12 dB/oct skirt). ONLY
     *   this band feeds the detector. A full-band detector is the classic de-esser
     *   failure: plosives, consonants and loud vowels carry their energy far below
     *   5 kHz, so "p", "b" and a belted note would duck the whole voice. Band-limited
     *   detection is what makes the module react to "s", "t" and "sh" and nothing else.
     *
     * - Broadband reduction. The gain is applied to the WHOLE signal, not to the
     *   isolated band. A split-band de-esser subtracts the processed band from the
     *   full signal; with a phase-shifted 2-pole band that subtraction comb-filters
     *   the voice (audible hollowing on the sustained vowels). Wide-band reduction
     *   is what a studio engineer does with a compressor keyed from an HF band: the
     *   whole voice ducks for a few milliseconds whenever sibilance spikes.
     *
     * - Detector: the same smoothed per-sample RMS motif as NoiseGate (10 ms time
     *   constant), measured on the band output and taken from the LOUDEST channel so
     *   both channels share one gain. A per-channel detector would duck L and R
     *   independently, wobbling the stereo image and changing the L+R sum. Because
     *   the level is an RMS, a sine at the threshold reads ~3 dB below its peak, so
     *   the open point (threshold + 6 dB) sits ~9 dB below the peak amplitude of a
     *   tone that triggers it -- the same convention the gate uses.
     *
     * - Hysteresis: engage at +6 dB over the threshold, disengage at the threshold.
     *   Without it a signal sitting exactly on the threshold chatters open/closed.
     *
     * - Fast attack (5 ms) / slow release (80 ms), ramped per sample. A hard gain
     *   toggle clicks, so every change is a one-pole ramp, exactly like NoiseGate's
     *   gate and UpwardCompressor's bypass. Max reduction caps the floor at
     *   10^(-maxReduction/20) so the voice is never ducked to silence.
     *
     * - Enable/disable is ramped over 25 ms (bypassGain crossfade) and the pass is
     *   skipped once the ramp has decayed: a disabled de-esser costs no CPU and
     *   leaves the buffer bit-identical to its input.
     *
     * State (detector, gain, hysteresis, bypass ramp) persists across blocks, so a
     * release continues exactly where the previous block ended.
     */
    class DeEsser : public IEffect
    {
    public:
        juce::String getId() const override    { return "deesser"; }
        juce::String getName() const override   { return "De-Esser"; }

        void prepare (double sampleRate, int maximumBlockSize) override
        {
            currentSampleRate = static_cast<float> (sampleRate);
            detectorCoeff = 1.0f - std::exp (-1.0f / (0.010f * currentSampleRate)); // 10 ms  detector
            attackCoeff   = 1.0f - std::exp (-1.0f / (0.005f * currentSampleRate)); // 5 ms   attack
            releaseCoeff  = 1.0f - std::exp (-1.0f / (0.080f * currentSampleRate)); // 80 ms  release
            bypassCoeff   = 1.0f - std::exp (-1.0f / (0.025f * currentSampleRate)); // 25 ms  enable ramp

            bandDirty  = true; // the band edges depend on the sample rate
            depthDirty = true;
        }

        void reset() override
        {
            for (auto& band : bands) band.reset();
            detector   = 0.0f;
            engaged    = false;
            currentGain = 1.0f;
            bypassGain  = 0.0f; // fully bypassed: the ramp fades the reduction in
        }

        /** Detector threshold, dB below full scale (-60..0). */
        void setThresholdDb (float db)     { thresholdDb = db; depthDirty = true; }
        /** Depth cap: how much the gain may be ducked, in dB (0..24). */
        void setMaxReductionDb (float db)  { maxReductionDb = db; depthDirty = true; }
        /** Centre of the sibilance band, in Hz (3000..12000). */
        void setFrequencyHz (float hz)     { centreFreqHz = hz; bandDirty = true; }

        // Diagnostics for the unit tests: the live gain the detector is ramping
        // through, and whether the detector currently holds the de-esser engaged.
        float getCurrentGain() const { return currentGain; }
        bool  isEngaged() const      { return engaged; }
        float getDetector() const    { return detector; } // smoothed band RMS

        void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) override
        {
            const int numChannels = buffer.getNumChannels();
            const int numSamples  = buffer.getNumSamples();
            if (numChannels <= 0 || numSamples <= 0) return;

            // Disabled and the ramp has decayed: leave the buffer untouched, so a
            // bypassed de-esser is bit-identical to its input and costs no CPU.
            if (! enabled && bypassGain < 1.0e-4f) return;

            if (bandDirty)  updateBandEdges();
            if (depthDirty) updateDepth();

            const float mix = juce::jlimit (0.0f, 1.0f, wetMix);
            const int detChannels = juce::jmin (numChannels, 2); // stereo-linked detector

            for (int s = 0; s < numSamples; ++s)
            {
                // Detector: per-sample RMS of the sibilance band, stereo-linked over
                // the loudest channel. Same smoothed-RMS motif as NoiseGate, so the
                // threshold reads in the same dBFS convention as the gate.
                float sumSq = 0.0f;
                for (int c = 0; c < detChannels; ++c)
                {
                    const float b = bands[c].process (buffer.getSample (c, s));
                    sumSq += b * b;
                }
                const float bandLevel = std::sqrt (sumSq / float (detChannels));

                detector += (bandLevel - detector) * detectorCoeff;

                // Hysteresis: engage above threshold + 6 dB, release at the threshold.
                if (! engaged)
                {
                    if (detector > openLevel) engaged = true;
                }
                else if (detector < closeLevel)
                {
                    engaged = false;
                }

                // Gain ramp: fast attack into the depth floor, slow release back to unity.
                const float target = engaged ? reductionFloor : 1.0f;
                currentGain += (target - currentGain) * (target < currentGain ? attackCoeff : releaseCoeff);

                // Enable ramp: 25 ms crossfade so switching cannot click.
                bypassGain += ((enabled ? 1.0f : 0.0f) - bypassGain) * bypassCoeff;

                // Broadband reduction: depth runs from 0 (bypassed) to mix (fully engaged).
                const float depth = bypassGain * mix * (1.0f - currentGain);
                for (int c = 0; c < numChannels; ++c)
                    buffer.setSample (c, s, buffer.getSample (c, s) * (1.0f - depth));
            }
        }

    private:
        /**
         * The sibilance band: a 2-pole high-pass at fLow cascaded into a 2-pole
         * low-pass at fHigh, written inline. The plugin target links juce_dsp
         * (juce::BiquadFilter) and the test target does not, so keeping the filter
         * local makes this header usable from both without a build change.
         */
        struct Section
        {
            // Coefficients are stored pre-normalised by a0, as FormantPreserver does.
            void setCoefficients (float b0, float b1, float b2, float a1, float a2)
            {
                cb0 = b0; cb1 = b1; cb2 = b2; ca1 = a1; ca2 = a2;
            }
            void reset() { x1 = x2 = y1 = y2 = 0.0f; }
            float process (float x)
            {
                const float y = cb0 * x + cb1 * x1 + cb2 * x2 - ca1 * y1 - ca2 * y2;
                x2 = x1; x1 = x; y2 = y1; y1 = y;
                return y;
            }
            float cb0 = 1.0f, cb1 = 0.0f, cb2 = 0.0f, ca1 = 0.0f, ca2 = 0.0f;
            float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        };

        /** One channel's band: high-pass then low-pass, in series. */
        struct BandSection
        {
            void setEdges (float fLowHz, float fHighHz, float sr)
            {
                setHighPass (high, fLowHz,  sr);
                setLowPass  (low,  fHighHz, sr);
            }
            void reset() { high.reset(); low.reset(); }
            float process (float x) { return low.process (high.process (x)); }
            Section high, low;
        };

        // RBJ cookbook biquads at Q = 0.7071 (the Butterworth-spacing reference),
        // coefficients divided by a0 up front so the state update needs no division.
        static void setHighPass (Section& f, float freqHz, float sr)
        {
            const float w     = 2.0f * juce::MathConstants<float>::pi * freqHz / sr;
            const float cosw  = std::cos (w);
            const float alpha = std::sin (w) / (2.0f * 0.70710678118654752f);
            const float a0    = 1.0f + alpha;

            f.setCoefficients ((1.0f + cosw) * 0.5f / a0,
                               - (1.0f + cosw) / a0,
                               (1.0f + cosw) * 0.5f / a0,
                               - 2.0f * cosw / a0,
                               (1.0f - alpha) / a0);
        }

        static void setLowPass (Section& f, float freqHz, float sr)
        {
            const float w     = 2.0f * juce::MathConstants<float>::pi * freqHz / sr;
            const float cosw  = std::cos (w);
            const float alpha = std::sin (w) / (2.0f * 0.70710678118654752f);
            const float a0    = 1.0f + alpha;

            f.setCoefficients ((1.0f - cosw) * 0.5f / a0,
                               (1.0f - cosw) / a0,
                               (1.0f - cosw) * 0.5f / a0,
                               - 2.0f * cosw / a0,
                               (1.0f - alpha) / a0);
        }

        void updateDepth()
        {
            const float thresholdLinear = std::pow (10.0f, thresholdDb / 20.0f);
            openLevel      = thresholdLinear * 2.0f; // +6 dB hysteresis, as in NoiseGate
            closeLevel     = thresholdLinear;
            reductionFloor = std::pow (10.0f, -maxReductionDb / 20.0f); // 0 dB cap -> unity gain
            depthDirty     = false;
        }

        void updateBandEdges()
        {
            const float nyquist = currentSampleRate * 0.5f;
            const float fLow    = juce::jlimit (300.0f, nyquist - 200.0f, centreFreqHz * 0.7f);
            const float fHigh   = juce::jlimit (fLow + 200.0f, nyquist - 200.0f, centreFreqHz * 1.3f);

            for (auto& band : bands)
                band.setEdges (fLow, fHigh, currentSampleRate);
            bandDirty = false;
        }

        float currentSampleRate = 44100.0f;
        float detectorCoeff = 0.0f, attackCoeff = 0.0f, releaseCoeff = 0.0f, bypassCoeff = 0.0f;

        float thresholdDb     = -25.0f;
        float maxReductionDb  = 12.0f;
        float centreFreqHz    = 7000.0f;

        float openLevel      = 0.35565f;
        float closeLevel     = 0.17783f;
        float reductionFloor = 0.25119f;

        BandSection bands[2];
        float detector   = 0.0f;
        float currentGain = 1.0f;
        float bypassGain  = 0.0f;
        bool  engaged     = false;
        bool  bandDirty   = true;
        bool  depthDirty  = true;
    };
}
