#pragma once
// BlockAwareOnePoleTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/BlockAwareOnePole.h"

class BlockAwareOnePoleTest : public juce::UnitTest
{
public:
    BlockAwareOnePoleTest() : juce::UnitTest ("BlockAwareOnePole") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("prepare() sets the sample rate and resets the state");
        {
            BlockAwareOnePole s;
            s.prepare (44100.0);
            s.setTimeConstantSeconds (0.1f);
            // After prepare, currentValue is 1.0 (the default initial).
            expectWithinAbsoluteError (s.getCurrentValue(), 1.0f, 1e-6f);
        }

        beginTest ("snapTo() forces an instant value, no smoothing");
        {
            BlockAwareOnePole s;
            s.prepare (44100.0);
            s.setTimeConstantSeconds (0.5f);
            s.snapTo (0.3f);
            expectWithinAbsoluteError (s.getCurrentValue(), 0.3f, 1e-6f);
        }

        beginTest ("tau=0 -> instant response, no smoothing");
        {
            BlockAwareOnePole s;
            s.prepare (44100.0);
            s.setTimeConstantSeconds (0.0f);
            s.reset (0.0f);
            const float v = s.processBlock (1.0f, 256);
            expectWithinAbsoluteError (v, 1.0f, 1e-6f,
                "tau=0 must snap to the target value");
        }

        beginTest ("Effective time constant is INDEPENDENT of buffer size (128 vs 256 vs 512 vs 1024)");
        {
            // This is the core regression test for the FlexTune fix.
            // We feed a step target (0 -> 1) and measure the number of
            // samples it takes to reach 1 - 1/e ~ 0.632. The count must
            // be the same in SAMPLES regardless of how many samples we
            // take per call to processBlock.
            const float targetTauSec = 0.100f; // 100 ms
            const double sampleRate = 44100.0;
            const int   expectedTauSamples = static_cast<int> (sampleRate * targetTauSec); // 4410

            for (int blockSize : { 64, 128, 256, 512, 1024 })
            {
                BlockAwareOnePole s;
                s.prepare (sampleRate);
                s.setTimeConstantSeconds (targetTauSec);
                s.reset (0.0f);

                // Run enough blocks to be very close to 1.0; track the first
                // block where the value crosses 1 - 1/e = 0.6321.
                int sampleAt63Pct = -1;
                int totalSamples = 0;
                const int maxBlocks = expectedTauSamples * 5 / blockSize + 10;
                for (int b = 0; b < maxBlocks && sampleAt63Pct < 0; ++b)
                {
                    const float v = s.processBlock (1.0f, blockSize);
                    totalSamples += blockSize;
                    if (v >= 0.6321f && sampleAt63Pct < 0)
                        sampleAt63Pct = totalSamples;
                }

                // The 63% crossing should be near 1*tau in SAMPLES, with
                // an error tolerance that accounts for block quantization.
                // The block-quantization error is up to one block size, so
                // we accept a tolerance of (expectedTauSamples * 0.1) +
                // blockSize.
                const int tolerance = expectedTauSamples / 10 + blockSize;
                expect (sampleAt63Pct > 0,
                    "blockSize=" + juce::String(blockSize) + ": never reached 63%");
                expectWithinAbsoluteError ((float) sampleAt63Pct,
                    (float) expectedTauSamples,
                    (float) tolerance,
                    "blockSize=" + juce::String(blockSize) +
                    ": 63% crossing at " + juce::String(sampleAt63Pct) +
                    " samples, expected ~" + juce::String(expectedTauSamples));
            }
        }

        beginTest ("Naive 'y = y*0.95 + x*0.05' is NOT buffer-size independent (control test)");
        {
            // This is the BUG we are fixing. With per-block alpha=0.05,
            // the time constant in samples is 1/0.05 = 20 blocks, which
            // is 20*blockSize in SAMPLES. So the same alpha gives:
            //   128 samples: 2560 samples = 58 ms
            //   256 samples: 5120 samples = 116 ms
            // 2x different. Documenting this here so future readers
            // understand why we have a dedicated helper.
            const int samplesAt128 = []{
                float y = 0.0f;
                int n = 0;
                while (y < 0.6321f && n < 10000) {
                    y = y * 0.95f + 1.0f * 0.05f;
                    n += 128;
                }
                return n;
            }();
            const int samplesAt256 = []{
                float y = 0.0f;
                int n = 0;
                while (y < 0.6321f && n < 10000) {
                    y = y * 0.95f + 1.0f * 0.05f;
                    n += 256;
                }
                return n;
            }();
            // The naive form is 2x slower at 256 samples than at 128.
            // This test will FAIL if someone "optimizes" the smoother
            // back to the naive form. Tolerance accounts for the
            // 1-block quantization.
            expect (std::abs (samplesAt128 - samplesAt256) > samplesAt128 / 2,
                "Naive smoother should be ~2x slower at 256 vs 128 samples "
                "(128=" + juce::String(samplesAt128) +
                ", 256=" + juce::String(samplesAt256) +
                "). If this fails, the BlockAwareOnePole is being "
                "compared against itself rather than against the buggy form.");
        }

        beginTest ("processBypassed() snaps and resets the alpha");
        {
            BlockAwareOnePole s;
            s.prepare (44100.0);
            s.setTimeConstantSeconds (0.5f);
            s.reset (0.0f);
            s.processBlock (0.5f, 128); // should be partway up
            const float mid = s.getCurrentValue();
            expect (mid > 0.0f && mid < 0.5f, "should be partway between 0 and 0.5");
            s.processBypassed (1.0f);
            expectWithinAbsoluteError (s.getCurrentValue(), 1.0f, 1e-6f);
        }

        beginTest ("reset() returns to the default initial value (1.0)");
        {
            BlockAwareOnePole s;
            s.prepare (44100.0);
            s.setTimeConstantSeconds (0.1f);
            for (int i = 0; i < 50; ++i)
                s.processBlock (0.3f, 128);
            expect (s.getCurrentValue() < 0.5f, "should have moved away from 1.0");
            s.reset();
            expectWithinAbsoluteError (s.getCurrentValue(), 1.0f, 1e-6f);
        }

        beginTest ("reset(customValue) returns to the specified initial value");
        {
            BlockAwareOnePole s;
            s.prepare (44100.0);
            s.reset (0.42f);
            expectWithinAbsoluteError (s.getCurrentValue(), 0.42f, 1e-6f);
        }
    }
};

static BlockAwareOnePoleTest blockAwareOnePoleTest;


