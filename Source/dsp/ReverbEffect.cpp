// ReverbEffect.cpp
// Implementation of a simple reverb effect using JUCE's built-in reverb.

#include "ReverbEffect.h"

namespace ovtdsp
{
    ReverbEffect::ReverbEffect()
    {
        // Tuned for vocal monitoring: medium-large room, longer tail, gentle damping
        params.roomSize = 0.7f;
        params.damping = 0.3f;
        params.wetLevel = 0.5f;      // Internal wet gain reduced to avoid volume boost
        params.dryLevel = 0.0f;
        params.width = 1.0f;
        params.freezeMode = 0.0f;
    }

    void ReverbEffect::prepare (double sr, int /*maximumBlockSize*/)
    {
        sampleRate = sr;
        reverb.reset();
        reverb.setSampleRate (sampleRate);
        reverb.setParameters (params);
        prepared = true;
    }

    void ReverbEffect::reset()
    {
        reverb.reset();
    }

    void ReverbEffect::process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix)
    {
        if (!prepared || buffer.getNumSamples() == 0)
            return;

        if (!enabled || wetMix <= 0.0f)
            return;

        const float mix = juce::jlimit (0.0f, 1.0f, wetMix);

        if (buffer.getNumChannels() == 1)
        {
            // Mono: process through the reverb, mix dry + wet
            auto* data = buffer.getWritePointer (0);
            const int numSamples = buffer.getNumSamples();

            // Create a temporary wet buffer
            juce::AudioBuffer<float> wetBuffer (1, numSamples);
            wetBuffer.clear();
            wetBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
            auto* wetData = wetBuffer.getWritePointer (0);
            reverb.processMono (wetData, numSamples);

            // Mix dry + wet
            for (int i = 0; i < numSamples; ++i)
                data[i] = data[i] * (1.0f - mix) + wetData[i] * mix;
        }
        else
        {
            // Stereo: process each channel through the reverb, mix
            auto* dataL = buffer.getWritePointer (0);
            auto* dataR = buffer.getWritePointer (1);
            const int numSamples = buffer.getNumSamples();

            // Create temporary wet buffer
            juce::AudioBuffer<float> wetBuffer (2, numSamples);
            wetBuffer.clear();
            wetBuffer.copyFrom (0, 0, buffer, 0, 0, numSamples);
            wetBuffer.copyFrom (1, 0, buffer, 1, 0, numSamples);
            auto* wetL = wetBuffer.getWritePointer (0);
            auto* wetR = wetBuffer.getWritePointer (1);
            reverb.processStereo (wetL, wetR, numSamples);

            // Mix dry + wet
            for (int i = 0; i < numSamples; ++i)
            {
                dataL[i] = dataL[i] * (1.0f - mix) + wetL[i] * mix;
                dataR[i] = dataR[i] * (1.0f - mix) + wetR[i] * mix;
            }
        }
    }

    void ReverbEffect::setRoomSize (float size)
    {
        params.roomSize = juce::jlimit (0.0f, 1.0f, size);
        if (prepared)
            reverb.setParameters (params);
    }

    void ReverbEffect::setDamping (float damping)
    {
        params.damping = juce::jlimit (0.0f, 1.0f, damping);
        if (prepared)
            reverb.setParameters (params);
    }
}