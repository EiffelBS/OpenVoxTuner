#pragma once
// UpwardCompressorTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/UpwardCompressor.h"
#include <cmath>
#include <vector>

class UpwardCompressorTest : public juce::UnitTest
{
public:
    UpwardCompressorTest() : juce::UnitTest ("UpwardCompressor") {}

    void runTest() override
    {
        using namespace ovtdsp;

        const double sampleRate = 44100.0;
        const int blockSize = 512;

        // === Test 1: Compressor boosts quiet signal when enabled ===
        {
            beginTest ("Boosts quiet signal when enabled");

            UpwardCompressor comp;
            comp.prepare (sampleRate);
            comp.setEnabled (true);
            comp.setAmount (0.5f);

            // Feed silence to let the pivot settle, then a quiet signal.
            juce::AudioBuffer<float> buf (1, blockSize);

            // 10 blocks of silence to warm up detectors
            for (int b = 0; b < 10; ++b)
            {
                buf.clear();
                comp.process (buf);
            }

            // Now feed a quiet signal (-30 dBFS â‰ˆ 0.0316)
            const float quietLevel = 0.0316f;
            for (int s = 0; s < blockSize; ++s)
                buf.setSample (0, s, quietLevel);

            // Process several blocks so the compressor settles
            for (int b = 0; b < 20; ++b)
                comp.process (buf);

            // Measure output RMS
            float sumSq = 0.0f;
            for (int s = 0; s < blockSize; ++s)
            {
                float v = buf.getSample (0, s);
                sumSq += v * v;
            }
            float outputRms = std::sqrt (sumSq / (float) blockSize);

            // Output should be louder than input (quietLevel) because the
            // compressor boosts quiet passages.
            expect (outputRms > quietLevel,
                    "Output RMS (" + juce::String (outputRms, 4)
                    + ") should be greater than input (" + juce::String (quietLevel, 4) + ")");

            // Also check it's not crazy loud (max 8x gain)
            expect (outputRms < quietLevel * 10.0f,
                    "Output RMS should not exceed 10x input");
        }

        // === Test 2: Compressor bypass (amount=0) passes through unity ===
        {
            beginTest ("Bypass passes through unity");

            UpwardCompressor comp;
            comp.prepare (sampleRate);
            comp.setEnabled (true);
            comp.setAmount (0.0f);  // No compression

            juce::AudioBuffer<float> buf (1, blockSize);

            // Feed a constant signal
            const float level = 0.1f;
            for (int s = 0; s < blockSize; ++s)
                buf.setSample (0, s, level);

            // Process enough blocks for bypassGain to settle
            for (int b = 0; b < 20; ++b)
                comp.process (buf);

            float sumSq = 0.0f;
            for (int s = 0; s < blockSize; ++s)
            {
                float v = buf.getSample (0, s);
                sumSq += v * v;
            }
            float outputRms = std::sqrt (sumSq / (float) blockSize);

            // With amount=0, the compressor should pass through at unity
            expectWithinAbsoluteError (outputRms, level, 0.001f,
                    "Bypass output should match input at unity gain");
        }

        // === Test 3: Disabled compressor passes through unity ===
        {
            beginTest ("Disabled compressor passes through");

            UpwardCompressor comp;
            comp.prepare (sampleRate);
            comp.setEnabled (false);
            comp.setAmount (0.5f);

            juce::AudioBuffer<float> buf (1, blockSize);
            const float level = 0.2f;
            for (int s = 0; s < blockSize; ++s)
                buf.setSample (0, s, level);

            for (int b = 0; b < 20; ++b)
                comp.process (buf);

            float sumSq = 0.0f;
            for (int s = 0; s < blockSize; ++s)
            {
                float v = buf.getSample (0, s);
                sumSq += v * v;
            }
            float outputRms = std::sqrt (sumSq / (float) blockSize);

            expectWithinAbsoluteError (outputRms, level, 0.001f,
                    "Disabled compressor should pass through at unity gain");
        }

        // === Test 4: Gain increases with higher amount ===
        {
            beginTest ("Higher amount = more gain on quiet signal");

            auto measureGain = [&] (float amount) -> float
            {
                UpwardCompressor comp;
                comp.prepare (sampleRate);
                comp.setEnabled (true);
                comp.setAmount (amount);

                juce::AudioBuffer<float> buf (1, blockSize);

                // Warm up
                for (int b = 0; b < 10; ++b)
                {
                    buf.clear();
                    comp.process (buf);
                }

                // Quiet signal
                const float quietLevel = 0.02f;
                for (int s = 0; s < blockSize; ++s)
                    buf.setSample (0, s, quietLevel);

                for (int b = 0; b < 30; ++b)
                    comp.process (buf);

                float sumSq = 0.0f;
                for (int s = 0; s < blockSize; ++s)
                {
                    float v = buf.getSample (0, s);
                    sumSq += v * v;
                }
                return std::sqrt (sumSq / (float) blockSize) / quietLevel;
            };

            float gainLow  = measureGain (0.1f);
            float gainHigh = measureGain (0.9f);

            expect (gainHigh > gainLow,
                    "Higher amount (" + juce::String (gainHigh, 2)
                    + "x) should produce more gain than lower amount ("
                    + juce::String (gainLow, 2) + "x)");
        }
    }
};

static UpwardCompressorTest upwardCompressorTest;


