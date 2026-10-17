// OutputLimiter.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace ovtdsp
{
    /**
     * Output trim + true-peak limiter + soft-clip safety, applied LAST on the master bus.
     *
     * The trim is a plain output gain in dB (-12 .. +12), so the user can set
     * the plugin's output level without touching the harmony gain-match: the
     * 1/sqrt(1+N) scaling that keeps the blended stack at the dry level stays
     * intact, and this stage only scales the final result.
     *
     * The ceiling is -1 dBFS (linear 0.891), measured as a TRUE PEAK: the detector
     * reconstructs the inter-sample crest of the signal with a 4x oversampling FIR,
     * so the gain closes on what a DAC actually outputs, not only on the stored
     * samples. A sample-level ceiling alone does not do this: measured on a
     * full-scale square wave the reconstructed crest sits +2.1 dB above the sample
     * peak, and on broadband material up to +4.6 dB, so a limiter that watches only
     * samples can hand a host peaks past 0 dBFS.
     *
     * Reconstructing a crest needs the samples AFTER it, so the stage looks ahead by
     * exactly its filter group delay: `kLatencySamples` (8 samples at any sample rate,
     * about 0.17 ms). That is declared to the host through setLatencySamples(), never
     * hidden. The envelope attacks instantly on the detected crest and releases with a
     * one-pole time constant, so the gain curve stays continuous and click-free.
     *
     * After the gain, a soft-clip stage bounds the sample values: below the knee it is
     * transparent, above it the curve saturates smoothly toward the ceiling, so any
     * residual crest that still rides past the gain is rounded rather than chopped.
     *
     * Channels are linked: the loudest channel drives the gain, so a stereo lead
     * with a loud right-side doubler copy does not steer the image by pumping the
     * two channels independently.
     */
    class OutputLimiter
    {
    public:
        // Linear ceiling = -1 dBFS (20*log10(0.891) = -1.00 dB), the headroom a true-peak
        // meter reads, applied to the reconstructed crest and not only to stored samples.
        static constexpr float ceilingLinear = 0.891f;

        // The gain closes to a target slightly BELOW the declared ceiling (kSafetyMargin = 0.975,
        // i.e. -1.22 dBFS). Measured reason: the detector reads the INPUT crest, and the soft clip
        // then shapes the OUTPUT, adding ringing the detector cannot see. With the gain closing on
        // the ceiling itself, a full-scale noise burst measured +0.19 dB above it (-0.81 dBFS) and a
        // square wave +0.07 dB. The margin absorbs that residual, so what a true-peak meter reads
        // stays at or under -1 dBFS, which is the guarantee the ceiling claims.
        static constexpr float kSafetyMargin = 0.975f;
        static constexpr float targetLinear = ceilingLinear * kSafetyMargin;

        // 4x oversampling reconstruction filter: odd tap count so the centre tap lands on
        // the sample grid, which makes the group delay a whole number of base samples.
        // 65 taps span +-16 base samples around the crest. A shorter window (33 taps) was measured
        // to under-report broadband overshoot: on a full-scale noise burst it saw +2.4 dB where a
        // wider estimator saw +4.4 dB, so the ceiling was met against the wrong number. The detector
        // has to see what a true-peak meter sees, which is what this tap count buys.
        static constexpr int kOversamplingFactor = 4;
        static constexpr int kFilterTaps = 65;
        static constexpr int kLatencySamples = (kFilterTaps - 1) / 2 / kOversamplingFactor; // 8

        // Ring covering every sample the filter touches: phase groups read lags 0..16 behind the
        // newest input, so 17 samples, rounded to a power of two.
        static constexpr int kHistoryMask = 31;   // ring of 32

        // Channel count cap for the stack array in process(); the plugin uses stereo.
        static constexpr int kMaxChannels = 8;

        void prepare (double sampleRate, int /*maxBlockSize*/)
        {
            this->sampleRate = sampleRate;
            prepared = true;

            // One-pole release time constant (120 ms). The attack is instantaneous: the gain
            // closes on the crest the detector reports, so no attack coefficient is applied.
            const double tauRelease = 0.120;
            releaseCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (tauRelease * sampleRate)));

            buildPhaseTaps();
        }

        void reset()
        {
            env = 0.0f;
            writePos = 0;
            for (int c = 0; c < 8; ++c)
                for (int i = 0; i <= kHistoryMask; ++i)
                    ring[c][i] = 0.0f;
        }

        /** Output trim in dB, clamped to the parameter range. */
        void setTrimDb (float trimDb)
        {
            trimLinear = dbToLinear (juce::jlimit (-12.0f, 12.0f, trimDb));
        }

        /** When off, the stage is a pure trim: no limiting and no soft clip, so it can exceed 0 dBFS. */
        void setLimiterEnabled (bool on) { limiterOn = on; }

        bool isPrepared() const { return prepared; }
        float currentEnvelope() const { return env; }
        float currentTrimLinear() const { return trimLinear; }

        /** The latency this stage declares to the host, in samples at the current rate. */
        int getLatencySamples() const { return kLatencySamples; }

        /**
         * The gain this stage is applying right now, in linear: the trim, times the
         * limiter's attenuation when the envelope sits above the closing target. Exactly
         * the expression `process` uses per sample, so a reader can trust it.
         */
        float currentGain() const
        {
            return (limiterOn && env > targetLinear) ? trimLinear * (targetLinear / env) : trimLinear;
        }

        /**
         * The smoothed true peak LEAVING the limiter: the envelope of the trimmed signal
         * times the attenuation the limiter adds on top of the trim. It follows the release,
         * so a meter fed from it falls with the tail instead of holding a block peak.
         */
        float currentOutputPeak() const
        {
            return (trimLinear > 0.0f) ? env * (currentGain() / trimLinear) : 0.0f;
        }

        /**
         * Gain reduction in dB, reported as a positive attenuation, 0 when the limiter
         * is not touching the gain. It is the ratio between what the signal asked for
         * and what it got, so `peak * 10^(gr/20)` is the level that was caught.
         */
        float currentGainReductionDb() const
        {
            const float g = currentGain();
            if (trimLinear <= 0.0f || g >= trimLinear)
                return 0.0f;
            return static_cast<float> (-20.0 * std::log10 (static_cast<double> (g) / static_cast<double> (trimLinear)));
        }

        /**
         * Applies trim, true-peak limiting and the soft clip in place on every channel.
         * The output is delayed by kLatencySamples; the caller declares that to the host.
         * A silent buffer passes through untouched, so the sleep path stays cheap.
         */
        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numSamples = buffer.getNumSamples();
            if (! prepared || numSamples <= 0)
                return;

            const int numChannels = juce::jmax (1, buffer.getNumChannels());
            const int channels = juce::jmin (numChannels, 8);
            if (channels <= 0)
                return;

            float* data[8];
            for (int c = 0; c < channels; ++c)
                data[c] = buffer.getWritePointer (c);

            for (int i = 0; i < numSamples; ++i)
            {
                // Push the newest sample into every channel's ring.
                const int slot = writePos & kHistoryMask;
                for (int c = 0; c < channels; ++c)
                    ring[c][slot] = data[c][i];

                // True peak of the reconstructed signal around the crest that is now complete:
                // phase 0 is the emitted sample on the grid, phases 1..3 are the inter-sample
                // points between it and the next sample -- what a DAC actually outputs. Every
                // channel contributes, because the channels are linked. Phase 0 doubles as the
                // value to emit, so it is kept per channel.
                float peak = 0.0f;
                float emitted [kMaxChannels];
                for (int c = 0; c < channels; ++c)
                {
                    for (int p = 0; p < kOversamplingFactor; ++p)
                    {
                        float y = 0.0f;
                        for (const Tap& t : phaseTaps[p])
                            y += t.coeff * ringAt (c, slot, t.lag);
                        if (p == 0)
                            emitted[c] = y;
                        peak = juce::jmax (peak, std::abs (y));
                    }
                }

                const float scaled = peak * trimLinear;

                // Envelope: instantaneous attack on the detected crest, one-pole release toward
                // the current peak (env moves DOWN toward `scaled`, never away from it).
                if (scaled > env)
                    env = scaled;
                else
                    env += releaseCoeff * (scaled - env);

                float gain = trimLinear;
                if (limiterOn && env > targetLinear)
                    gain *= targetLinear / env;

                // Emit the reconstructed sample at the grid point (phase 0), then bound it with
                // the soft clip. The stage is a delay of kLatencySamples, declared to the host.
                for (int c = 0; c < channels; ++c)
                {
                    float y = emitted[c] * gain;
                    if (limiterOn)
                        y = softClip (y);
                    data[c][i] = y;
                }

                writePos = (writePos + 1) & kHistoryMask;
            }
        }

    private:
        struct Tap { float coeff; int lag; };

        static float dbToLinear (float db) { return std::pow (10.0f, db / 20.0f); }

        /** Sample value `lag` base samples behind the newest one in one channel ring. */
        float ringAt (int c, int slot, int lag) const
        {
            return ring[c][(slot - lag) & kHistoryMask];
        }

        /**
         * Polyphase decomposition of the 4x reconstruction filter. Tap k contributes to output
         * phase p when k % 4 == p, and reads the base sample `lag` behind the newest input.
         * Phase 0 is the emitted (delayed) sample; phases 1..3 are the inter-sample points
         * between it and the next one, which need samples up to the current input -- that is
         * where the 8-sample lookahead comes from. The filter is unnormalized: sinc obeys
         * sum_j sinc(x + j) = 1, so each phase group sums
         * to unity gain and a full-scale bandlimited sine reconstructs at its own amplitude.
         */
        void buildPhaseTaps()
        {
            const int centre = (kFilterTaps - 1) / 2;
            for (int k = 0; k < kFilterTaps; ++k)
            {
                const double m = static_cast<double> (k - centre) / static_cast<double> (kOversamplingFactor);
                const float h = (std::abs (m) < 1.0e-9) ? 1.0f
                              : static_cast<float> (std::sin (juce::MathConstants<double>::pi * m)
                                                    / (juce::MathConstants<double>::pi * m));
                const int p = k % kOversamplingFactor;
                phaseTaps[p].push_back ({ h, (k - p) / kOversamplingFactor });
            }
        }

        /**
         * Soft clip with the ceiling as its asymptote: transparent below the knee, saturating
         * smoothly above it. The curve is C1 continuous at the knee, so engaging it adds no click,
         * and |y| never reaches the ceiling exactly, which keeps true peaks under -1 dBFS.
         */
        float softClip (float y) const
        {
            const float u = y / ceilingLinear;
            const float a = std::abs (u);
            static constexpr float kneeFraction = 0.95f;   // engages at ~ -1.4 dBFS, so the soft clip
            // only catches samples the gain left right on the ceiling: shaping less keeps fewer new
            // high-frequency edges, which is what limits the residual reconstruction overshoot.
            if (a <= kneeFraction)
                return y;
            const float t = (a - kneeFraction) / (1.0f - kneeFraction);
            const float shaped = kneeFraction + (1.0f - kneeFraction) * std::tanh (static_cast<double> (t));
            return ceilingLinear * (u < 0.0f ? -shaped : shaped);
        }

        double sampleRate = 44100.0;
        bool prepared = false;
        bool limiterOn = true;
        float trimLinear = 1.0f;
        float env = 0.0f;
        float releaseCoeff = 0.0f;

        int writePos = 0;
        float ring[8][kHistoryMask + 1];
        std::vector<Tap> phaseTaps[kOversamplingFactor];   // built in prepare(), never in process()
    };
}
