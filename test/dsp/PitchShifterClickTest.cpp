#pragma once
// PitchShifterClickTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/PitchShifter.h"

class PitchShifterClickTest : public juce::UnitTest
{
public:
    PitchShifterClickTest() : juce::UnitTest ("PitchShifterClick") {}

    void runTest() override
    {
        using namespace ovtdsp;

        // ===================================================================
        // Test 1: strong attack + pitch jump
        // ===================================================================
        beginTest ("Realistic scenario: 200Hz note -> jump to 300Hz, no audible click");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float ratio = 1.0f;          // no correction (isolates the shifter)
            const float formantRatio = 1.0f;

            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            int clickCount = 0;
            float prev = 0.0f;
            double t = 0.0;                    // current time (samples)
            juce::String clickLog;

            auto feedBlock = [&] (float f0, double freqHz)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float s = 0.0f;
                    if (f0 > 0.0f)
                        s = std::sin (2.0 * juce::MathConstants<double>::pi
                                      * freqHz * t / sr);
                    in.setSample (0, i, s);
                    ++t;
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, f0);

                for (int i = 0; i < 512; ++i)
                {
                    const float s = out.getSample (0, i);
                    expect (std::isfinite (s), "NaN in the output");
                    const float jump = std::abs (s - prev);
                    // An audible click = instantaneous jump > 0.1 in amplitude
                    // (on a unit sine signal).
                    if (jump > 0.15f)
                    {
                        ++clickCount;
                        clickLog += juce::String (static_cast<int> (t * 1000.0 / sr))
                                    + "ms(" + juce::String (jump, 3) + ") ";
                    }
                    prev = s;
                }
            };

            // 1) silence ~50 ms (f0 = 0)
            for (int b = 0; b < 5; ++b) feedBlock (0.0f, 0.0);
            // 2) 200 Hz note attack ~200 ms
            for (int b = 0; b < 20; ++b) feedBlock (200.0f, 200.0);
            // 3) jump to 300 Hz ~200 ms
            for (int b = 0; b < 20; ++b) feedBlock (300.0f, 300.0);

            // Still write the count to a file for traceability (relative to
            // the working directory the test runner is launched from).
            {
                auto f = juce::File::getCurrentWorkingDirectory()
                             .getChildFile ("test/dsp/click_count.txt");
                f.deleteFile();
                f.create();
                f.replaceWithText ("clickCount=" + juce::String (clickCount)
                                   + "\npositions=" + clickLog + "\n");
            }
            expectEquals (clickCount, 0,
                "Audible click(s) on note attack/jump: " + clickLog);
        }

        // ===================================================================
        // Test 2: STACCATO (same note repeated with a short gap)
        // Verifies that fast repetitions of the same note do not produce
        // a "pop" (OLA over-gain) at the start of each attack.
        // ===================================================================
        beginTest ("Staccato: 5 fast repetitions of same note, no pop at each attack");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float ratio = 1.0f;
            const float formantRatio = 1.0f;
            const float noteFreq = 200.0f;

            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            int clickCount = 0;
            float prev = 0.0f;
            double t = 0.0;
            juce::String clickLog;

            auto feedBlock = [&] (float f0, double freqHz)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float s = 0.0f;
                    if (f0 > 0.0f)
                        s = std::sin (2.0 * juce::MathConstants<double>::pi
                                      * freqHz * t / sr);
                    in.setSample (0, i, s);
                    ++t;
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, f0);

                for (int i = 0; i < 512; ++i)
                {
                    const float s = out.getSample (0, i);
                    expect (std::isfinite (s), "NaN in the output");
                    const float jump = std::abs (s - prev);
                    if (jump > 0.15f)
                    {
                        ++clickCount;
                        clickLog += juce::String (static_cast<int> (t * 1000.0 / sr))
                                    + "ms(" + juce::String (jump, 3) + ") ";
                    }
                    prev = s;
                }
            };

            // 1) initial silence ~50 ms
            for (int b = 0; b < 5; ++b) feedBlock (0.0f, 0.0);

            // 2) 5 staccato repetitions: note (~100 ms) + gap (~50 ms)
            for (int rep = 0; rep < 5; ++rep)
            {
                // 200 Hz note for ~100 ms (10 blocks)
                for (int b = 0; b < 10; ++b) feedBlock (noteFreq, noteFreq);
                // Silence for ~50 ms (5 blocks)
                for (int b = 0; b < 5; ++b) feedBlock (0.0f, 0.0);
            }

            expectEquals (clickCount, 0,
                "Audible pop(s) on staccato attacks: " + clickLog);
        }

        // ===================================================================
        // Test 3: amplitude peak at attack (OLA "trumpet" burst)
        // Verifies that the signal amplitude after the attack envelope does
        // not exceed 1.0x the input amplitude. Without the outPhase clamp,
        // 5 grains in burst cause a 2x OLA sum -> audible clipping even if
        // no jump > 0.1 is detected (the attack ramp smooths the transition
        // but the peak is over-amplified).
        // ===================================================================
        beginTest ("Raw attack: no OLA over-amplification (sum <= input)");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float ratio = 1.0f;
            const float formantRatio = 1.0f;
            const float noteFreq = 200.0f;

            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            double t = 0.0;
            // Initial silence ~50 ms
            for (int b = 0; b < 5; ++b)
            {
                for (int i = 0; i < 512; ++i) { in.setSample (0, i, 0.0f); ++t; }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, 0.0f);
            }
            // 200 Hz note for ~200 ms (40 blocks, ~464 ms)
            // We capture the max amplitude over the first 100 ms
            // (the attack + the start of the stable note).
            float maxAbs = 0.0f;
            double tAtMax = 0.0;
            const double inputAmp = 1.0; // sin amplitude
            for (int b = 0; b < 40; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = std::sin (2.0 * juce::MathConstants<double>::pi
                                              * noteFreq * t / sr);
                    in.setSample (0, i, s);
                    ++t;
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, noteFreq);
                for (int i = 0; i < 512; ++i)
                {
                    const float v = std::abs (out.getSample (0, i));
                    if (v > maxAbs) { maxAbs = v; tAtMax = (t - 512 + i) * 1000.0 / sr; }
                }
            }
            // Tolerates 10% over-amplification (1.1) to absorb small COLA
            // fluctuations. A 5-grain burst would give 2.0x, well above
            // the threshold.
            expect (maxAbs <= inputAmp * 1.10f,
                "OLA over-amplification: max |out|=" + juce::String (maxAbs, 3)
                + " (at t=" + juce::String (tAtMax, 2) + "ms), expected <= "
                + juce::String (inputAmp * 1.10f, 3)
                + " (10% above the input amplitude " + juce::String (inputAmp, 2) + ")");
        }
    }
};

static PitchShifterClickTest pitchShifterClickTest;


