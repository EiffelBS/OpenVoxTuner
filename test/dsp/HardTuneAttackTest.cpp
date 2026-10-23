#pragma once
// HardTuneAttackTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Hard Tune mode (`hard_tune`) switches off ONLY the ratio-jump arming of the
// PitchShifter's internal attack envelope (see `setRatioJumpEnvelopeEnabled`).
// That envelope has two arming paths:
//   - note onsets / > 2 semitone f0 jumps: masks the OLA grain reorganisation at
//     a note attack, which is also what keeps the attack click-free;
//   - caller ratio jumps > 3 % in one block: ducks the output before the OLA
//     re-arms the grains.
// Hard Tune bypasses the speed floor, so every semitone becomes a > 3 % step and
// the second path would duck on every snap - that pre-break swell is what the
// mode exists to remove. The first path must stay ON: switching the whole
// envelope off re-opens a measured onset click (max sample jump 0.39 vs 0.10).
//
// The envelope scales the OLA OUTPUT only, so it never feeds back into the grain
// state. A scenario shifter and a reference shifter fed the same signal produce
// sample-identical OLA output, and scenario/reference IS the envelope gain,
// sample by sample. The tests measure that ratio instead of comparing RMS levels,
// which keeps them exact and insensitive to OLA amplitude variation.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/PitchShifter.h"

class HardTuneAttackTest : public juce::UnitTest
{
public:
    HardTuneAttackTest() : juce::UnitTest ("HardTuneAttack") {}

    void runTest() override
    {
        using namespace ovtdsp;

        constexpr double sr        = 44100.0;
        constexpr int    blockSize = 512;                 // ~11.6 ms at 44.1 kHz
        constexpr float  noteHz    = 200.0f;
        constexpr float  ratioUp   = 1.0594630943592953f; // +1 semitone

        struct Rig
        {
            PitchShifter ps;
            juce::AudioBuffer<float> in  { 1, blockSize };
            juce::AudioBuffer<float> out { 1, blockSize };
            double t = 0.0;

            void feed (float ratio)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    in.setSample (0, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                          * (double) noteHz * t / sr));
                    ++t;
                }
                out.setSize (1, blockSize, false, true, false);
                ps.process (in, out, ratio, 1.0f, noteHz);
            }

            float sample (int i) const { return out.getSample (0, i); }
        };

        // `scenario` is the shifter under test; `reference` never arms the
        // ratio-jump duck, so scenario/reference is the scenario's envelope gain.
        struct Pair
        {
            Rig scenario, reference;

            void prepare()
            {
                scenario.ps.prepare (sr, blockSize);
                reference.ps.prepare (sr, blockSize);
                scenario.ps.setAttackTimeMs (30.0f);
                reference.ps.setAttackTimeMs (30.0f);
                reference.ps.setRatioJumpEnvelopeEnabled (false);
            }

            void feed (float ratio) { scenario.feed (ratio); reference.feed (ratio); }

            /** Envelope gain at one sample; the sample is skipped near a zero crossing. */
            bool gainAt (int i, float& g) const
            {
                const float d = reference.sample (i);
                if (std::abs (d) < 1.0e-3f)
                    return false;
                g = scenario.sample (i) / d;
                return true;
            }

            /** Minimum envelope gain over the block just fed (the duck's floor). */
            float minGain() const
            {
                float gMin = 1.0f;
                bool found = false;
                for (int i = 0; i < blockSize; ++i)
                {
                    float g = 0.0f;
                    if (gainAt (i, g)) { gMin = juce::jmin (gMin, g); found = true; }
                }
                return found ? gMin : 1.0f;
            }
        };

        beginTest ("normal path: a ratio step arms the ratio-jump duck");
        {
            Pair p;
            p.prepare();

            for (int b = 0; b < 35; ++b) // ~400 ms warm-up
                p.feed (1.0f);

            expect (p.minGain() > 0.999f,
                    "the envelope is fully open on a steady note (min gain="
                    + juce::String (p.minGain(), 4) + ")");

            // One-semitone step: > 3 % in one block, which is what arms the duck.
            // The ramp-down target is 0 and its time constant is 80 ms, so over the
            // 15 ms ramp the gain reaches roughly 0.83.
            p.feed (ratioUp);
            const float gainAfter = p.minGain();
            expect (gainAfter < 0.92f,
                    "the ratio-jump path must duck on a semitone step (min gain="
                    + juce::String (gainAfter, 4) + ")");
            expect (gainAfter > 0.50f,
                    "the duck must stay a dip, not a mute (min gain="
                    + juce::String (gainAfter, 4) + ")");
        }

        beginTest ("Hard Tune: with the ratio-jump duck off, a snap produces no duck at all");
        {
            Pair p;
            p.prepare();

            for (int b = 0; b < 35; ++b)
                p.feed (1.0f);

            p.scenario.ps.setRatioJumpEnvelopeEnabled (false); // the mode is on

            for (int rep = 0; rep < 4; ++rep)
            {
                p.feed (ratioUp);                              // the snap
                expect (p.minGain() > 0.999f,
                        "Hard Tune must not duck on a snap (min gain="
                        + juce::String (p.minGain(), 4) + ")");
                p.feed (1.0f);
                expect (p.minGain() > 0.999f,
                        "Hard Tune must not duck on the way back (min gain="
                        + juce::String (p.minGain(), 4) + ")");
            }
        }

        beginTest ("Hard Tune edge: leaving the mode arms the ratio-jump duck again");
        {
            Pair p;
            p.prepare();

            for (int b = 0; b < 35; ++b)
                p.feed (1.0f);

            // In the mode: a snap does not duck...
            p.scenario.ps.setRatioJumpEnvelopeEnabled (false);
            p.feed (ratioUp);
            expect (p.minGain() > 0.999f,
                    "no duck inside the mode (min gain="
                    + juce::String (p.minGain(), 4) + ")");

            // ...and leaving it restores the normal path for the NEXT step.
            p.scenario.ps.setRatioJumpEnvelopeEnabled (true);
            p.feed (1.0f);
            expect (p.minGain() < 0.92f,
                    "the duck must come back after the mode is left (min gain="
                    + juce::String (p.minGain(), 4) + ")");
        }

        beginTest ("Hard Tune keeps the onset mask: a note attack stays click-free");
        {
            // The onset arming is the other half of the envelope, and Hard Tune
            // must not touch it. This is the same measurement the click suite uses
            // (unit sine, 0.15 threshold): silence to finish the start-up fade, a
            // 200 Hz note, then semitone snaps. The ratio-jump duck is off, so the
            // only thing that can mask the onset is the onset arming itself.
            PitchShifter ps;
            ps.prepare (sr, blockSize);
            ps.setAttackTimeMs (30.0f);
            ps.setRatioJumpEnvelopeEnabled (false);

            juce::AudioBuffer<float> in  (1, blockSize);
            juce::AudioBuffer<float> out (1, blockSize);
            double t = 0.0;
            float prev = 0.0f;
            float maxJump = 0.0f;
            double maxJumpMs = 0.0;

            auto feed = [&] (float ratio)
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    in.setSample (0, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                          * (double) noteHz * t / sr));
                    ++t;
                }
                out.setSize (1, blockSize, false, true, false);
                ps.process (in, out, ratio, 1.0f, noteHz);
                for (int i = 0; i < blockSize; ++i)
                {
                    const float s = out.getSample (0, i);
                    const float jump = std::abs (s - prev);
                    if (jump > maxJump)
                    {
                        maxJump = jump;
                        maxJumpMs = (t - blockSize + i) * 1000.0 / sr;
                    }
                    prev = s;
                }
            };

            // Spend the 20 ms start-up fade in silence, like any real session does.
            for (int b = 0; b < 5; ++b)
            {
                in.clear();
                out.setSize (1, blockSize, false, true, false);
                ps.process (in, out, 1.0f, 1.0f, 0.0f);
                prev = 0.0f;
                t += blockSize;
            }

            for (int b = 0; b < 35; ++b) // warm-up
                feed (1.0f);

            for (int rep = 0; rep < 4; ++rep)
            {
                feed (ratioUp);
                feed (1.0f);
            }

            expect (maxJump < 0.15f,
                    "Hard Tune must keep the onset mask (max sample jump="
                    + juce::String (maxJump, 3) + " at " + juce::String (maxJumpMs, 1)
                    + " ms)");
        }
    }
};

static HardTuneAttackTest hardTuneAttackTest;
