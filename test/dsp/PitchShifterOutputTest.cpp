#pragma once
// PitchShifterOutputTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/PitchShifter.h"

class PitchShifterOutputTest : public juce::UnitTest
{
public:
    PitchShifterOutputTest() : juce::UnitTest ("PitchShifterOutput") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("Sustained 200 Hz sine: output is not silent (regression B)");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float f0 = 200.0f;           // stable sung note
            const float ratio = 1.0f;          // no correction
            const float formantRatio = 1.0f;

            // Processes ~0.5 s of sine; checks the output RMS over the
            // second half (after the ~20 ms startup fade-in).
            const int blocks = 24;              // 24 * 512 ~ 0.28 s
            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            double sumSq = 0.0;
            int nRms = 0;
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const double t = static_cast<double> (b) * 512.0 + i;
                    in.setSample (0, i,
                        std::sin (2.0 * juce::MathConstants<double>::pi
                                 * f0 * t / sr));
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, f0);

                // RMS over the 2nd half of the block (after local fade-in).
                for (int i = 256; i < 512; ++i)
                {
                    const float s = out.getSample (0, i);
                    expect (std::isfinite (s), "NaN in the PitchShifter output");
                    sumSq += static_cast<double> (s) * s;
                    ++nRms;
                }
            }

            const double rms = std::sqrt (sumSq / static_cast<double> (nRms));
            // A proper grain gain yields an RMS around 0.5-0.7 for a unit
            // sine. We simply require it to be clearly non-zero
            // (bug B gave ~1e-4 or less).
            expect (rms > 0.05,
                    "Quasi-silent PitchShifter output: "
                    + juce::String (rms, 6)
                    + " -> suspected grain gain bug");
        }
    }
};

static PitchShifterOutputTest pitchShifterOutputTest;


