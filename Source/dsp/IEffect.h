// IEffect.h
// Abstract interface for audio effects (reverb, delay, chorus, etc.).
// Each effect can be enabled/disabled independently and exposes a wet mix
// parameter. Effects are applied in order after the main DSP pipeline.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    /**
     * Abstract interface for post-processing audio effects.
     * Effects are stacked after the main pitch-correction + harmony pipeline
     * and process the final mixed output buffer.
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