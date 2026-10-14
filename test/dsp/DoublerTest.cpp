#pragma once
// DoublerTest.cpp
// Unit test for the stereo doubler (ovtdsp::Doubler).
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/Doubler.h"

/**
 * Shared test signal: broadband noise (dominant energy) plus inharmonic
 * partials. A pure tone would stay phase-locked through the Haas delay and
 * give the coherent +3 dB worst case, which is NOT what the constant-gain
 * claim is about; broadband content is what a real voice is, so the pair adds
 * in power and 1/sqrt(1+g^2) keeps the perceived RMS flat.
 */
class DoublerTest : public juce::UnitTest
{
public:
    DoublerTest() : juce::UnitTest ("Doubler") {}

    void runTest() override
    {
        using namespace ovtdsp;

        const double sr = 44100.0;
        const int block = 256;
        const int totalBlocks = 128;          // 743 ms: the 60 ms amount ramp is fully settled
        const int measureLen  = 64 * block;    // measure the last 64 blocks only

        // ----------------------------------------------------------------
        // 1) Parameter mapping: amount -> cents, ratio, Haas offset, copy level.
        // ----------------------------------------------------------------
        beginTest ("Amount mapping: detune capped at 10 cents, Haas floor at 5 ms");
        {
            expect (Doubler::detuneCents (0.0f) == 0.0f, "0 % must give 0 cent of detune.");
            expect (std::abs (Doubler::detuneCents (100.0f) - 10.0f) < 1e-4f,
                    "100 % must give exactly 10 cents (cap per docs/harmony-pad-prerequisites-analysis.md).");
            expect (std::abs (Doubler::detuneCents (50.0f) - 5.0f) < 1e-4f,
                    "50 % must give 5 cents.");

            // Exact ratio: 2^(10/1200) = 1.005827 (10 cents sharp).
            expect (std::abs (Doubler::detuneRatio (0.0f) - 1.0f) < 1e-6f,
                    "0 % must give ratio 1.0 (no detune).");
            const float r100 = Doubler::detuneRatio (100.0f);
            expect (std::abs (r100 - 1.005827f) < 1e-4f,
                    "100 % must give ratio 2^(10/1200) ~ 1.0058, measured = " + juce::String (r100, 6));
            const float r50 = Doubler::detuneRatio (50.0f);
            expect (std::abs (r50 - 1.002893f) < 1e-4f,
                    "50 % must give ratio 2^(5/1200) ~ 1.0029, measured = " + juce::String (r50, 6));

            // Haas offset: 0 at 0 %, floored at 5 ms once active, 25 ms at 100 %.
            expect (Doubler::haasDelaySamples (0.0f, sr) == 0.0f,
                    "0 % must give a zero Haas offset.");
            const float d1 = Doubler::haasDelaySamples (1.0f, sr);   // 0.25 ms requested -> floored to 5 ms
            expect (d1 >= 5.0f * 0.001f * static_cast<float> (sr) - 1e-3f,
                    "A tiny amount must still give the 5 ms Haas floor, measured = " + juce::String (d1, 2));
            const float d100 = Doubler::haasDelaySamples (100.0f, sr);
            expect (std::abs (d100 - 25.0f * 0.001f * static_cast<float> (sr)) < 1e-2f,
                    "100 % must give the 25 ms Haas offset, measured = " + juce::String (d100, 2));

            // Copy level never exceeds the lead (max 0.6 = -4.4 dB at 100 %).
            for (float a = 0.0f; a <= 100.0f; a += 5.0f)
                expect (Doubler::copyLevel (a) <= 1.0f,
                        "Copy level must never exceed the lead at amount " + juce::String (a, 1));
            expect (std::abs (Doubler::copyLevel (100.0f) - 0.6f) < 1e-4f,
                    "100 % copy level must be 0.6 (-4.4 dB).");

            // Pan law normalised to unity at centre: pan 0 -> (1, 1), so amount 0
            // leaves the lead path untouched.
            float gl, gr;
            Doubler::panGains (0.0f, gl, gr);
            expect (std::abs (gl - 1.0f) < 1e-3f && std::abs (gr - 1.0f) < 1e-3f,
                    "Pan 0 must give unity gains (1, 1).");
            Doubler::panGains (1.0f, gl, gr);
            expect (std::abs (gl - 0.0f) < 1e-3f && std::abs (gr - 1.41421f) < 1e-3f,
                    "Pan +1 must give (0, sqrt(2)).");
        }

        // ----------------------------------------------------------------
        // 2) Constant perceived RMS across the whole 0 -> 100 % range (stereo).
        // ----------------------------------------------------------------
        beginTest ("Perceived RMS stays constant from 0 % to 100 % (stereo)");
        {
            auto makeLead = [] (int blocks, int blk)
            {
                juce::AudioBuffer<float> lead (2, blocks * blk);
                unsigned int seed = 0x12345678u;
                for (int ch = 0; ch < 2; ++ch)
                {
                    float* d = lead.getWritePointer (ch);
                    for (int i = 0; i < blocks * blk; ++i)
                    {
                        seed = seed * 1664525u + 1013904223u;
                        const float noise = (static_cast<float> (seed >> 8) / 8388608.0f) - 1.0f;
                        const float t = static_cast<float> (i) / 44100.0f;
                        // Inharmonic partials: no shared fundamental, so the delayed
                        // copy is not phase-locked to the lead.
                        d[i] = 0.25f * noise
                             + 0.10f * std::sin (2.0f * juce::MathConstants<float>::pi * 317.0f * t)
                             + 0.08f * std::sin (2.0f * juce::MathConstants<float>::pi * 509.0f * t)
                             + 0.06f * std::sin (2.0f * juce::MathConstants<float>::pi * 719.0f * t);
                    }
                }
                return lead;
            };

            auto stereoRms = [] (const juce::AudioBuffer<float>& b, int start, int len)
            {
                double acc = 0.0;
                for (int ch = 0; ch < b.getNumChannels(); ++ch)
                {
                    const float* d = b.getReadPointer (ch);
                    for (int i = start; i < start + len; ++i)
                        acc += static_cast<double> (d[i]) * static_cast<double> (d[i]);
                }
                return std::sqrt (acc / static_cast<double> (b.getNumChannels() * len));
            };

            Doubler doubler;
            doubler.prepare (sr, block);

            auto measure = [&] (float amount) -> float
            {
                doubler.reset();
                doubler.setAmount (amount);

                juce::AudioBuffer<float> out = makeLead (totalBlocks, block);
                for (int b = 0; b < totalBlocks; ++b)
                {
                    juce::AudioBuffer<float> main (2, block);
                    for (int ch = 0; ch < 2; ++ch)
                        main.copyFrom (ch, 0, out, ch, b * block, block);
                    // The copy fed to the doubler is the lead content itself: in
                    // the plugin it is the pitch-shifted copy of the corrected lead.
                    doubler.process (main, main);
                    for (int ch = 0; ch < 2; ++ch)
                        out.copyFrom (ch, b * block, main, ch, 0, block);
                }
                return stereoRms (out, totalBlocks * block - measureLen, measureLen);
            };

            const float base = measure (0.0f);
            expect (base > 0.0f, "Baseline RMS must be non-zero.");

            for (float a : { 25.0f, 50.0f, 75.0f, 100.0f })
            {
                const float r = measure (a);
                const float db = 20.0f * std::log10 (r / base);
                expect (std::abs (db) < 2.0f,
                        "Amount " + juce::String (a, 0) + " % must keep the perceived RMS within +/- 2 dB of amount 0; measured = "
                        + juce::String (db, 2) + " dB");
            }
        }

        // ----------------------------------------------------------------
        // 3) Mono compatibility: summing L+R must not open a phase hole. The
        //    mono sum of the doubled signal must stay at or above the dry lead.
        // ----------------------------------------------------------------
        beginTest ("Mono sum (L+R) keeps the level: no phase hole");
        {
            Doubler doubler;
            doubler.prepare (sr, block);

            auto monoSumRms = [&] (float amount) -> float
            {
                doubler.reset();
                doubler.setAmount (amount);

                juce::AudioBuffer<float> out (2, totalBlocks * block);
                unsigned int seed = 0x9abcdef0u;
                for (int ch = 0; ch < 2; ++ch)
                {
                    float* d = out.getWritePointer (ch);
                    for (int i = 0; i < totalBlocks * block; ++i)
                    {
                        seed = seed * 1664525u + 1013904223u;
                        const float noise = (static_cast<float> (seed >> 8) / 8388608.0f) - 1.0f;
                        const float t = static_cast<float> (i) / 44100.0f;
                        d[i] = 0.25f * noise
                             + 0.10f * std::sin (2.0f * juce::MathConstants<float>::pi * 317.0f * t)
                             + 0.08f * std::sin (2.0f * juce::MathConstants<float>::pi * 509.0f * t);
                    }
                }

                for (int b = 0; b < totalBlocks; ++b)
                {
                    juce::AudioBuffer<float> main (2, block);
                    for (int ch = 0; ch < 2; ++ch)
                        main.copyFrom (ch, 0, out, ch, b * block, block);
                    doubler.process (main, main);
                    for (int ch = 0; ch < 2; ++ch)
                        out.copyFrom (ch, b * block, main, ch, 0, block);
                }

                double acc = 0.0;
                const int start = totalBlocks * block - measureLen;
                for (int i = start; i < start + measureLen; ++i)
                {
                    const float sum = out.getReadPointer (0)[i] + out.getReadPointer (1)[i];
                    acc += static_cast<double> (sum) * static_cast<double> (sum);
                }
                return std::sqrt (acc / static_cast<double> (measureLen));
            };

            const float dry = monoSumRms (0.0f);
            expect (dry > 0.0f, "Mono baseline must be non-zero.");

            for (float a : { 25.0f, 50.0f, 100.0f })
            {
                const float m = monoSumRms (a);
                const float db = 20.0f * std::log10 (m / dry);
                expect (db > -1.0f,
                        "Mono sum at amount " + juce::String (a, 0) + " % must not collapse below the dry lead; measured = "
                        + juce::String (db, 2) + " dB");
            }

            // Single-channel main must also work (mono plugin bus).
            doubler.reset();
            doubler.setAmount (100.0f);
            juce::AudioBuffer<float> mono (1, block);
            for (int i = 0; i < block; ++i)
                mono.setSample (0, i, 0.3f * std::sin (0.2f * static_cast<float> (i)));
            doubler.process (mono, mono);
            bool finite = true;
            for (int i = 0; i < block; ++i)
                if (! std::isfinite (mono.getReadPointer (0)[i]))
                    finite = false;
            expect (finite, "Mono bus processing must produce finite samples.");
        }

        // ----------------------------------------------------------------
        // 4) CPU short-circuit at amount 0: the module leaves the lead buffer
        //    bit-identical, and isActive() is false so the caller skips the
        //    dedicated pitch shifter entirely.
        // ----------------------------------------------------------------
        beginTest ("Amount 0 is a no-op: lead stays bit-identical, shifter skipped");
        {
            Doubler doubler;
            doubler.prepare (sr, block);
            doubler.setAmount (0.0f);
            expect (! doubler.isActive(), "Amount 0 must report inactive (CPU short-circuit).");

            juce::AudioBuffer<float> main (2, block);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    main.setSample (ch, i, static_cast<float> (std::sin (0.1f * i) * 0.5));

            juce::AudioBuffer<float> reference (2, block);
            reference.copyFrom (0, 0, main, 0, 0, block);
            reference.copyFrom (1, 0, main, 1, 0, block);

            int mismatches = 0;
            for (int b = 0; b < 8; ++b)
            {
                doubler.process (main, main);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < block; ++i)
                        if (main.getReadPointer (ch)[i] != reference.getReadPointer (ch)[i])
                            ++mismatches;
            }
            expect (mismatches == 0,
                    "Output must be bit-identical to the input at amount 0 (" + juce::String (mismatches) + " samples changed).");

            // Re-activation from 0: reset() clears the delay line, so the first
            // blocks read silence instead of stale audio from a previous take.
            doubler.reset();
            doubler.setAmount (100.0f);
            expect (doubler.isActive(), "Amount 100 % must report active.");

            juce::AudioBuffer<float> silentCopy (2, block);
            silentCopy.clear();
            juce::AudioBuffer<float> silentMain (2, block);
            silentMain.clear();
            doubler.process (silentCopy, silentMain);
            bool allZero = true;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    if (silentMain.getReadPointer (ch)[i] != 0.0f)
                        allZero = false;
            expect (allZero, "After reset() the copy path must read silence, not stale audio.");
        }
    }
};

static DoublerTest doublerTest;
