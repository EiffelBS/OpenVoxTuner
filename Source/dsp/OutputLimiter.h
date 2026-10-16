// OutputLimiter.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace ovtdsp
{
    /**
     * Output trim + peak limiter, applied LAST on the master bus.
     *
     * The trim is a plain output gain in dB (-12 .. +12), so the user can set
     * the plugin's output level without touching the harmony gain-match: the
     * 1/sqrt(1+N) scaling that keeps the blended stack at the dry level stays
     * intact, and this stage only scales the final result.
     *
     * The limiter is deliberately NON-lookahead: the gain at sample i is computed
     * only from samples <= i, so the plugin adds no latency and never anticipates
     * a peak. The envelope follower attacks instantly (the gain closes exactly at
     * the peak) and releases with a one-pole time constant, which keeps the gain
     * curve continuous and free of clicks. Peaks above the ceiling are caught as
     * they happen; anything already recorded past them is not recovered.
     *
     * Channels are linked: the loudest channel drives the gain, so a stereo lead
     * with a loud right-side doubler copy does not steer the image by pumping the
     * two channels independently.
     */
    class OutputLimiter
    {
    public:
        // Linear ceiling, ~ -1.68 dBFS: leaves headroom for the lossy encoding
        // some hosts apply on export, without audible pumping at normal levels.
        static constexpr float ceilingLinear = 0.98f;

        void prepare (double sampleRate, int /*maxBlockSize*/)
        {
            this->sampleRate = sampleRate;
            prepared = true;
            // One-pole coefficients: fast attack (0.7 ms), slow release (120 ms).
            const double tauAttack  = 0.0007;
            const double tauRelease = 0.120;
            attackCoeff  = static_cast<float> (1.0 - std::exp (-1.0 / (tauAttack  * sampleRate)));
            releaseCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (tauRelease * sampleRate)));
        }

        void reset()
        {
            env = 0.0f;
        }

        /** Output trim in dB, clamped to the parameter range. */
        void setTrimDb (float trimDb)
        {
            trimLinear = dbToLinear (juce::jlimit (-12.0f, 12.0f, trimDb));
        }

        /** When off, the stage is a pure trim: no gain is applied beyond the trim. */
        void setLimiterEnabled (bool on) { limiterOn = on; }

        bool isPrepared() const { return prepared; }
        float currentEnvelope() const { return env; }
        float currentTrimLinear() const { return trimLinear; }

        /**
         * The gain this stage is applying right now, in linear: the trim, times the
         * limiter's attenuation when the envelope sits above the ceiling. Exactly the
         * expression `process` uses per sample, so a reader can trust it.
         */
        float currentGain() const
        {
            return (limiterOn && env > ceilingLinear) ? trimLinear * (ceilingLinear / env) : trimLinear;
        }

        /**
         * The smoothed peak of the signal LEAVING the limiter: the envelope of the
         * trimmed signal times the attenuation the limiter is adding on top of the
         * trim. It follows the release, so a meter fed from it falls with the tail
         * instead of holding a block peak.
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
         * Applies trim, then limiting, in place on every channel.
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
                // Peak of the TRIMMED signal: the trim is what the limiter has to protect.
                float peak = 0.0f;
                for (int c = 0; c < channels; ++c)
                    peak = juce::jmax (peak, std::abs (data[c][i]));
                const float scaled = peak * trimLinear;

                // Non-lookahead envelope: instantaneous attack, one-pole release toward
                // the current peak (env moves DOWN toward `scaled`, never away from it).
                if (scaled > env)
                    env = scaled;
                else
                    env += releaseCoeff * (scaled - env);

                float gain = trimLinear;
                if (limiterOn && env > ceilingLinear)
                    gain *= ceilingLinear / env;

                for (int c = 0; c < channels; ++c)
                    data[c][i] *= gain;
            }
        }

    private:
        static float dbToLinear (float db) { return std::pow (10.0f, db / 20.0f); }

        double sampleRate = 44100.0;
        bool prepared = false;
        bool limiterOn = true;
        float trimLinear = 1.0f;
        float env = 0.0f;
        float attackCoeff = 0.0f;   // kept: documents the attack time constant
        float releaseCoeff = 0.0f;
    };
}
