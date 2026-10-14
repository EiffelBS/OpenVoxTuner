// Doubler.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "BlockAwareOnePole.h"

namespace ovtdsp
{
    /**
     * Stereo doubler (Haas-style doubling) of a single voice.
     *
     * The caller feeds a pitch-shifted COPY of the corrected lead into
     * `process()`. The copy is:
     *   - delayed by a Haas offset (0 -> 25 ms, never below 5 ms once active),
     *   - panned opposite to the lead with an equal-power law normalised to
     *     unity at centre, so amount = 0 leaves the lead path untouched,
     *   - mixed under a constant-gain compensation `1/sqrt(1+g^2)` applied to
     *     the lead AND the copy, so the perceived RMS stays flat across the
     *     whole 0 -> 100 % range. The compensation is independent of the
     *     harmony `gain match` switch: the doubler adds to the dry signal, so
     *     it must self-compensate even when harmony is off.
     *
     * Detune is NOT applied here (the caller owns the pitch shifter); this
     * class only exposes the mapping amount -> cents / ratio as static helpers
     * so the plugin and the tests share one definition.
     *
     * At amount = 0 the module is inactive: `isActive()` is false, the caller
     * skips the pitch-shifter call entirely (CPU short-circuit) and `process()`
     * is a no-op that leaves `main` bit-identical.
     */
    class Doubler
    {
    public:
        Doubler() = default;

        void prepare (double sampleRate, int maxBlockSize) noexcept;
        void reset() noexcept;

        /** Knob value in percent (0..100). Stored as the block target. */
        void setAmount (float amountPercent) noexcept
        {
            targetAmount = juce::jlimit (0.0f, 100.0f, amountPercent);
        }

        /** True when a copy must be produced. The caller uses this to skip the
         *  dedicated pitch shifter, which is the CPU short-circuit at 0 %. */
        bool isActive() const noexcept { return targetAmount > 0.0f; }

        float amount() const noexcept { return targetAmount; }

        /** Amount -> detune in cents: 0 % = 0 cent, 100 % = 10 cents.
         *  Capped at 10 cents on purpose (docs/harmony-pad-prerequisites-analysis.md
         *  "Keep detunes < 10 cents"): beyond that the lead + copy pair starts
         *  beating audibly and reads as a chorus/flanger rather than a doubler. */
        static float detuneCents (float amountPercent) noexcept
        {
            return juce::jlimit (0.0f, 100.0f, amountPercent) * 0.10f;
        }

        /** Amount -> pitch ratio: 2^(cents/1200). 100 % = 1.0058 (10 cents sharp). */
        static float detuneRatio (float amountPercent) noexcept
        {
            return std::pow (2.0f, detuneCents (amountPercent) / 1200.0f);
        }

        /** Haas offset in samples for a given amount (0 -> 25 ms, floored at
         *  5 ms once active so the copy is always decorrelated from the lead). */
        static float haasDelaySamples (float amountPercent, double sampleRate) noexcept
        {
            const float s = juce::jlimit (0.0f, 100.0f, amountPercent) / 100.0f;
            float ms = s * 25.0f;
            if (s > 0.0f)
                ms = juce::jmax (5.0f, ms);   // Haas floor: below 5 ms the pair comb-filters
            return ms * 0.001f * static_cast<float> (sampleRate);
        }

        /** Copy level relative to the lead for a given amount (0 -> 0.6, i.e.
         *  -4.4 dB at 100 %). The copy is always at or below the lead. */
        static float copyLevel (float amountPercent) noexcept
        {
            return (juce::jlimit (0.0f, 100.0f, amountPercent) / 100.0f) * 0.6f;
        }

        /** Constant-gain compensation `1/sqrt(1+g^2)` for a given amount. */
        static float gainCompensation (float amountPercent) noexcept
        {
            const float g = copyLevel (amountPercent);
            return 1.0f / std::sqrt (1.0f + g * g);
        }

        /** Equal-power pan gains, normalised to unity at centre:
         *  pan = 0 -> (1, 1), pan = +1 -> (0, sqrt(2)), pan = -1 -> (sqrt(2), 0). */
        static void panGains (float pan, float& leftGain, float& rightGain) noexcept
        {
            const float p = juce::jlimit (-1.0f, 1.0f, pan);
            const float angle = (p + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            leftGain  = std::sqrt (2.0f) * std::cos (angle);
            rightGain = std::sqrt (2.0f) * std::sin (angle);
        }

        /**
         * Mix the delayed, panned copy into `main` in place.
         *
         * @param copy  the pitch-shifted copy (same block length as `main`)
         * @param main  the lead buffer, modified in place
         */
        void process (const juce::AudioBuffer<float>& copy,
                      juce::AudioBuffer<float>& main) noexcept;

    private:
        double sampleRate = 44100.0;
        int    lineSize   = 0;
        juce::int64 writePos = 0;
        float  targetAmount = 0.0f;
        float  prevAmount   = 0.0f;   // amount applied on the previous block (ramp start)
        float  curAmount    = 0.0f;   // smoothed amount for this block (ramp end)
        juce::AudioBuffer<float> delayLine;
    };
}
