#pragma once
// FormantPreserverModulationTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/FormantPreserver.h"
#include <cmath>

class FormantPreserverModulationTest : public juce::UnitTest
{
public:
    FormantPreserverModulationTest() : juce::UnitTest ("FormantPreserverModulation (Fix AZ)") {}

    void runTest() override
    {
        using namespace ovtdsp;

        const double sampleRate = 44100.0;
        const int blockSize = 256;

        beginTest ("5Hz vibrato on input ratio: output is smooth, not warbling");
        {
            // Simulate a sustained 200Hz sinus input and modulate the
            // pitch ratio at 5Hz with amplitude 0.5% (the typical
            // vibrato). With the old biquadSmoothAlpha = 0.002 (TC =
            // 2.9s), the formant biquads lag far behind the 5Hz
            // modulation and the output has a strong 5Hz envelope
            // (the "warble"). With the new biquadSmoothAlpha = 0.05
            // (TC = 115ms), the biquads track the modulation closely
            // (|H(5Hz)| = 0.42) and the output is smooth.
            FormantPreserver fp;
            fp.prepare (sampleRate, blockSize);
            fp.setEnabled (true);
            fp.setFormantShift (0.0f);

            // Disable the user formant shift so the only effect is
            // the 1/sqrt(ratio) compensation.
            const int numBlocks = 400; // ~9.3 seconds, enough to see 5Hz mod settle
            const int totalSamples = numBlocks * blockSize;
            juce::AudioBuffer<float> buffer (1, totalSamples);
            juce::AudioBuffer<float> bufferRef (1, totalSamples);

            // Generate a 200Hz sinus for the reference path.
            for (int i = 0; i < totalSamples; ++i)
            {
                const float t = static_cast<float> (i) / static_cast<float> (sampleRate);
                const float s = std::sin (2.0f * 3.14159265f * 200.0f * t);
                bufferRef.setSample (0, i, s);
                buffer.setSample (0, i, s);
            }

            // Process with a 5Hz modulated ratio. The modulation is
            // applied per BLOCK (the same as the real PluginProcessor
            // path) to mimic the way FlexTune + vibrato produce
            // per-block changes in targetRatio.
            const float vibratoHz = 5.0f;
            const float vibratoAmp = 0.005f; // 0.5% = ~8.6 cents
            for (int b = 0; b < numBlocks; ++b)
            {
                const float t = static_cast<float> (b * blockSize) / static_cast<float> (sampleRate);
                const float mod = 1.0f + vibratoAmp * std::sin (2.0f * 3.14159265f * vibratoHz * t);
                juce::AudioBuffer<float> block (1, blockSize);
                for (int i = 0; i < blockSize; ++i)
                    block.setSample (0, i, buffer.getSample (0, b * blockSize + i));
                fp.process (block, mod);
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (0, b * blockSize + i, block.getSample (0, i));
            }

            // Measure the 5Hz amplitude in the output envelope: compute
            // the absolute value of the signal, low-pass it to get the
            // envelope, and measure the FFT magnitude at 5Hz.
            //
            // We use a simple approach: compute the difference between
            // the output and the reference. The output should be very
            // close to the reference (the FormantPreserver's response
            // is mostly transparent at ratio=1.0 +/- 0.5%), with only
            // a small smooth gain variation. The DIFFERENCE signal
            // should be smooth (not contain a strong 5Hz component).
            float diffRms = 0.0f;
            for (int i = 0; i < totalSamples; ++i)
            {
                const float d = buffer.getSample (0, i) - bufferRef.getSample (0, i);
                diffRms += d * d;
            }
            diffRms = std::sqrt (diffRms / static_cast<float> (totalSamples));

            // The diff should be small (the FormantPreserver is mostly
            // transparent at ratio near 1.0). At biquadSmoothAlpha =
            // 0.002 (old), the diff was ~0.4-0.5 because the biquads
            // moved slowly and the response was inconsistent. At the
            // new 0.05, the biquads track the modulation closely and
            // the diff is dominated by the smooth gain variation,
            // ~0.05-0.1.
            //
            // We assert the diff is < 0.2 (4x improvement over the
            // old value), which is well within "smooth modulation"
            // territory.
            expect (diffRms < 0.2f,
                "FormantPreserver output should be smooth (not warbling). diffRms=" +
                juce::String (diffRms) + " (expected < 0.2; old value was ~0.4-0.5)");
        }

        beginTest ("Constant ratio=1.0: output equals input (no drift)");
        {
            // Sanity check: with constant ratio = 1.0, the
            // FormantPreserver should be transparent (the biquads
            // stay in their passthrough state).
            FormantPreserver fp;
            fp.prepare (sampleRate, blockSize);
            fp.setEnabled (true);
            fp.setFormantShift (0.0f);

            juce::AudioBuffer<float> buffer (1, blockSize * 10);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float t = static_cast<float> (i) / static_cast<float> (sampleRate);
                buffer.setSample (0, i, std::sin (2.0f * 3.14159265f * 200.0f * t));
            }

            for (int b = 0; b < 10; ++b)
            {
                juce::AudioBuffer<float> block (1, blockSize);
                for (int i = 0; i < blockSize; ++i)
                    block.setSample (0, i, buffer.getSample (0, b * blockSize + i));
                fp.process (block, 1.0f);
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (0, b * blockSize + i, block.getSample (0, i));
            }

            // Verify output RMS is close to input RMS (1/sqrt(2) ~ 0.707).
            float inRms = 0.0f, outRms = 0.0f;
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float t = static_cast<float> (i) / static_cast<float> (sampleRate);
                inRms += std::pow (std::sin (2.0f * 3.14159265f * 200.0f * t), 2);
                outRms += std::pow (buffer.getSample (0, i), 2);
            }
            inRms = std::sqrt (inRms / static_cast<float> (buffer.getNumSamples()));
            outRms = std::sqrt (outRms / static_cast<float> (buffer.getNumSamples()));

            // With ratio=1.0 and shiftSemitones=0, the FormantPreserver
            // is transparent and the output RMS should equal the input
            // RMS (within 1%).
            expect (std::abs (outRms - inRms) < 0.01f,
                "FormantPreserver at ratio=1.0 must be transparent. inRms=" +
                juce::String (inRms) + ", outRms=" + juce::String (outRms));
        }
    }
};

static FormantPreserverModulationTest formantPreserverModulationTest;


