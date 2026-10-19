// ReverbEffect.cpp
// ReverbEffect.cpp
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "ReverbEffect.h"

namespace ovtdsp
{
    ReverbEffect::ReverbEffect()
    {
        // Default tuning only: the processor sets each bus instance from
        // ReverbTuning.h (HALL = this legacy room, ROOM = the short room), so the
        // sound of the two buses is declared in ONE place.
        params.roomSize = 0.7f;
        params.damping = 0.3f;
        params.wetLevel = 0.5f;      // Internal wet gain reduced to avoid volume boost
        params.dryLevel = 0.0f;
        params.width = 1.0f;
        params.freezeMode = 0.0f;
    }

    void ReverbEffect::prepare (double sr, int maximumBlockSize)
    {
        sampleRate = sr;
        const int maxBlock = juce::jmax (maximumBlockSize, 1);

        reverb.reset();
        reverb.setSampleRate (sampleRate);
        reverb.setParameters (params);

        // Every buffer this class uses is sized HERE, once. process() and the send /
        // return path reuse them for the whole session, so a block boundary
        // allocates nothing on the audio thread.
        wetScratch.setSize (2, maxBlock, false, true, false);
        sendAccum.setSize (2, maxBlock, false, true, true);

        // 25 ms ramps: long enough to be click-free when a knob moves or a voice
        // stops sending, short enough that a change feels immediate.
        rampSamples = juce::jmax (1, static_cast<int> (0.025 * sampleRate));
        leadSendRamp.reset (rampSamples);
        harmonySendRamp.reset (rampSamples);
        duckRamp.reset (rampSamples);

        for (auto& filter : leadLowCut)
            filter.prepare (sampleRate, kLeadSendLowCutHz);

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
        sendAccum.clear();
        for (auto& filter : leadLowCut)
            filter.reset();

        // Flush the ramps to their neutral state: sends silent, return unducked.
        leadSendRamp.reset (rampSamples);
        harmonySendRamp.reset (rampSamples);
        duckRamp.reset (rampSamples);
        leadSendRamp.setCurrentAndTargetValue (0.0f);
        harmonySendRamp.setCurrentAndTargetValue (0.0f);
        duckRamp.setCurrentAndTargetValue (1.0f);

        tailPeak = 0.0f;
        masterGain = masterTarget;
    }

    void ReverbEffect::process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix)
    {
        if (!prepared || buffer.getNumSamples() == 0)
            return;

        const int numSamples = juce::jmin (buffer.getNumSamples(), wetScratch.getNumSamples());
        if (numSamples <= 0)
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

        const int numChannels = buffer.getNumChannels();

        // Copy the dry signal into the prepared scratch and run the room on it.
        wetScratch.clear();
        for (int ch = 0; ch < numChannels; ++ch)
            wetScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

        if (numChannels == 1)
            reverb.processMono (wetScratch.getWritePointer (0), numSamples);
        else
            reverb.processStereo (wetScratch.getWritePointer (0), wetScratch.getWritePointer (1), numSamples);

        // The insert is a dry/wet CROSSFADE: mix 0 leaves the input bit-identical and
        // mix 1 replaces it with the room. The plugin's buses are additive sends instead
        // (see processLeadSend and processHarmonySend), where stealing from the tapped
        // path would be wrong; this mode remains the IEffect contract and is covered by
        // the unit tests.
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            const float* wet = wetScratch.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                masterGain += (masterTarget - masterGain) * masterCoeff;
                const float w = mix * masterGain;
                data[i] = data[i] * (1.0f - w) + wet[i] * w;
            }
        }
    }

    void ReverbEffect::processSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount)
    {
        // Interface hook: the lead send slot is the one carrying the pre-send
        // low-cut, so a generic send lands there.
        processLeadSend (sendBuffer, sendAmount);
    }

    void ReverbEffect::processLeadSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount)
    {
        if (!prepared)
            return;

        const int numSamples = juce::jmin (sendBuffer.getNumSamples(), sendAccum.getNumSamples());
        if (numSamples <= 0 || sendBuffer.getNumChannels() <= 0)
            return;

        leadSendRamp.setTargetValue (juce::jlimit (0.0f, 1.0f, sendAmount));

        const int srcChannels = sendBuffer.getNumChannels();

        for (int ch = 0; ch < sendAccum.getNumChannels(); ++ch)
        {
            // A mono source feeding a stereo room: the same voice drives both sides,
            // so the send stays centred instead of vanishing from one channel.
            const int srcCh = juce::jmin (ch, srcChannels - 1);
            const float* src = sendBuffer.getReadPointer (srcCh);
            float* dst = sendAccum.getWritePointer (ch);
            OnePoleLowCut& lowCut = leadLowCut[ch];

            for (int i = 0; i < numSamples; ++i)
                dst[i] += lowCut.process (src[i]) * leadSendRamp.getNextValue();
        }
    }

    void ReverbEffect::processHarmonySend (juce::AudioBuffer<float>& sendBuffer, float sendAmount)
    {
        if (!prepared)
            return;

        const int numSamples = juce::jmin (sendBuffer.getNumSamples(), sendAccum.getNumSamples());
        if (numSamples <= 0 || sendBuffer.getNumChannels() <= 0)
            return;

        harmonySendRamp.setTargetValue (juce::jlimit (0.0f, 1.0f, sendAmount));

        const int srcChannels = sendBuffer.getNumChannels();

        for (int ch = 0; ch < sendAccum.getNumChannels(); ++ch)
        {
            const int srcCh = juce::jmin (ch, srcChannels - 1);
            const float* src = sendBuffer.getReadPointer (srcCh);
            float* dst = sendAccum.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
                dst[i] += src[i] * harmonySendRamp.getNextValue();
        }
    }

    void ReverbEffect::processReturn (juce::AudioBuffer<float>& masterBuffer, float duckGain)
    {
        if (!prepared || masterBuffer.getNumSamples() == 0)
            return;

        const int numSamples = juce::jmin (masterBuffer.getNumSamples(), sendAccum.getNumSamples());
        if (numSamples <= 0)
            return;

        float* sendL = sendAccum.getWritePointer (0);
        float* sendR = sendAccum.getWritePointer (1);

        // Nothing is being sent and the tail has already decayed: skip the room
        // entirely. This is what keeps the sleep path at ~1 % CPU -- a cleared
        // send buffer drives the tail down, and once it is inaudible the room is
        // simply not run. duckGain comes from the sidechain ducker (ReverbDucker),
        // not from the noise gate: the room dips while the lead sings and blooms
        // back between phrases.
        float sendPeak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            sendPeak = juce::jmax (sendPeak, juce::jmax (std::abs (sendL[i]), std::abs (sendR[i])));

        duckRamp.setTargetValue (juce::jlimit (0.0f, 1.0f, duckGain));

        if (sendPeak <= 0.0f && tailPeak < 1.0e-5f)
        {
            duckRamp.skip (numSamples);
            return;
        }

        // The send line becomes the wet: the room runs in place on the accumulated
        // sends, so no extra buffer is needed.
        reverb.processStereo (sendL, sendR, numSamples);

        tailPeak = 0.0f;

        if (masterBuffer.getNumChannels() == 1)
        {
            // Mono master: the stereo room is down-mixed to the centre, so the
            // return level matches what a stereo listener hears. The trim keeps the
            // tail behind the voice instead of doubling its level.
            float* out = masterBuffer.getWritePointer (0);

            for (int i = 0; i < numSamples; ++i)
            {
                const float g = duckRamp.getNextValue() * kReturnTrim;
                const float wet = (sendL[i] + sendR[i]) * 0.5f * g;
                out[i] += wet;
                tailPeak = juce::jmax (tailPeak, std::abs (wet));
            }
        }
        else
        {
            float* outL = masterBuffer.getWritePointer (0);
            float* outR = masterBuffer.getWritePointer (1);

            for (int i = 0; i < numSamples; ++i)
            {
                const float g = duckRamp.getNextValue() * kReturnTrim;
                const float wetL = sendL[i] * g;
                const float wetR = sendR[i] * g;
                outL[i] += wetL;
                outR[i] += wetR;
                tailPeak = juce::jmax (tailPeak, juce::jmax (std::abs (wetL), std::abs (wetR)));
            }
        }

        // The sends are consumed: the next block starts with an empty line, so a
        // send amount is applied exactly once and never accumulates across blocks.
        sendAccum.clear();
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

    void ReverbEffect::setStereoWidth (float width)
    {
        params.width = juce::jlimit (0.0f, 1.0f, width);
        if (prepared)
            reverb.setParameters (params);
    }
}

