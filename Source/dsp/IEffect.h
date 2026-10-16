// IEffect.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    /**
     * Abstract interface for vocal effects.
     *
     * An effect is registered on one of the three tap points of the chain
     * (see ovtdsp::EffectsBus): the corrected lead voice before any harmony is
     * added, the harmony bus before it is mixed into the output, or the master
     * output after every bus has been blended. Each bus owns its OWN instances,
     * so the same class can run twice with different fixed settings (a short room
     * on the lead, a long wide one on the harmony pad). Processing is in-place on
     * the tapped buffer; the effect never reads parameters, the caller's wiring
     * computes `enabled` and `wetMix` from them.
     */
    class IEffect
    {
    public:
        virtual ~IEffect() = default;

        /** Unique identifier for this effect type (e.g. "reverb"). */
        virtual juce::String getId() const = 0;

        /** Human-readable name (e.g. "Reverb"). */
        virtual juce::String getName() const = 0;

        /** Prepare with sample rate and max block size. */
        virtual void prepare (double sampleRate, int maximumBlockSize) = 0;

        /** Reset internal state (flush delays, reverb tails, etc.). */
        virtual void reset() = 0;

        /**
         * Apply the effect to the audio buffer.
         * @param buffer      The audio buffer to process (in-place).
         * @param enabled     Whether the effect is currently enabled.
         * @param wetMix      Wet/dry mix ratio (0.0 = dry only, 1.0 = full wet).
         */
        virtual void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) = 0;
    };
}


