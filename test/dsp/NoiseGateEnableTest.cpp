#pragma once
// NoiseGateEnableTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <iostream>

#include "../../Source/dsp/NoiseGate.h"

namespace
{
    // Feeds the gate a long stretch of silence so it closes fully, then checks what the gate
    // reports afterwards. Names are prefixed ngx: every test file is #included into Main.cpp and
    // shares one anonymous namespace.
    void ngxFillSilence (juce::AudioBuffer<float>& buf)
    {
        for (int c = 0; c < buf.getNumChannels(); ++c)
            buf.clear (c, 0, buf.getNumSamples());
    }

    // The plugin's ducking formula, shared by the lead wet mix and the send return.
    float ngxDuckGain (float depth, float gateGain)
    {
        return 1.0f - depth * (1.0f - gateGain);
    }
}

class NoiseGateEnableTest : public juce::UnitTest
{
public:
    NoiseGateEnableTest() : juce::UnitTest ("NoiseGateEnable") {}

    void runTest() override
    {
        beginTest ("A disabled gate leaves the signal bit-identical");
        {
            ovtdsp::NoiseGate gate;
            gate.prepare (48000.0);
            gate.setThresholdDb (-50.0f);   // plugin default
            gate.setEnabled (false);

            juce::AudioBuffer<float> buf (2, 256);
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, 0.3f * std::sin (0.1f * i));

            juce::AudioBuffer<float> copy = buf;
            gate.process (buf);

            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                    expectEquals (buf.getSample (c, i), copy.getSample (c, i),
                                  "a disabled gate must not touch the audio");
        }

        beginTest ("A closed gate reports a low gain and keeps it after disabling");
        {
            // The gate only updates its reported gain inside process(), which returns early when
            // the gate is off. So the value survives a disable: a caller that reads it without
            // checking the enable flag keeps ducking on a gate the user switched off, and at full
            // depth that is silence -- the reverb knob at full mix sounding like nothing.
            ovtdsp::NoiseGate gate;
            gate.prepare (48000.0);
            gate.setThresholdDb (-50.0f);
            gate.setEnabled (true);

            juce::AudioBuffer<float> silent (2, 256);
            ngxFillSilence (silent);
            for (int k = 0; k < 200; ++k)   // ~1 s of silence, release tau is 50 ms
                gate.process (silent);

            const float closedGain = gate.getCurrentGain();
            std::cout << "closed gate gain: " << closedGain << "\n";
            expectLessThan (closedGain, 0.01f);   // fully closed

            gate.setEnabled (false);
            expectEquals (gate.getCurrentGain(), closedGain,
                          "disabling the gate must not rewrite the stale gain");

            const float duckedWithStale = ngxDuckGain (1.0f, gate.getCurrentGain());
            expectLessThan (duckedWithStale, 0.02f);   // full depth on a stale closed gate: silence

            // The guard the plugin uses: read the gain only when the gate is enabled.
            const float guarded = gate.isEnabled() ? gate.getCurrentGain() : 1.0f;
            expectEquals (ngxDuckGain (1.0f, guarded), 1.0f,
                          "a disabled gate must leave the reverb unducked");
        }
    }
};

static NoiseGateEnableTest noiseGateEnableTest;
