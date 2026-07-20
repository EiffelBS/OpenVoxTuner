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
        // 25 ms master enable ramp -- long enough to be click-free, short
        // enough that toggling feels instant.
        const double tau = 0.025;
        masterCoeff = 1.0f - std::exp (-1.0 / (tau * sampleRate));
        masterGain = 0.0f;
        masterTarget = 0.0f;
        prepared = true;
    }

    void ReverbEffect::reset()
    {
        reverb.reset();
        masterGain = masterTarget;
    }

    void ReverbEffect::process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix)
    {
        if (!prepared || buffer.getNumSamples() == 0)
            return;

        const float mix = juce::jlimit (0.0f, 1.0f, wetMix);

        // If neither the master enable nor the user mix contributes, and the
        // master gain has already fully decayed, skip processing entirely to
        // save CPU (the reverb tail is silent).
        if (!enabled && mix <= 0.0f && masterGain < 1.0e-4f)
            return;

        // The master enable now ramps smoothly instead of hard-cutting. This
        // keeps the reverb tail alive while it fades out (no click when
        // disabling) and fades the wet in gently when enabling -- identical in
        // feel to moving the mix slider.
        masterTarget = enabled ? 1.0f : 0.0f;

        // Process the wet path whenever there is anything to contribute, so the
        // tail continues to decay naturally after disable.
        const bool needWet = (masterGain > 1.0e-4f) || enabled;
        if (!needWet || mix <= 0.0f)
        {
            // Still advance the master gain so a disable-then-enable sequence
            // stays smooth, but skip the (silent) reverb work.
            masterGain += (masterTarget - masterGain) * masterCoeff;
            return;
        }

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

            // Mix dry + wet, scaled by the smoothed master enable gain.
            for (int i = 0; i < numSamples; ++i)
            {
                masterGain += (masterTarget - masterGain) * masterCoeff;
                data[i] = data[i] * (1.0f - mix * masterGain) + wetData[i] * mix * masterGain;
            }
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

            // Mix dry + wet, scaled by the smoothed master enable gain.
            for (int i = 0; i < numSamples; ++i)
            {
                masterGain += (masterTarget - masterGain) * masterCoeff;
                dataL[i] = dataL[i] * (1.0f - mix * masterGain) + wetL[i] * mix * masterGain;
                dataR[i] = dataR[i] * (1.0f - mix * masterGain) + wetR[i] * mix * masterGain;
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