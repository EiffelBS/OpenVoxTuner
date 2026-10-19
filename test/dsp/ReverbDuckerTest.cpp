#pragma once
// ReverbDuckerTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_basics/juce_audio_basics.h>
#include "../../Source/dsp/ReverbDucker.h"
#include <cmath>
#include <iostream>

namespace
{
    constexpr double rvdkSr = 48000.0;
    constexpr int rvdkBlock = 256;

    /** A key block whose peak is exactly `amplitude` on every sample: the envelope settles
        to that peak, which is what makes the gain assertions closed-form. */
    juce::AudioBuffer<float> rvdkKey (float amplitude, int channels = 2, int samples = rvdkBlock)
    {
        juce::AudioBuffer<float> buf (channels, samples);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < samples; ++i)
            {
                const float sign = (i % 2 == 0) ? 1.0f : -1.0f;
                buf.setSample (c, i, sign * amplitude);
            }
        return buf;
    }

    /** Feed `seconds` of a constant-amplitude key, then return: the envelope settles on the
        amplitude (attack time 15 ms), so every assertion below is about the settled state. */
    void rvdkDrive (ovtdsp::ReverbDucker& ducker, float amplitude, double seconds)
    {
        const auto key = rvdkKey (amplitude);
        const int blocks = static_cast<int> (std::ceil (seconds * rvdkSr / rvdkBlock));
        for (int k = 0; k < blocks; ++k)
            ducker.processKey (key);
    }

    float rvdkGainAtFullReference (float depth)
    {
        return std::pow (10.0f, -depth * ovtdsp::ReverbDucker::kMaxDepthDb / 20.0f);
    }
}

class ReverbDuckerTest : public juce::UnitTest
{
public:
    ReverbDuckerTest() : juce::UnitTest ("Reverb Ducker") {}

    void runTest() override
    {
        // === The three corners of the gain expression ===

        beginTest ("A fresh ducker leaves the rooms alone");
        {
            // The key is silent, so there is nothing to duck: a plugin that played nothing
            // since load must not attenuate its own reverb.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);

            expectEquals (ducker.getDuckGain (1.0f), 1.0f);
            expectEquals (ducker.getEnvelope(), 0.0f);
        }

        beginTest ("Depth 0 never ducks, whatever the key does");
        {
            // 0 is the documented off switch: the amount IS the switch, exactly like a send,
            // so a depth of 0 has to be a true bypass and not a tiny attenuation.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 1.0f, 0.2);

            expectEquals (ducker.getDuckGain (0.0f), 1.0f);
            expectLessThan (ducker.getDuckGain (1.0f), 0.9f);   // the same key does duck when armed
        }

        beginTest ("reset() returns the ducker to unity gain");
        {
            // A transport restart must not leave the rooms ducked from the previous take.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 0.8f, 0.2);
            expectLessThan (ducker.getDuckGain (1.0f), 0.5f);

            ducker.reset();
            expectEquals (ducker.getEnvelope(), 0.0f);
            expectEquals (ducker.getDuckGain (1.0f), 1.0f);
        }

        // === Calibration: the depth knob means decibels ===

        beginTest ("A loud lead at full depth ducks by the 12 dB ceiling");
        {
            // The reference peak is -14 dBFS (0.2): a lead louder than that reaches the full
            // range, which is what makes the knob's top predictable instead of source-dependent.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 0.8f, 0.2);

            const float gain = ducker.getDuckGain (1.0f);
            std::cout << "full-depth duck gain: " << gain
                      << " (" << juce::Decibels::gainToDecibels (gain) << " dB)\n";

            expectWithinAbsoluteError (gain, rvdkGainAtFullReference (1.0f), 1.0e-3f);
            expectWithinAbsoluteError (gain, 0.2512f, 1.0e-3f);
        }

        beginTest ("Middle depth ducks by half the ceiling in dB");
        {
            // Depth is a dB amount, not a linear scale: 50 % of the range must read -6 dB,
            // so the knob's position matches what the ear hears.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 0.8f, 0.2);

            const float gain = ducker.getDuckGain (0.5f);
            std::cout << "half-depth duck gain: " << gain
                      << " (" << juce::Decibels::gainToDecibels (gain) << " dB)\n";

            expectWithinAbsoluteError (gain, rvdkGainAtFullReference (0.5f), 1.0e-3f);
            expectWithinAbsoluteError (gain, 0.5012f, 1.0e-3f);
        }

        beginTest ("Quiet singing ducks proportionally less");
        {
            // Below the reference peak the amount scales with the level, so a whispered take
            // cannot pull a hole in the room the way a full-voice chorus would.
            ovtdsp::ReverbDucker halfLoud;
            halfLoud.prepare (rvdkSr);
            rvdkDrive (halfLoud, 0.1f, 0.2);   // half the reference peak

            const float gain = halfLoud.getDuckGain (1.0f);
            std::cout << "half-level key duck gain: " << gain
                      << " (" << juce::Decibels::gainToDecibels (gain) << " dB)\n";

            expectWithinAbsoluteError (gain, rvdkGainAtFullReference (0.5f), 1.0e-2f);
        }

        // === Envelope shape ===

        beginTest ("The envelope attacks in milliseconds and releases in tenths of a second");
        {
            // The asymmetry IS the effect: it must catch the first consonant of a phrase
            // (fast, ~15 ms) and then bloom back slowly (~300 ms), never chatter on vibrato.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);

            ducker.processKey (rvdkKey (0.8f));          // one block = 5.3 ms
            const float afterAttack = ducker.getEnvelope();

            ducker.processSilence (rvdkBlock);              // one block of silence = same 5.3 ms
            const float releaseMove = afterAttack - ducker.getEnvelope();

            std::cout << "envelope after 5.3 ms key: " << afterAttack
                      << ", released in the same time: " << releaseMove << "\n";

            expectGreaterThan (afterAttack, 0.0f);
            // Attack coefficient is ~1 - exp(-5.3/15) = 0.30 of the gap; release is
            // ~1 - exp(-5.3/300) = 0.018 of it, so the attack moves an order of magnitude more.
            expectGreaterThan (afterAttack, 0.8f * 0.2f);
            expectLessThan (releaseMove, afterAttack * 0.25f);
        }

        beginTest ("Silence blooms the rooms back in about a release time constant");
        {
            // One release constant (300 ms) has to take the envelope down to e^-1 of where the
            // phrase left it, so the tail is allowed back between phrases -- the sound the knob
            // promises. The key sits below the -14 dBFS reference (0.15 peak) so the gain is in
            // its partial range and the recovery is visible as a gain change, not as a clamp.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 0.15f, 0.5);

            const float settledEnvelope = ducker.getEnvelope();
            const float duckedGain = ducker.getDuckGain (1.0f);
            expectLessThan (duckedGain, 0.9f);   // a quiet lead still ducks a little

            ducker.processSilence (static_cast<int> (0.3 * rvdkSr));   // one release constant
            const float releasedEnvelope = ducker.getEnvelope();

            // Envelope: e^-1 of the settled value.
            std::cout << "envelope settled: " << settledEnvelope
                      << ", after one release constant: " << releasedEnvelope << "\n";
            expectWithinAbsoluteError (releasedEnvelope, settledEnvelope * 0.3679f, settledEnvelope * 0.05f);

            // Gain: follows the envelope through the same dB law, so it recovers only as far as
            // the envelope did (the dB depth, not the linear gain, is what the release scales).
            const float keyLevel = juce::jlimit (0.0f, 1.0f,
                releasedEnvelope / ovtdsp::ReverbDucker::kKeyReferencePeak);
            const float expectedGain = std::pow (10.0f, -ovtdsp::ReverbDucker::kMaxDepthDb * keyLevel / 20.0f);
            const float recoveredGain = ducker.getDuckGain (1.0f);
            std::cout << "gain after one release constant: " << recoveredGain
                      << " (expected " << expectedGain << ")\n";

            expectGreaterThan (recoveredGain, duckedGain);
            expectWithinAbsoluteError (recoveredGain, expectedGain, 1.0e-3f);

            // Long silence: back to unity.
            ducker.processSilence (static_cast<int> (2.0 * rvdkSr));
            expectGreaterThan (ducker.getDuckGain (1.0f), 0.995f);
        }

        beginTest ("The duck never goes below the designed floor");
        {
            // A runaway envelope must not mute the room: -12 dB is the promise of the knob.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 1.0f, 1.0);   // a full-scale key for a second

            expectGreaterOrEqual (ducker.getDuckGain (1.0f), 0.25f);
            expectLessOrEqual (ducker.getDuckGain (1.0f), 0.252f);
        }

        // === Stereo linkage and mono degradation ===

        beginTest ("The key is stereo-linked: the loudest channel drives both returns");
        {
            // A ducker that answered per channel would shift the stereo image as the singer
            // moves off-axis. The envelope is a peak over channels, so left and right always
            // receive the same gain.
            ovtdsp::ReverbDucker leftKeyed;
            ovtdsp::ReverbDucker rightKeyed;
            leftKeyed.prepare (rvdkSr);
            rightKeyed.prepare (rvdkSr);

            juce::AudioBuffer<float> leftOnly (2, rvdkBlock);
            juce::AudioBuffer<float> rightOnly (2, rvdkBlock);
            leftOnly.clear();
            rightOnly.clear();
            for (int i = 0; i < rvdkBlock; ++i)
            {
                leftOnly.setSample (0, i, (i % 2 == 0) ? 0.8f : -0.8f);
                rightOnly.setSample (1, i, (i % 2 == 0) ? 0.8f : -0.8f);
            }

            for (int k = 0; k < 100; ++k)
            {
                leftKeyed.processKey (leftOnly);
                rightKeyed.processKey (rightOnly);
            }

            expectWithinAbsoluteError (leftKeyed.getEnvelope(), rightKeyed.getEnvelope(), 1.0e-4f);
            expectWithinAbsoluteError (leftKeyed.getDuckGain (1.0f), rightKeyed.getDuckGain (1.0f), 1.0e-6f);
        }

        beginTest ("A mono key drives the same envelope as a stereo copy");
        {
            // A mono source is a normal case (mono vocal track, mono sidechain): it must duck
            // exactly as much as the same signal in stereo.
            ovtdsp::ReverbDucker mono;
            ovtdsp::ReverbDucker stereo;
            mono.prepare (rvdkSr);
            stereo.prepare (rvdkSr);

            const auto monoKey = rvdkKey (0.5f, 1);
            const auto stereoKey = rvdkKey (0.5f, 2);
            for (int k = 0; k < 100; ++k)
            {
                mono.processKey (monoKey);
                stereo.processKey (stereoKey);
            }

            expectWithinAbsoluteError (mono.getEnvelope(), stereo.getEnvelope(), 1.0e-5f);
        }

        beginTest ("An empty key block is a no-op");
        {
            // A host may deliver a zero-length block; the ducker must leave its envelope alone
            // rather than dividing by the block size or resetting.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 0.5f, 0.1);
            const float before = ducker.getEnvelope();

            juce::AudioBuffer<float> empty (2, 0);
            ducker.processKey (empty);

            expectEquals (ducker.getEnvelope(), before);
        }

        // === Lifecycle ===

        beginTest ("prepare() re-arms the envelope at any sample rate");
        {
            // The coefficients follow the sample rate, and a re-prepare (new device, new block
            // size) must start from a clean ducker, not from the previous session's tail.
            ovtdsp::ReverbDucker ducker;
            ducker.prepare (rvdkSr);
            rvdkDrive (ducker, 0.8f, 0.2);
            expectLessThan (ducker.getDuckGain (1.0f), 0.5f);

            for (double rate : { 44100.0, 96000.0 })
            {
                ducker.prepare (rate);
                expectEquals (ducker.getEnvelope(), 0.0f);
                expectEquals (ducker.getDuckGain (1.0f), 1.0f);

                // The attack still reaches most of the gap in a block at the new rate.
                ducker.processKey (rvdkKey (0.8f));
                expectGreaterThan (ducker.getEnvelope(), 0.0f);
            }
        }
    }
};

static ReverbDuckerTest reverbDuckerTest;
