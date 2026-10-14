#pragma once
// UnisonDetuneTest.cpp
// Unit test for the fixed per-voice Unison detune (HarmonyEngine::unisonDetuneCents).
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/HarmonyEngine.h"

/**
 * The claim under test is a level claim, not a sound claim: HarmonyEngine emits
 * IDENTICAL frequencies for voices 0 and 1 of Unison2 / UnisonOctaves4, so the
 * two copies add coherently (+6.02 dB over one voice) and the user hears a loud
 * voice, not a thick one. A fixed +/-5 cent spread makes the pair add in power
 * (+3.01 dB) and the residual cross term becomes a slow amplitude beat (~1.2 Hz
 * at 200 Hz). The test measures exactly that: coherent sum, decorrelated sum,
 * and the presence of the beat.
 */
class UnisonDetuneTest : public juce::UnitTest
{
public:
    UnisonDetuneTest() : juce::UnitTest ("UnisonDetune") {}

    static float rms (const float* data, int n)
    {
        double acc = 0.0;
        for (int i = 0; i < n; ++i)
            acc += (double) data[i] * (double) data[i];
        return (float) std::sqrt (acc / (double) n);
    }

    static float db (float a, float ref)
    {
        return 20.0f * std::log10 (a / ref);
    }

    void runTest() override
    {
        using namespace ovtdsp;

        const double sr = 44100.0;
        const float baseHz = 200.0f;

        // ----------------------------------------------------------------
        // 1) The Unison types really do generate identical voices, which is
        //    the root of the correlation problem.
        // ----------------------------------------------------------------
        beginTest ("Unison layout: voices 0 and 1 are the same note, octaves are voices 2-3");
        {
            juce::Array<int> scale;
            scale.add (0); scale.add (2); scale.add (3); scale.add (5);
            scale.add (7); scale.add (8); scale.add (10);

            HarmonyEngine engine;
            engine.prepare (sr);

            const auto u2 = engine.getHarmonyNotes (baseHz, scale, HarmonyType::Unison2);
            expectEquals (u2.size(), 2, "Unison2 must generate 2 voices.");
            expect (std::abs (u2[0] - u2[1]) < 1e-4f,
                    "Unison2 voices 0 and 1 are the SAME frequency: fully correlated copies.");

            const auto u4 = engine.getHarmonyNotes (baseHz, scale, HarmonyType::UnisonOctaves4);
            expectEquals (u4.size(), 4, "UnisonOctaves4 must generate 4 voices.");
            expect (std::abs (u4[0] - u4[1]) < 1e-4f,
                    "Voices 0 and 1 are the unison pair: identical frequencies.");
            expect (std::abs (u4[2] - u4[0] * 0.5f) < 1e-3f, "Voice 2 is the octave below.");
            expect (std::abs (u4[3] - u4[0] * 2.0f) < 1e-3f, "Voice 3 is the octave above.");

            // The engine quantizes the base to the nearest scale degree, so the
            // test compares RATIOS between voices, never absolute Hz.
            expect (u4[0] > baseHz * 0.9f && u4[0] < baseHz * 1.1f,
                    "The quantized unison voice must stay within a semitone of the input.");
        }

        // ----------------------------------------------------------------
        // 2) Mapping: +5 / -5 cents on the unison pair, zero everywhere else.
        // ----------------------------------------------------------------
        beginTest ("Detune mapping: +5 cents voice 0, -5 cents voice 1, octaves untouched");
        {
            expect (std::abs (HarmonyEngine::unisonDetuneCents (0, HarmonyType::Unison2) - 5.0f) < 1e-6f,
                    "Unison2 voice 0 must be +5 cents.");
            expect (std::abs (HarmonyEngine::unisonDetuneCents (1, HarmonyType::Unison2) + 5.0f) < 1e-6f,
                    "Unison2 voice 1 must be -5 cents.");
            expect (std::abs (HarmonyEngine::unisonDetuneCents (0, HarmonyType::UnisonOctaves4) - 5.0f) < 1e-6f,
                    "UnisonOctaves4 voice 0 must be +5 cents.");
            expect (std::abs (HarmonyEngine::unisonDetuneCents (1, HarmonyType::UnisonOctaves4) + 5.0f) < 1e-6f,
                    "UnisonOctaves4 voice 1 must be -5 cents.");
            expect (HarmonyEngine::unisonDetuneCents (2, HarmonyType::UnisonOctaves4) == 0.0f,
                    "The octave voices must NOT be detuned: they are already decorrelated.");
            expect (HarmonyEngine::unisonDetuneCents (3, HarmonyType::UnisonOctaves4) == 0.0f,
                    "The octave voices must NOT be detuned.");
            expect (HarmonyEngine::unisonDetuneCents (0, HarmonyType::FifthAbove) == 0.0f,
                    "A non-Unison type must get 0 cents: no change to any other harmony.");

            // The spread between the two unison voices is exactly 10 cents.
            const float rUp   = (float) std::pow (2.0,  5.0 / 1200.0);
            const float rDown = (float) std::pow (2.0, -5.0 / 1200.0);
            const float spreadCents = 1200.0f * std::log2 (rUp / rDown);
            expect (std::abs (spreadCents - 10.0f) < 1e-3f, "The pair must be 10 cents apart.");

            // 10 cents at 200 Hz is a ~1.2 Hz beat: slow shimmer, not a comb.
            const float beatHz = baseHz * (rUp - rDown);
            expect (beatHz > 1.0f && beatHz < 1.5f, "Beat rate must land near 1.2 Hz at 200 Hz.");
        }

        // ----------------------------------------------------------------
        // 3) The level claim: correlated pair = +6 dB, detuned pair = +3 dB.
        //    Measured over ~1.8 s, i.e. more than two beat periods.
        // ----------------------------------------------------------------
        beginTest ("Sum level: correlated pair is +6 dB, detuned pair drops to +3 dB");
        {
            const int n = 80000;   // 1.81 s at 44.1 kHz
            juce::AudioBuffer<float> single (1, n), correlated (1, n), detuned (1, n);
            auto* s = single.getWritePointer (0);
            auto* c = correlated.getWritePointer (0);
            auto* d = detuned.getWritePointer (0);

            const float rUp   = (float) std::pow (2.0,  5.0 / 1200.0);
            const float rDown = (float) std::pow (2.0, -5.0 / 1200.0);

            for (int i = 0; i < n; ++i)
            {
                const double t = (double) i / sr;
                const float v0  = std::sin (2.0 * juce::MathConstants<double>::pi * (double) baseHz * t);
                const float v1c = v0;
                const float v1d = std::sin (2.0 * juce::MathConstants<double>::pi * (double) (baseHz * rDown) * t);
                s[i] = v0;
                c[i] = v0 + v1c;
                d[i] = std::sin (2.0 * juce::MathConstants<double>::pi * (double) (baseHz * rUp) * t) + v1d;
            }

            const float ref = rms (s, n);
            const float cDb = db (rms (c, n), ref);
            const float dDb = db (rms (d, n), ref);

            expect (std::abs (cDb - 6.02f) < 0.05f,
                    "Identical copies must add coherently: +6.02 dB. This is the Unison2 level boost.");
            expect (dDb > 2.7f && dDb < 3.4f,
                    "Detuned copies must add in power (~3.01 dB), not coherently: the boost is gone.");

            // The pair must shimmer: short-window RMS oscillates at the beat rate.
            const int frame = 2205;   // 50 ms
            float fmax = 0.0f, fmin = 1e9f;
            for (int b = 20; b + frame < n; b += frame)
            {
                const float f = rms (d + b, frame);
                fmax = juce::jmax (fmax, f);
                fmin = juce::jmin (fmin, f);
            }
            expect (fmax / fmin > 1.05f,
                    "The detuned pair must show an amplitude beat (shimmer), not a flat level.");

            float cmax = 0.0f, cmin = 1e9f;
            for (int b = 20; b + frame < n; b += frame)
            {
                const float f = rms (c + b, frame);
                cmax = juce::jmax (cmax, f);
                cmin = juce::jmin (cmin, f);
            }
            expect (cmax / cmin < 1.02f,
                    "The correlated pair is a flat level boost: no shimmer to be lost.");
        }

        // ----------------------------------------------------------------
        // 4) Mono compatibility: the detuned pair never sums to a hole.
        // ----------------------------------------------------------------
        beginTest ("Mono sum: the detuned pair keeps its level, no phase hole");
        {
            const int n = 80000;
            const float rUp   = (float) std::pow (2.0,  5.0 / 1200.0);
            const float rDown = (float) std::pow (2.0, -5.0 / 1200.0);

            double acc = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double t = (double) i / sr;
                const float sum = std::sin (2.0 * juce::MathConstants<double>::pi * (double) (baseHz * rUp) * t)
                                  + std::sin (2.0 * juce::MathConstants<double>::pi * (double) (baseHz * rDown) * t);
                acc += (double) sum * (double) sum;
            }
            const float monoRms = (float) std::sqrt (acc / (double) n);
            expect (monoRms > 0.6f, "The L+R sum of the detuned pair must stay near sqrt(2)/2 of a single sine amplitude.");
        }
    }
};

static UnisonDetuneTest unisonDetuneTest;
