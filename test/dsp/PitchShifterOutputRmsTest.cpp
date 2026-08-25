#pragma once
// PitchShifterOutputRmsTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/PitchShifter.h"

class PitchShifterOutputRmsTest : public juce::UnitTest
{
public:
    PitchShifterOutputRmsTest() : juce::UnitTest ("PitchShifterOutputRms") {}

    void runTest() override
    {
        using namespace ovtdsp;

        // RMS on a sustained 200 Hz sinus MUST be at least 0.65
        // (theoretical target = 1/sqrt(2) = 0.7071 for a unity-amplitude
        // sinus passing through a unity-gain OLA chain). 0.65 = -0.7 dB,
        // so any regression of more than ~0.7 dB will fail the test.
        // The previous failure (Fix K) measured RMS = 0.519 = -2.68 dB
        // (with kGainCompensation = 1.45) and 0.244 = -9.3 dB (with the
        // stale "Tout / outLength" formula) - both well under 0.65.
        beginTest ("Sustained 200 Hz sine: RMS > 0.65 (Fix K regression, no under-gain)");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float f0 = 200.0f;
            const float ratio = 1.0f;
            const float formantRatio = 1.0f;

            const int blocks = 40;              // 40 * 512 / 44100 ~ 464 ms
            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            // Let the first 20 blocks (~232 ms) go by to move away from
            // the startup fade-in and the attack envelope, then measure
            // the RMS over the following 20.
            const int skipBlocks = 20;

            double sumSq = 0.0;
            int nRms = 0;
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const double ts = static_cast<double> (b) * 512.0 + i;
                    in.setSample (0, i,
                        std::sin (2.0 * juce::MathConstants<double>::pi
                                 * f0 * ts / sr));
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, f0);

                if (b >= skipBlocks)
                {
                    for (int i = 0; i < 512; ++i)
                    {
                        const float s = out.getSample (0, i);
                        expect (std::isfinite (s), "NaN in the PitchShifter output");
                        sumSq += static_cast<double> (s) * s;
                        ++nRms;
                    }
                }
            }

            const double rms = std::sqrt (sumSq / static_cast<double> (nRms));
            const double rmsExpected = 0.70710678; // 1/sqrt(2) for a unit sine
            const double gainDb = 20.0 * std::log10 (rms / rmsExpected);

            // Thresholds: RMS > 0.65 = -0.73 dB minimum, RMS < 0.80 (the
            // OLA peak can theoretically exceed 0.707 if kbdColaSum is
            // underestimated; we accept up to +1.1 dB to absorb COLA
            // fluctuations).
            expect (rms > 0.65,
                "Severe under-gain: RMS=" + juce::String (rms, 4)
                + " (target = 0.707, " + juce::String (gainDb, 2) + " dB). "
                + "Check the OLA gain formula (kGainCompensation / kbdColaSum) "
                + "and the kbdColaSum measurement in PitchShifter::prepare().");
            expect (rms < 0.80,
                "Over-gain: RMS=" + juce::String (rms, 4)
                + " (target = 0.707, " + juce::String (gainDb, 2) + " dB). "
                + "kGainCompensation too high.");
        }
    }
};

static PitchShifterOutputRmsTest pitchShifterOutputRmsTest;


