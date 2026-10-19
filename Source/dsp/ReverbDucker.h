// ReverbDucker.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <algorithm>
#include <cmath>

namespace ovtdsp
{
    /**
     * The sidechain ducker of the shared reverb buses.
     *
     * This is NOT gate-linked ducking: the old implementation multiplied the
     * reverb wet by the noise gate's gain, so the room only ever moved when the
     * gate closed, and it stayed motionless while the singer was singing -- the
     * exact opposite of what a ducker does. Here the key is the corrected lead
     * voice itself, before the lead bus effects (so delay echoes cannot re-trigger
     * it), and the gain it produces is applied to the two rooms' returns:
     *
     *   - the room dips while the lead is singing, so long tails never mask the
     *     consonants;
     *   - it blooms back between phrases, which is the classic vocal-ducking
     *     sound a long hall needs to stay usable on a lead.
     *
     * Implementation: a per-sample peak envelope follower, stereo-linked (the
     * loudest channel drives both returns, so the image never shifts), with a
     * fast attack (~15 ms) and a slow release (~300 ms). The envelope is
     * normalised against a reference vocal peak of about -14 dBFS, so the depth
     * knob means the same thing at any input gain stage instead of only at 0 dBFS.
     *
     * The class allocates nothing: prepare() only computes two coefficients, and
     * processKey() walks the key buffer in place.
     */
    class ReverbDucker
    {
    public:
        void prepare (double sampleRateHz)
        {
            sampleRate = std::max (1.0, sampleRateHz);
            attackCoeff  = onePoleCoeff (kAttackMs);
            releaseCoeff = onePoleCoeff (kReleaseMs);
            envelope = 0.0f;
        }

        void reset() noexcept { envelope = 0.0f; }

        /** Update the envelope with a block of the key signal (the corrected lead). */
        void processKey (const juce::AudioBuffer<float>& key) noexcept
        {
            const int numSamples = key.getNumSamples();
            if (numSamples <= 0)
                return;

            const int numChannels = key.getNumChannels();
            if (numChannels <= 0)
            {
                processSilence (numSamples);
                return;
            }

            for (int i = 0; i < numSamples; ++i)
            {
                float peak = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    peak = std::max (peak, std::abs (key.getSample (ch, i)));

                envelope += (peak - envelope) * (peak > envelope ? attackCoeff : releaseCoeff);
            }
        }

        /** Update the envelope with silence: the release still runs in real time, so a
            transport stop or a sleep block lets the room bloom back gradually instead of
            freezing a ducked return. */
        void processSilence (int numSamples) noexcept
        {
            for (int i = 0; i < numSamples; ++i)
                envelope += (0.0f - envelope) * releaseCoeff;
        }

        /** Duck gain for the room returns, in [0, 1].
            depth 0 -> 1.0 (no ducking); depth 1 with the key at its reference peak -> 10^(-12/20). */
        float getDuckGain (float depth) const noexcept
        {
            const float amount = juce::jlimit (0.0f, 1.0f, depth);
            if (amount <= 0.0f || envelope <= 0.0f)
                return 1.0f;

            const float keyLevel = juce::jlimit (0.0f, 1.0f, envelope / kKeyReferencePeak);
            const float depthDb = amount * keyLevel * kMaxDepthDb;
            return std::pow (10.0f, -depthDb / 20.0f);
        }

        /** Current envelope, for tests and diagnostics. */
        float getEnvelope() const noexcept { return envelope; }

        static constexpr float kAttackMs  = 15.0f;
        static constexpr float kReleaseMs = 300.0f;
        static constexpr float kMaxDepthDb = 12.0f;
        /** Vocal peaks around -14 dBFS reach full depth; quieter singing ducks proportionally
            less, which keeps a whispered take from pulling a hole in the room. */
        static constexpr float kKeyReferencePeak = 0.2f;

    private:
        float onePoleCoeff (float timeMs) const noexcept
        {
            const float samples = static_cast<float> (timeMs * 0.001 * sampleRate);
            return 1.0f - std::exp (-1.0f / std::max (1.0f, samples));
        }

        double sampleRate = 48000.0;
        float attackCoeff = 0.5f;
        float releaseCoeff = 0.01f;
        float envelope = 0.0f;
    };
}
