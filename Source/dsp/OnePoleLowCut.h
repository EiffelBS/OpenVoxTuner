// OnePoleLowCut.h
// One-pole high-pass (low-cut) used as a pre-send filter on the lead send.
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <cmath>

namespace ovtdsp
{
    /**
     * A single-pole low-cut (high-pass) filter, one per audio channel.
     *
     * Used on the lead send before it enters the shared room: a vocal sent to a
     * long room carries low-frequency energy (breath, mic handling, the body of
     * the voice below 150 Hz) that the room smears into a muddy rumble. Cutting
     * the lows BEFORE the send keeps the room bright without touching the dry
     * voice, which is what makes a pre-send filter worth having.
     *
     * Why this is written inline instead of reusing BlockAwareOnePole.h: that
     * class is a scalar VALUE smoother (it steps once per block, alpha = 1 -
     * exp(-blockDuration/tau)) meant for gain and parameter ramps. It never sees
     * a sample, so it cannot filter audio. This is the audio-rate one-pole: the
     * same single-pole maths, applied per sample.
     *
     * Implementation: a one-pole low-pass in parallel with a subtractive
     * high-pass, i.e. `lp += a * (x - lp)` then `out = x - lp`. That is the
     * classic 12 dB/octave rolloff with its -3 dB point at the cutoff, it has no
     * feedback coefficients to diverge, and it blocks DC by construction (a
     * constant input drives lp to x, so the output goes to zero).
     *
     * No allocation, no heap, no JUCE dependency: the plugin target links
     * juce_dsp (juce::BiquadFilter) and the test target does not, so a local
     * filter is the pattern this project already uses for send-side filtering.
     */
    struct OnePoleLowCut
    {
        /**
         * Prepare the pole for a given sample rate and cutoff.
         * A cutoff of 0 (or negative) bypasses the filter: the send passes through
         * untouched, which is how a "no pre-send filter" state is expressed.
         */
        void prepare (double sampleRate, float cutoffHz)
        {
            if (cutoffHz <= 0.0f || sampleRate <= 0.0)
            {
                bypass = true;
                coeff = 0.0f;
            }
            else
            {
                // Keep the pole below Nyquist: a cutoff above sr/2 is meaningless,
                // clamp to a quarter of the sample rate so alpha stays well inside (0, 1).
                const float fc = cutoffHz < static_cast<float> (sampleRate * 0.25)
                                 ? cutoffHz : static_cast<float> (sampleRate * 0.25);
                coeff = 1.0f - static_cast<float> (std::exp (-2.0 * kPi * static_cast<double> (fc) / sampleRate));
                bypass = false;
            }
            state = 0.0f;
        }

        /** Clear the filter memory (used by reset() and on a send tap change). */
        void reset()
        {
            state = 0.0f;
        }

        /** Filter one sample. */
        float process (float x)
        {
            if (bypass)
                return x;

            state += coeff * (x - state);   // one-pole low-pass of x
            return x - state;               // complementary high-pass = the low-cut
        }

        /** Current pole coefficient (0 .. 1), exposed for tests and diagnostics. */
        float getCoefficient() const { return coeff; }
        bool isBypassed() const { return bypass; }

    private:
        static constexpr double kPi = 3.14159265358979323846;

        float coeff = 0.0f;
        float state = 0.0f;
        bool bypass = true;
    };
}

