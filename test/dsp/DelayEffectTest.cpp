#pragma once
// DelayEffectTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>

#include "../../Source/dsp/DelayEffect.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <vector>

namespace
{
    // Helpers for the Delay suite. This file is #included into Main.cpp, so every name carries a dvx
    // prefix to stay clear of the other suites' helpers (rvx = reverb, sat = saturation).

    constexpr double dvxSr          = 48000.0;
    constexpr int    dvxBlock       = 512;                                                       // a realistic host block size
    const int        dvxRampSamples = static_cast<int> (std::llround (0.025 * dvxSr));              // 1200 samples = the 25 ms convention

    /** The division table the UI knob uses, in beat fractions. Mirrored by VALUE from DelayEffect.h:
        if the class changes its fractions this constant makes the suite fail loudly instead of quietly
        agreeing with whatever the code now does (the same rule as kReturnTrimTest in ReverbEffectTest).
        The table is ordered by repetition length, exactly like the knob: 1/8T, 1/8, 1/4T, 1/8D,
        1/4 (the default), 1/2T, 1/4D, 1/2, 1/2D. */
    constexpr float kFractionByIndex[9] = { 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f,
                                            4.0f / 3.0f, 1.5f, 2.0f, 3.0f };

    /** Named division indices of that duration-sorted table, so a test that needs one musical value
        does not carry a magic number (0 = 1/8T, 1 = 1/8, 2 = 1/4T, 3 = 1/8D, 4 = 1/4, 5 = 1/2T,
        6 = 1/4D, 7 = 1/2, 8 = 1/2D). The knob turns in exactly this order. */
    constexpr int kDivEighthTriplet  = 0;
    constexpr int kDivEighth         = 1;
    constexpr int kDivQuarterTriplet = 2;
    constexpr int kDivDottedEighth   = 3;
    constexpr int kDivQuarter        = 4;
    constexpr int kDivHalfTriplet    = 5;
    constexpr int kDivDottedQuarter  = 6;
    constexpr int kDivHalf           = 7;
    constexpr int kDivHalfDotted     = 8;

    /** The contract maths computed independently of the class: copySamples = round((60/bpm)*fraction*sr). */
    int dvxExpectedCopy (double bpm, int divisionIndex)
    {
        return static_cast<int> (std::llround ((60.0 / bpm) * static_cast<double> (kFractionByIndex[divisionIndex]) * dvxSr));
    }

    /** The clamp floor the class documents: max(1, round(0.04 * sr)) = 1920 samples at 48 kHz. */
    int dvxFloorSamples()
    {
        return juce::jmax (1, static_cast<int> (std::llround (0.04 * dvxSr)));
    }

    /** Source generator: writes the L/R samples for one global sample index. Impulses and tones are built
        as lambdas, so every case is deterministic — no RNG, no host state, no transport, phase-continuous. */
    using dvxSource = std::function<void (int globalSample, float& left, float& right)>;

    /** Run nBlocks blocks through the effect and record both output channels as one continuous timeline.
        A parameter change is applied at a BLOCK boundary via onBlock — exactly where a host hands parameters
        over — so the ramp inside process() is what moves the copy length, not the setter call itself. */
    void dvxRun (ovtdsp::DelayEffect& d, int nBlocks, bool enabled, const dvxSource& source,
                 std::vector<float>& recL, std::vector<float>& recR,
                 const std::function<void (int blockIndex)>& onBlock = nullptr)
    {
        recL.assign (static_cast<size_t> (nBlocks) * dvxBlock, 0.0f);
        recR.assign (static_cast<size_t> (nBlocks) * dvxBlock, 0.0f);

        juce::AudioBuffer<float> buf (2, dvxBlock);

        for (int k = 0; k < nBlocks; ++k)
        {
            if (onBlock != nullptr)
                onBlock (k);

            const int base = k * dvxBlock;

            for (int s = 0; s < dvxBlock; ++s)
            {
                float l = 0.0f, r = 0.0f;
                source (base + s, l, r);
                buf.setSample (0, s, l);
                buf.setSample (1, s, r);
            }

            d.process (buf, enabled, 1.0f);   // the wiring callback hands wetMix = 1.0

            for (int s = 0; s < dvxBlock; ++s)
            {
                recL[base + s] = buf.getSample (0, s);
                recR[base + s] = buf.getSample (1, s);
            }
        }
    }

    /** Warm-up: reset() re-seeds the copy ramp at the comb floor, so silence is fed FIRST and fills the
        lines with zeros. Feeding an impulse AFTER this makes the first measurable echo land at the settled
        copy length; a position test without warm-up measures the ramp, not the delay. */
    void dvxWarmUp (ovtdsp::DelayEffect& d, int blocks = 3)
    {
        const dvxSource silent = [] (int, float& l, float& r) { l = 0.0f; r = 0.0f; };
        std::vector<float> warmL, warmR;
        dvxRun (d, blocks, true, silent, warmL, warmR);
    }

    /** Peak magnitude in a window: the peak is what an echo is. */
    float dvxPeak (const std::vector<float>& rec, int start, int end)
    {
        float p = 0.0f;
        const int lo = juce::jmax (0, start);
        const int hi = juce::jmin (end, static_cast<int> (rec.size()));
        for (int i = lo; i < hi; ++i)
            p = juce::jmax (p, std::abs (rec[i]));
        return p;
    }

    /** Mean magnitude in a window: the DC guard reads this. */
    float dvxMean (const std::vector<float>& rec, int start, int end)
    {
        double sum = 0.0;
        const int lo = juce::jmax (0, start);
        const int hi = juce::jmin (end, static_cast<int> (rec.size()));
        for (int i = lo; i < hi; ++i)
            sum += static_cast<double> (rec[i]);
        return static_cast<float> (sum / static_cast<double> (juce::jmax (1, hi - lo)));
    }

    /** RMS in a window: energy is what the ping-pong image claim rests on. */
    float dvxRms (const std::vector<float>& rec, int start, int end)
    {
        double sum = 0.0;
        const int lo = juce::jmax (0, start);
        const int hi = juce::jmin (end, static_cast<int> (rec.size()));
        for (int i = lo; i < hi; ++i)
            sum += static_cast<double> (rec[i]) * static_cast<double> (rec[i]);
        return static_cast<float> (std::sqrt (sum / static_cast<double> (juce::jmax (1, hi - lo))));
    }

    /** Biggest sample-to-sample jump in a window: a phase discontinuity shows up here as one big spike. */
    float dvxMaxJump (const std::vector<float>& rec, int start, int end)
    {
        float m = 0.0f;
        const int lo = juce::jmax (1, start);
        const int hi = juce::jmin (end, static_cast<int> (rec.size()));
        for (int i = lo; i < hi; ++i)
            m = juce::jmax (m, std::abs (rec[i] - rec[i - 1]));
        return m;
    }

    /** First index at/after `from` whose magnitude crosses `threshold`, or -1. */
    int dvxFirstAbove (const std::vector<float>& rec, int from, float threshold)
    {
        for (int i = juce::jmax (0, from); i < static_cast<int> (rec.size()); ++i)
            if (std::abs (rec[i]) > threshold) return i;
        return -1;
    }

    /** Bit-identity: bypass must leave the buffer EXACTLY as it found it, so equality with no tolerance is
        the assertion — same helper philosophy as sendBuffersIdentical in ReverbEffectTest. */
    bool dvxBuffersIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        if (a.getNumChannels() != b.getNumChannels() || a.getNumSamples() != b.getNumSamples())
            return false;

        for (int c = 0; c < a.getNumChannels(); ++c)
            for (int i = 0; i < a.getNumSamples(); ++i)
                if (a.getSample (c, i) != b.getSample (c, i))
                    return false;

        return true;
    }

    /** Deterministic centred voice material, phase-continuous across blocks (same philosophy as roomSignal).
        A centred source matters: the L/R spread this suite asserts must come from the effect, not the input. */
    void dvxVoice (int n, float* writeL, float* writeR)
    {
        const double x = static_cast<double> (n);
        const float v = 0.35f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * x / dvxSr));
        if (writeL != nullptr) writeL[0] = v;
        if (writeR != nullptr) writeR[0] = v;
    }

    /** Prepared instance with bpm/division/mix/feedback set BEFORE prepare + reset, so the copy target is
        known before any audio moves — the order the plugin uses at session start. */
    void dvxSetup (ovtdsp::DelayEffect& d, double bpm, int division, float mix, float feedback)
    {
        d.setBpm (bpm);
        d.setDivision (division);
        d.setMix (mix);
        d.setFeedback (feedback);

        d.prepare (dvxSr, dvxBlock);
        d.reset();
    }

    /** An impulse in ONE channel only: the response to it IS the delay's signature, so an arrival time read
        off an impulse is a known shape. */
    dvxSource dvxImpulse (int atSample, int channel, float amp = 1.0f)
    {
        return [atSample, channel, amp] (int n, float& l, float& r)
        {
            l = 0.0f; r = 0.0f;

            if (n == atSample)
            {
                if (channel == 0) l = amp;
                else              r = amp;
            }
        };
    }

    /** A sustained low tone: the material a feedback loop builds DC on (breath / plosive thump). */
    dvxSource dvxLowTone (float freq, float amp)
    {
        return [freq, amp] (int n, float& l, float& r)
        {
            const double x = static_cast<double> (n);
            const float v = amp * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * freq * x / dvxSr));
            l = v; r = v;
        };
    }

    /** A sustained tone feeding BOTH lines, so the cross-feedback is exercised on continuous material: a
        phase discontinuity in the copy ramp is audible as a click here and nowhere else. */
    dvxSource dvxTone (float freq, float amp)
    {
        return [freq, amp] (int n, float& l, float& r)
        {
            const double x = static_cast<double> (n);
            const float v = amp * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * freq * x / dvxSr));
            l = v;
            r = 0.5f * v;   // side-weighted so both lines carry energy
        };
    }

    /** Echo window around an expected arrival: narrow enough to skip the next echo, wide enough for the
        fractional read-out that spreads a single-sample impulse over two samples. */
    constexpr int kEchoHalfWindow = 64;

    /** Blocks needed for the enable ramp plus the tail decay of one-pole bypassGain down to kBypassFloor:
        exp(-n / rampSamples) < 1e-4 needs ~9.2 ramps, so 10 is the documented budget. */
    int dvxDecayBlocks()
    {
        return static_cast<int> (std::ceil ((10.0 * dvxRampSamples) / dvxBlock));
    }
}

class DelayEffectTest : public juce::UnitTest
{
public:
    DelayEffectTest() : juce::UnitTest ("DelayEffect") {}

    void runTest() override
    {
        // === A. Bit-identical bypass ===

        beginTest ("A disabled delay writes nothing once the ramp and the tail have decayed");
        {
            // Toggling power off must not re-shape a single sample after decay: a lead vocal muted on the
            // strip that keeps getting processed reads as a phantom effect, and the bypass has to be neutral.
            ovtdsp::DelayEffect d;
            dvxSetup (d, 120.0, kDivQuarter, 0.45f, 0.6f);

            const int decayBlocks = dvxDecayBlocks();   // ramp + tail budget at this block size
            juce::AudioBuffer<float> buf (2, dvxBlock);

            int firstTouchedBlock = -1;
            int lastTouchedBlock  = -1;

            for (int k = 0; k < decayBlocks + 8; ++k)
            {
                for (int s = 0; s < dvxBlock; ++s)
                    dvxVoice (k * dvxBlock + s, buf.getWritePointer (0) + s, buf.getWritePointer (1) + s);

                const juce::AudioBuffer<float> reference = buf;   // the input, unprocessed
                d.process (buf, false, 0.3f);                     // disabled: the mix value must be irrelevant

                if (! dvxBuffersIdentical (buf, reference))
                {
                    if (firstTouchedBlock < 0) firstTouchedBlock = k;
                    lastTouchedBlock = k;
                }
            }

            std::cout << "A: disabled stage touched blocks [" << firstTouchedBlock << ".." << lastTouchedBlock
                      << "] out of " << (decayBlocks + 8) << ", decay budget ends at block " << decayBlocks << "\n";

            // The point of the contract: once the ramp and the tail have decayed, NOTHING is written.
            expect (lastTouchedBlock < decayBlocks,
                    "A disabled delay kept writing samples past its ramp + decay window");

            // And the state says so: after that budget the stage reports itself fully bypassed.
            d.process (buf, false, 1.0f);
            // Note: expect(), not expectEquals() -- the latter streams the value into the
            // failure message and juce::String deletes its bool operator<< on purpose.
            expect (d.isBypassed(), "The delay kept processing while disabled");
        }

        beginTest ("A wet mix of zero is a no-op at full enable: the voice stays bit-identical");
        {
            // IEffect::process documents wetMix as the dry/wet amount. The contribution here is
            // bypassGain * (mix * wetMix), so wetMix = 0 must change nothing, block after block.
            ovtdsp::DelayEffect d;
            dvxSetup (d, 120.0, kDivQuarter, 0.6f, 0.7f);

            juce::AudioBuffer<float> buf (2, dvxBlock);
            int touched = -1;

            for (int k = 0; k < 40; ++k)
            {
                for (int s = 0; s < dvxBlock; ++s)
                    dvxVoice (k * dvxBlock + s, buf.getWritePointer (0) + s, buf.getWritePointer (1) + s);

                const juce::AudioBuffer<float> reference = buf;
                d.process (buf, true, 0.0f);   // enabled, and nothing is handed to the output

                if (! dvxBuffersIdentical (buf, reference)) touched = k;
            }

            std::cout << "A2: first block written by a zero wet mix: " << touched << "\n";
            expectEquals (touched, -1, "A wet mix of zero changed the voice");
        }

        // === B. Copy length mapping per division index at 120 BPM ===

        beginTest ("Division indices map to beat fractions of the 120 BPM beat, echo in the opposite channel");
        {
            // At 120 BPM one beat is 0.5 s, so the nine fractions are
            // {8000, 12000, 16000, 18000, 24000, 32000, 36000, 48000, 72000} samples. Two assertions per
            // index: the target the class computes (exact contract maths), and an impulse echo arriving at that
            // sample count in the OPPOSITE channel. The +/- 3 tolerance covers the fractional read-out; warm-up
            // is mandatory because reset() re-seeds the copy ramp at the comb floor, not at the target. The
            // triplet and dotted entries must land on their own fractions, so a mislabelled value cannot pass.
            const int impulseAt = 1536;   // after a 3-block silence warm-up
            bool mappingOk = true;

            for (int idx = 0; idx < 9; ++idx)
            {
                ovtdsp::DelayEffect d;
                dvxSetup (d, 120.0, idx, 1.0f, 0.5f);
                dvxWarmUp (d, 3);

                const int expected = dvxExpectedCopy (120.0, idx);
                expectEquals (static_cast<int> (d.getTargetCopySamples()), expected,
                              "Division index target does not match round((60/bpm)*fraction*sr)");

                std::vector<float> recL, recR;
                const int totalBlocks = static_cast<int> ((expected + 3000 + dvxBlock - 1) / dvxBlock);
                dvxRun (d, totalBlocks, true, dvxImpulse (impulseAt, 0), recL, recR);

                const int onset = dvxFirstAbove (recR, impulseAt + 2, 5e-4f);
                const int offset = onset >= 0 ? onset - impulseAt : -1;

                std::cout << "B: idx " << idx << " fraction " << kFractionByIndex[idx]
                          << " target " << d.getTargetCopySamples() << ", echo onset at offset " << offset
                          << ", delta " << (offset - expected) << "\n";

                if (onset < 0 || std::abs (offset - expected) > 3) mappingOk = false;

                // Zero-latency declaration: the dry impulse stays on ITS OWN index at unity, so this stage
                // never delayed the voice. The lines were all zeros when it was written, so nothing is added.
                expectEquals (recL[impulseAt], 1.0f, "The dry impulse moved off its own sample");
            }

            expect (mappingOk, "An echo did not arrive at the tempo-mapped copy length (+/-3 samples)");
        }

        // === C. Tempo sync: halving the BPM doubles the copy ===

        beginTest ("At 60 BPM the quarter-index copy is exactly double the 120 BPM copy");
        {
            ovtdsp::DelayEffect d;
            dvxSetup (d, 60.0, kDivQuarter, 1.0f, 0.5f);

            const int expected60 = dvxExpectedCopy (60.0, kDivQuarter);
            expectEquals (expected60, 2 * dvxExpectedCopy (120.0, kDivQuarter), "Halving the tempo did not double the quarter copy");

            const int impulseAt = 1536;
            dvxWarmUp (d, 4);   // room for the ramp to reach this longer target before measuring

            std::vector<float> recL, recR;
            dvxRun (d, static_cast<int> ((expected60 + 3500 + dvxBlock - 1) / dvxBlock), true,
                    dvxImpulse (impulseAt, 0), recL, recR);

            const int onset = dvxFirstAbove (recR, impulseAt + 2, 5e-4f);
            const int offset60 = onset >= 0 ? onset - impulseAt : -1;

            std::cout << "C: 60 BPM quarter copy target " << expected60 << ", echo onset at offset " << offset60 << "\n";

            expect (onset > 0 && std::abs (offset60 - expected60) <= 3,
                    "The 60 BPM echo does not arrive at double the 120 BPM length");
        }

        // === D. Feedback decay: successive echoes step down by the feedback knob ===

        beginTest ("Successive echoes step down by the feedback value; feedback 0 leaves a single echo");
        {
            const int idx = kDivQuarter, bpm = 120;
            const int D = dvxExpectedCopy (bpm, idx);
            const int impulseAt = 1536;

            // Echo k lands at impulseAt + k*D: odd echoes in R, even ones back in L (cross-feedback L->R, R->L).
            ovtdsp::DelayEffect d;
            dvxSetup (d, bpm, idx, 1.0f, 0.5f);
            dvxWarmUp (d, 3);

            std::vector<float> recL, recR;
            dvxRun (d, static_cast<int> ((3 * D + 4000 + dvxBlock - 1) / dvxBlock), true,
                    dvxImpulse (impulseAt, 0), recL, recR);

            const float e1 = dvxPeak (recR, impulseAt + D - kEchoHalfWindow, impulseAt + D + kEchoHalfWindow);
            const float e2 = dvxPeak (recL, impulseAt + 2 * D - kEchoHalfWindow, impulseAt + 2 * D + kEchoHalfWindow);
            const float e3 = dvxPeak (recR, impulseAt + 3 * D - kEchoHalfWindow, impulseAt + 3 * D + kEchoHalfWindow);

            const float r1 = e1 > 0.0f ? e2 / e1 : 0.0f;
            const float r2 = e2 > 0.0f ? e3 / e2 : 0.0f;

            std::cout << "D: feedback 0.5, echoes e1=" << e1 << " e2=" << e2 << " e3=" << e3
                      << ", ratios " << r1 << " / " << r2 << "\n";

            // Each pass carries the 150 Hz low-cut as well as the gain, so the tolerance is deliberately wide:
            // what is under test here is "the ratio follows the knob", not "the ratio equals it".
            expectGreaterThan (e1, 0.05f);
            expect (r1 > 0.25f && r1 < 0.85f, "First-to-second echo ratio does not follow feedback 0.5");
            expect (r2 > 0.25f && r2 < 0.85f, "Second-to-third echo ratio does not follow feedback 0.5");

            // Feedback 0: ONE echo only — everything past the first arrival has to be silent.
            ovtdsp::DelayEffect z;
            dvxSetup (z, bpm, idx, 1.0f, 0.0f);
            dvxWarmUp (z, 3);

            std::vector<float> zL, zR;
            dvxRun (z, static_cast<int> ((2 * D + 4000 + dvxBlock - 1) / dvxBlock), true,
                    dvxImpulse (impulseAt, 0), zL, zR);

            const float singleEcho = dvxPeak (zR, impulseAt + D - kEchoHalfWindow, impulseAt + D + kEchoHalfWindow);
            const float afterTail   = dvxRms (zL, impulseAt + D + kEchoHalfWindow, static_cast<int> (zL.size()));

            std::cout << "D0: feedback 0 single echo peak " << singleEcho
                      << ", L-side RMS past the echo window " << afterTail << "\n";

            expectGreaterThan (singleEcho, 0.1f);   // the one echo is there
            expectLessThan (afterTail, 1e-4f);       // and nothing followed it
        }

        // === E. Ping-pong image: echoes alternate sides, mirrored by the input side ===

        beginTest ("L input spreads echoes alternately L/R; R-only input mirrors the pattern");
        {
            const int idx = kDivQuarter, bpm = 120;
            const int D = dvxExpectedCopy (bpm, idx);
            const int impulseAt = 1536;

            std::vector<float> lRecL, lRecR, rRecL, rRecR;
            const int nBlocks = static_cast<int> ((3 * D + 4000 + dvxBlock - 1) / dvxBlock);

            ovtdsp::DelayEffect a;
            dvxSetup (a, bpm, idx, 0.9f, 0.4f);
            dvxWarmUp (a, 3);
            dvxRun (a, nBlocks, true, dvxImpulse (impulseAt, 0), lRecL, lRecR);

            ovtdsp::DelayEffect mirror;
            dvxSetup (mirror, bpm, idx, 0.9f, 0.4f);
            dvxWarmUp (mirror, 3);
            dvxRun (mirror, nBlocks, true, dvxImpulse (impulseAt, 1), rRecL, rRecR);

            // One window per echo: the side that owns it has to dominate by a clear factor.
            const float w1L = dvxRms (lRecL, impulseAt + D - kEchoHalfWindow, impulseAt + D + kEchoHalfWindow);
            const float w1R = dvxRms (lRecR, impulseAt + D - kEchoHalfWindow, impulseAt + D + kEchoHalfWindow);
            const float w2L = dvxRms (lRecL, impulseAt + 2 * D - kEchoHalfWindow, impulseAt + 2 * D + kEchoHalfWindow);
            const float w2R = dvxRms (lRecR, impulseAt + 2 * D - kEchoHalfWindow, impulseAt + 2 * D + kEchoHalfWindow);
            const float w3L = dvxRms (lRecL, impulseAt + 3 * D - kEchoHalfWindow, impulseAt + 3 * D + kEchoHalfWindow);
            const float w3R = dvxRms (lRecR, impulseAt + 3 * D - kEchoHalfWindow, impulseAt + 3 * D + kEchoHalfWindow);

            std::cout << "E: L input echo windows [L,R] = [" << w1L << ", " << w1R << "] ["
                      << w2L << ", " << w2R << "] [" << w3L << ", " << w3R << "]\n";

            expectGreaterThan (w1R / juce::jmax (1e-6f, w1L), 3.0f);   // first echo in R
            expectGreaterThan (w2L / juce::jmax (1e-6f, w2R), 3.0f);   // second back in L
            expectGreaterThan (w3R / juce::jmax (1e-6f, w3L), 3.0f);   // third in R again

            // The mirror: a right-only input has to dance the other way.
            const float m1L = dvxRms (rRecL, impulseAt + D - kEchoHalfWindow, impulseAt + D + kEchoHalfWindow);
            const float m1R = dvxRms (rRecR, impulseAt + D - kEchoHalfWindow, impulseAt + D + kEchoHalfWindow);
            const float m2L = dvxRms (rRecL, impulseAt + 2 * D - kEchoHalfWindow, impulseAt + 2 * D + kEchoHalfWindow);
            const float m2R = dvxRms (rRecR, impulseAt + 2 * D - kEchoHalfWindow, impulseAt + 2 * D + kEchoHalfWindow);

            std::cout << "E: R input mirror windows [L,R] = [" << m1L << ", " << m1R << "] ["
                      << m2L << ", " << m2R << "]\n";

            expectGreaterThan (m1L / juce::jmax (1e-6f, m1R), 3.0f);   // first echo in L
            expectGreaterThan (m2R / juce::jmax (1e-6f, m2L), 3.0f);   // second back in R
        }

        // === F. Ramp smoothness: a division change never jumps the read-out ===

        beginTest ("Division change idx4 -> idx7 at 120 BPM ramps continuously and settles inside ~35 ms");
        {
            const int oldCopy = dvxExpectedCopy (120.0, kDivQuarter);   // 24000
            const int newCopy = dvxExpectedCopy (120.0, kDivHalf);      // 48000
            const int changeBlock = 8;                        // the switch lands at sample 8 * dvxBlock
            const int impulseAt = changeBlock * dvxBlock + 16;  // the measuring impulse rides in the change block

            ovtdsp::DelayEffect d;
            dvxSetup (d, 120.0, kDivQuarter, 0.5f, 0.5f);
            dvxWarmUp (d, 3);   // copy settled at idx4 (1/4) with silent lines

            const int settleSamples = static_cast<int> (0.035 * dvxSr);   // ~1680: the stated settle window

            // Sustained tone is the material for this test: an impulse IS a jump by construction, so it cannot
            // prove smoothness. The measuring impulse rides inside the stream, and stays small enough to keep
            // legitimate deltas far below the click threshold.
            const dvxSource tone = dvxTone (220.0f, 0.4f);

            std::vector<float> fL, fR;
            dvxRun (d, static_cast<int> ((newCopy + settleSamples + 3500) / dvxBlock) + 1, true,
                    [&] (int n, float& l, float& r)
                    {
                        tone (n, l, r);
                        if (n == impulseAt) l += 1.0f;
                    },
                    fL, fR,
                    [&] (int k) { if (k == changeBlock) d.setDivision (kDivHalf); });

            const int changeStart = changeBlock * dvxBlock;
            // The window stops 2400 samples after the change: the ramp is 1200 samples, and the measuring
            // impulse (a legitimately instantaneous edge, and its echo even more so) arrives far later at
            // the new copy length. Measuring to the end of the recording would report the ECHO as a jump.
            const float maxJump = dvxMaxJump (fR, juce::jmax (0, changeStart - 1200), changeStart + 2400);

            std::cout << "F: biggest sample-to-sample jump across the division ramp: " << maxJump << "\n";

            // A 220 Hz tone at this level moves ~0.01 per sample, echoes included; reading a step of tens of ms
            // in one single sample spikes far past this bound, which is exactly what the ramp prevents.
            expectLessThan (maxJump, 0.06f);

            // Settle: with continuous material already echoing, the impulse shows up as a LOCAL peak near the new
            // copy length. Compare that window against a control window further out — energy must have arrived by
            // now and nothing louder may show up later.
            const int echoWindowStart = impulseAt + newCopy - kEchoHalfWindow;
            const float arrivalPeak   = dvxPeak (fR, echoWindowStart, echoWindowStart + 2 * kEchoHalfWindow);

            const int lateStart = impulseAt + newCopy + settleSamples;
            const float latePeak = dvxPeak (fR, lateStart, static_cast<int> (fR.size()));

            std::cout << "F: arrival peak in the new-copy window " << arrivalPeak
                      << ", biggest peak after the settle window " << latePeak << "\n";

            expectGreaterThan (arrivalPeak, 0.15f);                       // the change block echoed at all
            expectLessThan (latePeak, juce::jmax (0.12f, 1.6f * arrivalPeak));   // it settled inside ~35 ms
        }

        // === G. DC / sub-bass guard with high feedback ===

        beginTest ("Feedback 0.9 on a sustained low tone builds no DC and never explodes");
        {
            ovtdsp::DelayEffect d;
            dvxSetup (d, 120.0, kDivHalf, 0.7f, 0.9f);   // 1/2 copy + near-max feedback: several passes fit the 2 s run
            dvxWarmUp (d, 3);

            const int nBlocks = static_cast<int> ((2.0 * dvxSr) / dvxBlock);   // 2 s of sustained material
            std::vector<float> gL, gR;
            dvxRun (d, nBlocks, true, dvxLowTone (60.0f, 0.4f), gL, gR);

            const int window = 4096;
            float worstMean = 0.0f, firstPeak = 0.0f, lastPeak = 0.0f;
            bool dcOk = true, bounded = true;

            for (int start = window; start + window <= static_cast<int> (gL.size()); start += window)
            {
                const float m = std::abs (dvxMean (gL, start, start + window));
                worstMean = juce::jmax (worstMean, m);

                const float p = dvxPeak (gL, start, start + window);
                if (start == window) firstPeak = p;
                lastPeak = p;

                if (m > 0.03f)  dcOk = false;
                if (p > 1.6f)   bounded = false;
            }

            std::cout << "G: worst |mean| over 4096-sample windows " << worstMean
                      << ", first window peak " << firstPeak << ", last window peak " << lastPeak << "\n";

            expect (dcOk, "The feedback loop built DC on a sustained low tone");
            expect (bounded, "The feedback loop exploded past 1.6 at feedback 0.9");

            // The pole blocks DC by construction: an offsetting send cannot lock the loop. A constant input
            // (a pure DC impulse train) must decay to silence inside the loop.
            ovtdsp::DelayEffect dc;
            dvxSetup (dc, 120.0, kDivDottedEighth, 1.0f, 0.95f);
            dvxWarmUp (dc, 3);

            const dvxSource directCurrent = [] (int n, float& l, float& r) { l = n < 4 ? 1.0f : 0.0f; r = 0.0f; };
            std::vector<float> dcL, dcR;
            dvxRun (dc, static_cast<int> ((dvxExpectedCopy (120.0, kDivDottedEighth) + 6000 + dvxBlock - 1) / dvxBlock), true,
                    directCurrent, dcL, dcR);

            const float lateMean = std::abs (dvxMean (dcR, static_cast<int> (dcR.size()) - dvxBlock * 4,
                                                       static_cast<int> (dcR.size())));

            std::cout << "G2: mean in R at the end of a DC burst " << lateMean << "\n";
            expectLessThan (lateMean, 0.1f);   // the pole drains it instead of storing the offset
        }

        // === H. Clamping: extreme tempo / division stay inside the prepared buffer ===

        beginTest ("Extreme bpm x division clamps to [floor, prepared max] and never writes out of range");
        {
            const int floorSamples = dvxFloorSamples();   // 1920 at 48 kHz

            ovtdsp::DelayEffect fast;
            dvxSetup (fast, 400.0, kDivEighth, 0.7f, 0.5f);   // (60/400) * 0.5 = 75 ms -> 3600 samples

            expectEquals (static_cast<int> (fast.getTargetCopySamples()), dvxExpectedCopy (400.0, kDivEighth));
            expectGreaterThan (static_cast<int> (fast.getTargetCopySamples()), floorSamples - 1);   // stays above the comb floor

            const int impulseAt = 1536;
            dvxWarmUp (fast, 3);

            std::vector<float> fL, fR;
            dvxRun (fast, static_cast<int> ((dvxExpectedCopy (400.0, kDivEighth) + 2000 + dvxBlock - 1) / dvxBlock), true,
                    dvxImpulse (impulseAt, 0), fL, fR);

            const int onset = dvxFirstAbove (fR, impulseAt + floorSamples - 4, 5e-4f);
            const int offset = onset >= 0 ? onset - impulseAt : -1;

            std::cout << "H: bpm 400 idx1 target " << fast.getTargetCopySamples() << ", floor " << floorSamples
                      << ", echo onset at offset " << offset << "\n";

            expect (onset > 0 && std::abs (offset - dvxExpectedCopy (400.0, kDivEighth)) <= 3,
                    "The fastest legal division did not echo at the clamped copy length");

            // The longest request: bpm 20 x 3 beats (1/2D) = 9 s, exactly the prepared maximum, so it must
            // fit, stay finite and never write outside the buffer. Finiteness plus boundedness on every
            // recorded sample is what an out-of-range write breaks first — a wild read produces inf/NaN or a
            // huge spike before anything else.
            ovtdsp::DelayEffect slow;
            dvxSetup (slow, 20.0, kDivHalfDotted, 0.8f, 0.6f);

            const int capacity = slow.getLineCapacity();
            std::cout << "H: bpm 20 idx8 target " << slow.getTargetCopySamples()
                      << ", prepared capacity " << capacity << "\n";

            expect (slow.getTargetCopySamples() <= capacity, "The copy target escaped the prepared buffer");
            expectGreaterThan (capacity, dvxExpectedCopy (20.0, kDivHalfDotted) / 2);   // room was reserved for the worst case

            const int slowBlocks = static_cast<int> ((3.0 * dvxSr) / dvxBlock);   // a 3 s window of material
            std::vector<float> sL, sR;
            dvxRun (slow, slowBlocks, true, dvxImpulse (1536, 0, 0.8f), sL, sR);

            bool allFinite = true;
            float maxSeen = 0.0f;

            for (size_t i = 0; i < sL.size(); ++i)
            {
                if (! std::isfinite (sL[i]) || ! std::isfinite (sR[i]))
                {
                    allFinite = false;
                    break;
                }
                maxSeen = juce::jmax (maxSeen, std::abs (sL[i]), std::abs (sR[i]));
            }

            std::cout << "H: bpm 20 idx8 biggest |sample| over a 3 s run: " << maxSeen << "\n";

            expect (allFinite, "Extreme clamped settings produced non-finite samples");
            expectLessThan (maxSeen, 3.0f);

            // The dry path survives the extreme settings on its own sample at unity — which is also the proof that
            // nothing was written outside the buffer's own range: a stray write lands on this exact timeline.
            expectEquals (sL[1536], 0.8f, "Extreme clamped settings disturbed the dry sample");

            // Mono is not starved by the two-line design: one input channel still produces echo energy on BOTH sides.
            ovtdsp::DelayEffect mono;
            dvxSetup (mono, 120.0, kDivDottedEighth, 1.0f, 0.5f);
            dvxWarmUp (mono, 3);

            std::vector<float> mL, mR;
            // Crossing the input means the L side answers one copy LATER than the R side, so the run has
            // to cover two copies for both sides to show echo energy.
            const int monoBlocks = static_cast<int> ((2 * dvxExpectedCopy (120.0, kDivDottedEighth) + 4000 + dvxBlock - 1) / dvxBlock) + 1;
            dvxRun (mono, monoBlocks, true, dvxImpulse (1536, 0), mL, mR);

            const float monoEchoL = dvxPeak (mL, 1536 + 2, static_cast<int> (mL.size()));
            const float monoEchoR = dvxPeak (mR, 1536 + 2, static_cast<int> (mR.size()));

            std::cout << "H: mono input echo energy L " << monoEchoL << ", R " << monoEchoR << "\n";
            expectGreaterThan (monoEchoL, 0.1f);   // the L side carries echoes
            expectGreaterThan (monoEchoR, 0.1f);   // the cross-feed puts them in R too
        }

        // === Reset flushes both lines: a clean start, not a ringing buffer ===

        beginTest ("reset() empties both delay lines so the next phrase starts clean");
        {
            ovtdsp::DelayEffect d;
            dvxSetup (d, 120.0, kDivQuarter, 1.0f, 0.95f);   // near-max feedback: a tail that rings for seconds
            dvxWarmUp (d, 3);

            std::vector<float> xL, xR;
            // Three copies long: with the input crossed into the opposite line, the R channel's third
            // repeat (0.95^2 of the first) is what the tail window at the end of the run must hear.
            const int nBlocks = static_cast<int> ((3 * dvxExpectedCopy (120.0, kDivQuarter) + 4000 + dvxBlock - 1) / dvxBlock);
            dvxRun (d, nBlocks, true, dvxImpulse (1536, 0), xL, xR);

            const int D = dvxExpectedCopy (120.0, kDivQuarter);
            // An impulse answers as DISCRETE repeats, not a diffuse tail: the evidence that the line is
            // still ringing before the reset is the last repeat of the run (0.95^2 of the first), so the
            // window targets that arrival instead of the end of the buffer, which holds silence.
            const float ringing = dvxPeak (xR, 1536 + 3 * D - kEchoHalfWindow, 1536 + 3 * D + kEchoHalfWindow);
            d.reset();

            const dvxSource silent = [] (int, float& l, float& r) { l = 0.0f; r = 0.0f; };
            std::vector<float> sL, sR;
            dvxRun (d, 20, true, silent, sL, sR);

            const float afterResetPeakL = dvxPeak (sL, dvxRampSamples, static_cast<int> (sL.size()));
            const float afterResetPeakR = dvxPeak (sR, dvxRampSamples, static_cast<int> (sR.size()));

            std::cout << "R: tail left at the end of the run " << ringing
                      << ", peaks after reset over 10240 silent samples L " << afterResetPeakL
                      << " R " << afterResetPeakR << "\n";

            expectGreaterThan (ringing, 0.05f);   // the tail was real before reset
            expectLessThan (afterResetPeakL, 1e-4f);
            expectLessThan (afterResetPeakR, 1e-4f);
        }
    }
};

static DelayEffectTest delayEffectTest;
