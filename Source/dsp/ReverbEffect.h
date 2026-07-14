// ReverbEffect.h
// Reverb effect implementation using juce::Reverb.
// A simple, musical reverb with a single wet/dry mix control.
// Designed for vocal monitoring comfort in standalone mode.

#pragma once

#include "IEffect.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace ovtdsp
{
    class ReverbEffect : public IEffect
    {
    public:
        ReverbEffect();
        ~ReverbEffect() override = default;

        // IEffect interface
        juce::String getId() const override   { return "reverb"; }
        juce::String getName() const override { return "Reverb"; }
        void prepare (double sampleRate, int maximumBlockSize) override;
        void reset() override;
        void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) override;

        /** Adjust reverb room size (0.0 .. 1.0). */
        void setRoomSize (float size);

        /** Adjust reverb damping (0.0 .. 1.0). */
        void setDamping (float damping);

    private:
        juce::Reverb reverb;
        juce::Reverb::Parameters params;
        double sampleRate = 44100.0;
        bool prepared = false;
    };
}