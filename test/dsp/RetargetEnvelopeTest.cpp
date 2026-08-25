#pragma once
// RetargetEnvelopeTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/RetargetEnvelope.h"

class RetargetEnvelopeTest : public juce::UnitTest
{
public:
    RetargetEnvelopeTest() : juce::UnitTest ("RetargetEnvelope") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("Speed=0 -> instantaneous response");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (0.0f);
            // First call: the value must already equal the target.
            const float v = env.processSample (1.5f);
            expectWithinAbsoluteError (v, 1.5f, 1e-3f);
        }

        beginTest ("Speed=200 ms -> progressive convergence");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (200.0f);
            env.reset();
            // Step by step over 1 second (44100 samples) with target 2.0
            // from 1.0: we should be close to 2.0 at the end.
            float v = 1.0f;
            for (int i = 0; i < 44100; ++i)
                v = env.processSample (2.0f);
            // After 1 s = 5 * tau, we are at ~99% of the target.
            expect (v > 1.95f, "Insufficient convergence: v=" + juce::String (v));
        }

        beginTest ("Target = 1.0 -> stays at 1.0");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (50.0f);
            // Same target as the initial value -> no drift.
            float v = 1.0f;
            for (int i = 0; i < 1000; ++i)
                v = env.processSample (1.0f);
            expectWithinAbsoluteError (v, 1.0f, 1e-5f);
        }

        beginTest ("Reset restores 1.0");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (50.0f);
            for (int i = 0; i < 100; ++i) (void) env.processSample (1.5f);
            env.reset();
            // After reset, the next target must be applied from 1.0.
            const float v = env.processSample (1.5f);
            expect (v < 1.1f, "Reset not applied, v=" + juce::String (v));
        }
    }
};

static RetargetEnvelopeTest retargetEnvelopeTest;


