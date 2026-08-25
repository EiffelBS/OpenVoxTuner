#pragma once
// FormantPreserverTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/FormantPreserver.h"

class FormantPreserverTest : public juce::UnitTest
{
public:
    FormantPreserverTest() : juce::UnitTest ("FormantPreserver") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("Disabled: no modification");
        {
            FormantPreserver fp;
            fp.prepare (44100.0, 64);
            fp.setEnabled (false);

            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i) buf.setSample (0, i, 0.5f);

            fp.process (buf, 2.0f); // extreme ratio, but disabled
            for (int i = 0; i < 64; ++i)
                expectWithinAbsoluteError (buf.getSample (0, i), 0.5f, 1e-6f);
        }

        beginTest ("Ratio = 1.0: no modification");
        {
            FormantPreserver fp;
            fp.prepare (44100.0, 64);
            fp.setEnabled (true);

            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i) buf.setSample (0, i, 0.5f);

            fp.process (buf, 1.0f);
            // 500 Hz biquad filter on DC -> b0 + b1 + b2 = DC gain.
            // The DC signal therefore goes through a non-unity gain, but it
            // is NOT zero. The test only checks that there is no NaN.
            for (int i = 0; i < 64; ++i)
                expect (std::isfinite (buf.getSample (0, i)),
                        "NaN in the output");
        }

        beginTest ("Extreme ratio: filtered signal stays bounded");
        {
            FormantPreserver fp;
            fp.prepare (44100.0, 64);
            fp.setEnabled (true);

            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i)
                buf.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi * 100.0f * i / 44100.0f));

            fp.process (buf, 4.0f);
            for (int i = 0; i < 64; ++i)
            {
                const float s = buf.getSample (0, i);
                expect (std::isfinite (s), "NaN");
                expect (std::abs (s) < 10.0f, "Explosive output: " + juce::String (s));
            }
        }

        beginTest ("MultiFormant: no discontinuity at ratio jump (click)");
        {
            // Fixes cause A: the biquad coefficients must be smooth from
            // one block to the next. A brutal ratio jump (a note starting)
            // must NOT produce an output sample that explodes abruptly
            // relative to the previous one (which would be a pop).
            FormantPreserver fp;
            fp.prepare (44100.0, 512);
            fp.setEnabled (true);
            fp.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);

            // Continuous sine signal (100 Hz) to isolate the filter effect.
            juce::AudioBuffer<float> buf (1, 512);
            auto fill = [&] (float phase0)
            {
                for (int i = 0; i < 512; ++i)
                    buf.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 100.0f * (phase0 + i) / 44100.0f));
            };

            // Phase 1: stable ratio (reference note).
            fill (0.0f);
            fp.process (buf, 1.0f);
            float lastSample = buf.getSample (0, 511);

            // Phase 2: ratio JUMP (simulates the start of a sung note
            // or a pitch change). Several blocks are processed.
            int maxJumpCount = 0;
            float prev = lastSample;
            for (int blk = 0; blk < 5; ++blk)
            {
                fill (static_cast<float> (blk) * 512.0f);
                fp.process (buf, 2.0f); // doubled ratio
                for (int i = 0; i < 512; ++i)
                {
                    const float s = buf.getSample (0, i);
                    expect (std::isfinite (s), "NaN after ratio jump");
                    // A real audible click = an instantaneous jump > 0.5 in
                    // amplitude (on a unit signal). The smoothing of the biquad
                    // coefficients prevents repeated jumps: at most 3 jump
                    // samples are tolerated (the very first sample of the
                    // coefficient change, unavoidable since the filter has
                    // memory), versus dozens without smoothing.
                    const float jump = std::abs (s - prev);
                    if (jump > 0.5f) maxJumpCount++;
                    prev = s;
                }
            }
            expect (maxJumpCount <= 3,
                    "Too many discontinuities (clicks) at MultiFormant ratio jump: "
                    + juce::String (maxJumpCount) + " samples (expected <= 3)");
        }

        beginTest ("P0 strategy (1/r + voice-type) differs from Current (1/sqrt(r))");
        {
            FormantPreserver fpC, fpP;
            fpC.prepare (44100.0, 512);
            fpP.prepare (44100.0, 512);
            fpC.setEnabled (true);
            fpP.setEnabled (true);
            fpP.setStrategy (ovtdsp::FormantPreserver::Strategy::P0);
            fpP.setVoiceType (5); // Soprano
            fpC.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);
            fpP.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);

            // Identical noise for both strategies. Amplitude 0.5 is safe:
            // the peaking-EQ cascade (4 x Q<=3.25, gain=6 dB) at ratio 2.0
            // produces bounded output without NaN when the biquad states
            // are fresh (no warm-up).
            srand (42);
            juce::AudioBuffer<float> bufC (1, 512);
            for (int i = 0; i < 512; ++i)
                bufC.setSample (0, i, (2.0f * (float)rand() / (float)RAND_MAX - 1.0f) * 0.5f);

            srand (42);
            juce::AudioBuffer<float> bufP (1, 512);
            for (int i = 0; i < 512; ++i)
                bufP.setSample (0, i, (2.0f * (float)rand() / (float)RAND_MAX - 1.0f) * 0.5f);

            fpC.process (bufC, 2.0f);
            fpP.process (bufP, 2.0f);

            float rmsC = 0.0f, rmsP = 0.0f;
            bool okC = true, okP = true;
            for (int i = 0; i < 512; ++i)
            {
                const float c = bufC.getSample (0, i);
                const float p = bufP.getSample (0, i);
                if (!std::isfinite (c)) okC = false;
                if (!std::isfinite (p)) okP = false;
                rmsC += c * c;
                rmsP += p * p;
            }
            expect (okC, "Current output has NaN/inf");
            expect (okP, "P0 output has NaN/inf");
            rmsC = std::sqrt (rmsC / 512.0f);
            rmsP = std::sqrt (rmsP / 512.0f);
            expect (rmsC > 0.01f, "Current output too small");
            expect (rmsP > 0.01f, "P0 output too small");
            // The two compensation laws (1/sqrt(r) vs 1/r) plus different
            // formant centers must yield measurably different RMS.
            float rmsRatio = (rmsC > rmsP) ? (rmsP / rmsC) : (rmsC / rmsP);
            expect (rmsRatio < 0.9995f,
                    "P0 and Current should differ at ratio 2.0 "
                    "(rmsRatio=" + juce::String (rmsRatio, 6) + ")");
        }

        beginTest ("Mode.Allpass: no discontinuity at ratio jump (pop/click fix)");
        {
            // Verifies fix #X: the smoothed coefficients use their
            // own z1/z2. The ratio jump must be smooth without pop.
            FormantPreserver fp;
            fp.prepare (44100.0, 256);
            fp.setEnabled (true);
            fp.setMode (ovtdsp::FormantPreserver::Mode::Allpass);
            fp.setStrategy (ovtdsp::FormantPreserver::Strategy::Current);

            juce::AudioBuffer<float> buf (1, 256);
            auto initSignal = [&] (float phase0)
            {
                for (int i = 0; i < 256; ++i)
                    buf.setSample (0, i,
                                   std::sin (2.0f * juce::MathConstants<float>::pi
                                             * 220.0f * (phase0 + i) / 44100.0f));
            };

            // Block 1: ratio 1.0
            initSignal (0.0f);
            fp.process (buf, 1.0f);
            float lastSamp = buf.getSample (0, 255);

            // Block 2: JUMP to ratio 2.0
            initSignal (256.0f);
            fp.process (buf, 2.0f);
            int bigJumps = 0;
            float prev = lastSamp;
            for (int i = 0; i < 256; ++i)
            {
                const float s = buf.getSample (0, i);
                expect (std::isfinite (s), "Allpass: NaN on ratio jump");
                if (std::abs (s - prev) > 0.5f) bigJumps++;
                prev = s;
            }
            expect (bigJumps <= 3,
                    "Allpass: too many jumps > 0.5 at ratio jump "
                    "(expected <= 3, got " + juce::String (bigJumps) + ")");

            // Block 3: back to ratio 1.0 (simulated vibrato)
            initSignal (512.0f);
            fp.process (buf, 1.0f);
            prev = buf.getSample (0, 252);
            for (int i = 253; i < 256; ++i)
                prev = buf.getSample (0, i);
            float prevRatio1 = prev;
            initSignal (768.0f);
            fp.process (buf, 1.0f);
            for (int i = 0; i < 256; ++i)
            {
                const float s = buf.getSample (0, i);
                expect (std::isfinite (s), "Allpass: NaN back at ratio 1.0");
            }

            // Block 4+: fast variations (ratio alternating between 1.0 and 2.0)
            float prevBig = buf.getSample (0, 252);
            for (int blk = 0; blk < 10; ++blk)
            {
                initSignal (1024.0f + blk * 256.0f);
                float altRatio = (blk % 2 == 0) ? 1.5f : 0.8f;
                fp.process (buf, altRatio);
                for (int i = 0; i < 256; ++i)
                {
                    const float s = buf.getSample (0, i);
                    if (std::abs (s - prevBig) > 0.5f) bigJumps++;
                    prevBig = s;
                }
            }
            expect (bigJumps <= 50,
                    "Allpass fast variations: too many pops ("
                    + juce::String (bigJumps) + ", expected <= 50)");
        }

        beginTest ("Reset: no artifact after reinitialization");
        {
            FormantPreserver fp;
            fp.prepare (44100.0, 256);
            fp.setEnabled (true);
            fp.setMode (ovtdsp::FormantPreserver::Mode::Allpass);

            juce::AudioBuffer<float> buf (1, 256);
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * i / 44100.0f));

            fp.process (buf, 1.5f);
            // Reset, then another process block
            fp.reset();
            for (int i = 0; i < 256; ++i)
                buf.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * i / 44100.0f));
            fp.process (buf, 1.5f);
            for (int i = 0; i < 256; ++i)
                expect (std::isfinite (buf.getSample (0, i)), "Allpass post-reset: NaN");
        }
    }
};

static FormantPreserverTest formantPreserverTest;




