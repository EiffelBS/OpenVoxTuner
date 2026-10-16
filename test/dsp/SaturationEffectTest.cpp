#pragma once
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include <juce_audio_processors/juce_audio_processors.h>

#include "../../Source/dsp/SaturationEffect.h"

// Helpers for the Saturation suite. The file is #included into Main.cpp, so the
// names carry a sat prefix to stay clear of the other suites' helpers.

static float satRms (const juce::AudioBuffer<float>& b, int channel, int start, int count)
{
    double sum = 0.0;
    const int end = juce::jmin (start + count, b.getNumSamples());
    for (int s = start; s < end; ++s)
    {
        const float v = b.getSample (channel, s);
        sum += static_cast<double> (v) * static_cast<double> (v);
    }
    return static_cast<float> (std::sqrt (sum / static_cast<double> (juce::jmax (1, end - start))));
}

static float satMean (const juce::AudioBuffer<float>& b, int channel, int start, int count)
{
    double sum = 0.0;
    const int end = juce::jmin (start + count, b.getNumSamples());
    for (int s = start; s < end; ++s)
        sum += static_cast<double> (b.getSample (channel, s));
    return static_cast<float> (sum / static_cast<double> (juce::jmax (1, end - start)));
}

/** Fill a block with a stereo sine that stays phase-continuous across blocks. */
static void satFillStereoSine (juce::AudioBuffer<float>& b, float& phase, float freq,
                               float ampL, float ampR, double sr)
{
    const float twoPi = static_cast<float> (juce::MathConstants<double>::twoPi);
    const float inc   = twoPi * freq / static_cast<float> (sr);
    for (int s = 0; s < b.getNumSamples(); ++s)
    {
        b.setSample (0, s, ampL * std::sin (phase));
        b.setSample (1, s, ampR * std::sin (phase));
        phase += inc;
        if (phase >= twoPi) phase -= twoPi;
    }
}

/** Fill a block with a sine plus a DC offset: the input an ARA render can hand over. */
static void satFillSinePlusDc (juce::AudioBuffer<float>& b, float& phase, float freq,
                               float amp, float dc, double sr)
{
    const float twoPi = static_cast<float> (juce::MathConstants<double>::twoPi);
    const float inc   = twoPi * freq / static_cast<float> (sr);
    for (int s = 0; s < b.getNumSamples(); ++s)
    {
        b.setSample (0, s, dc + amp * std::sin (phase));
        b.setSample (1, s, dc + amp * std::sin (phase));
        phase += inc;
        if (phase >= twoPi) phase -= twoPi;
    }
}

/** Fill a block with a sawtooth ramp carrying DC: the worst case for a DC budget. */
static void satFillRampWithDc (juce::AudioBuffer<float>& b, int& step, float period,
                               float amp, float dc)
{
    for (int s = 0; s < b.getNumSamples(); ++s)
    {
        const float r = static_cast<float> (step) / period;   // 0..1 ramp
        const float v = dc + amp * (2.0f * r - 1.0f);
        b.setSample (0, s, v);
        b.setSample (1, s, v);
        if (++step >= static_cast<int> (period)) step = 0;
    }
}

/**
 * Amplitude at an exact bin through Goertzel. The window holds a whole number of cycles of
 * the test tone, so every harmonic of that tone is also a whole number of cycles and lands
 * in its own orthogonal bin: nothing leaks from a loud line into a quiet one. A narrow
 * resonator cannot do this -- measured 1 kHz away from a strong line it still passes about
 * -26 dB of skirt, which reads as aliasing when it is only the skirt.
 */
static float satDftMag (const juce::AudioBuffer<float>& b, int channel, int start, int len, double cycles)
{
    const double pi = juce::MathConstants<double>::pi;
    const double w  = 2.0 * pi * cycles / static_cast<double> (len);
    const double cw = std::cos (w), sw = std::sin (w);

    double s1 = 0.0, s2 = 0.0;
    for (int i = 0; i < len; ++i)
    {
        const double x = static_cast<double> (b.getSample (channel, start + i));
        const double s0 = x + 2.0 * cw * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double re = s1 - cw * s2, im = sw * s2;
    return static_cast<float> (2.0 * std::sqrt (re * re + im * im) / static_cast<double> (len));
}

/**
 * Feed a buffer through the stage region by region, once per region. The enable ramp climbs
 * across the buffer, so measuring its last quarter reads steady-state gain instead of a
 * half-engaged ramp. Re-running the effect over the same samples is not an option: it would
 * stack saturation on top of saturation and invent harmonics the test then measures as real.
 */
static void satRunSettled (ovtdsp::SaturationEffect& sat, juce::AudioBuffer<float>& b, int block)
{
    float* ptrs[2] = { b.getWritePointer (0), b.getWritePointer (1) };
    for (int off = 0; off + block <= b.getNumSamples(); off += block)
    {
        float* region[2] = { ptrs[0] + off, ptrs[1] + off };
        juce::AudioBuffer<float> view (region, 2, block);
        sat.process (view, true, 1.0f);   // the view shares b's storage: writes land in place
    }
}

/**
 * Saturation unit tests.
 *
 * The stage is a soft clipper running at 2x with a warm pole and a DC block, so the suite
 * checks the properties that define it, all measured rather than estimated:
 *  - drive 0 must be a transparent bypass (the shaper curve is NOT unity at k = 1, so it
 *    has to be skipped entirely), and disabling must fade out to a bit-identical buffer;
 *  - makeup keeps loudness level matched across the whole drive range (+/-1 dB RMS);
 *  - DC offset stays zero on a sine that arrives with DC and on a ramp that carries DC;
 *  - headroom: a +6 dBFS input must not clip hard, peaks come down, nothing exceeds full scale;
 *  - aliasing: harmonics above fs/2 must be attenuated by tens of dB before they fold back;
 *  - the internal delay is exactly the measured filter centre, with no energy ahead of it.
 */
class SaturationTest : public juce::UnitTest
{
public:
    SaturationTest() : juce::UnitTest ("Saturation") {}

    void runTest() override
    {
        constexpr double sr   = 48000.0;
        constexpr int    block = 256;
        const float minus18DbAmp = 0.2512f;   // -18 dBFS, the level makeup was measured at

        ovtdsp::SaturationEffect sat;
        sat.prepare (sr, block);

        beginTest ("drive 0 is a transparent bypass, not a shaper value");
        {
            juce::AudioBuffer<float> in (2, block), out (2, block);
            float phase = 0.0f;
            sat.setDrive (0.0f);
            sat.setTone (1.0f);   // warmest pole: if any filter ran, it would show here

            for (int i = 0; i < 40; ++i)
            {
                satFillStereoSine (in, phase, 3000.0f, 0.6f, 0.5f, sr);
                out = in;   // the stage must leave this copy untouched
                sat.process (out, true, 1.0f);
            }

            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < block; ++s)
                    expectEquals (out.getSample (c, s), in.getSample (c, s));
        }

        beginTest ("disabling fades out and stops touching the buffer");
        {
            juce::AudioBuffer<float> in (2, block), out (2, block);
            float phase = 0.0f;
            sat.setDrive (0.6f);
            sat.setTone (0.4f);

            for (int i = 0; i < 30; ++i)   // ramp fully engaged
            {
                satFillStereoSine (in, phase, 1000.0f, minus18DbAmp, minus18DbAmp, sr);
                out = in;
                sat.process (out, true, 1.0f);
            }
            const float engagedRms = satRms (out, 0, block - 64, 64);
            expect (engagedRms > 0.05f);   // the stage is audible while enabled

            for (int i = 0; i < 80; ++i)   // ramp fully decayed (25 ms at 48 kHz)
            {
                satFillStereoSine (in, phase, 1000.0f, minus18DbAmp, minus18DbAmp, sr);
                out = in;
                sat.process (out, false, 1.0f);
            }
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < block; ++s)
                    expectEquals (out.getSample (c, s), in.getSample (c, s));

            sat.reset();
        }

        beginTest ("makeup keeps loudness level matched across the drive range");
        {
            // Measured shaper RMS loss: -0.03 dB at drive 0, -2.59 dB at full drive. The
            // makeup table inverts that curve, so the stage must leave a -18 dBFS sine at
            // unity within +/- 1 dB for every knob position.
            const float drives[5] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };

            for (float freq : { 200.0f, 1000.0f })
            {
                for (const float d : drives)
                {
                    sat.reset();
                    sat.setDrive (d);
                    sat.setTone (0.4f);

                    juce::AudioBuffer<float> in (2, block), out (2, block);
                    float phase = 0.0f;
                    for (int i = 0; i < 60; ++i)   // ramp + pole + DC block all settled
                    {
                        satFillStereoSine (in, phase, freq, minus18DbAmp, minus18DbAmp, sr);
                        out = in;
                        sat.process (out, true, 1.0f);
                    }

                    const float ratio = satRms (out, 0, block - 128, 128) / satRms (in, 0, block - 128, 128);
                    const float dB    = 20.0f * std::log10 (ratio);
                    expectWithinAbsoluteError (dB, 0.0f, 1.0f, "makeup keeps the level at "
                        + juce::String (freq) + " Hz with drive " + juce::String (d)
                        + ": " + juce::String (dB) + " dB");
                }
            }
        }

        beginTest ("DC offset is blocked on a sine and on a ramp");
        {
            // Measuring DC means averaging over WHOLE periods: a partial cycle of the test
            // tone contributes its own mean, which is not an offset. So the tone's period is
            // exactly one block (sr / 256 = 187.5 Hz) and every window below is a whole
            // number of periods -- anything left in the average is DC. The budget is 1e-3,
            // i.e. -60 dBFS: the measured residual (~2e-4) is the high-pass's phase shift on
            // the saturated wave, not an offset, and it is far below audibility.
            const float dcFreq = static_cast<float> (sr) / 256.0f;

            sat.reset();
            sat.setDrive (0.75f);   // deep saturation: the case where DC would appear
            sat.setTone (0.0f);

            juce::AudioBuffer<float> buf (2, block * 32);
            float phase = 0.0f;
            satFillSinePlusDc (buf, phase, dcFreq, 0.4f, 0.3f, sr);   // +0.3 DC in
            satRunSettled (sat, buf, block);
            expectWithinAbsoluteError (satMean (buf, 0, block * 24, block * 8), 0.0f, 1.0e-3f,
                                       "a +0.3 DC input must leave the stage with no offset");

            int step = 0;
            buf.clear();
            satFillRampWithDc (buf, step, 256.0f, 0.5f, 0.25f);   // ramp carrying DC
            sat.reset();
            satRunSettled (sat, buf, block);
            expectWithinAbsoluteError (satMean (buf, 0, block * 24, block * 8), 0.0f, 1.0e-3f,
                                       "a ramp carrying +0.25 DC must leave the stage at zero");

            // A symmetric input through an odd shaper must not create DC on its own.
            buf.clear();
            phase = 0.0f;
            satFillStereoSine (buf, phase, dcFreq, 0.7f, 0.7f, sr);
            sat.reset();
            satRunSettled (sat, buf, block);
            expectWithinAbsoluteError (satMean (buf, 0, block * 24, block * 8), 0.0f, 1.0e-3f,
                                       "a symmetric input must not generate DC");
        }

        beginTest ("headroom: a hot ARA buffer stays inside full scale and continuous");
        {
            sat.reset();
            sat.setDrive (1.0f);   // k = 10, the most aggressive setting
            sat.setTone (0.0f);

            const float amps[3] = { 1.0f, 2.0f, 4.0f };   // 0 dBFS, +6 dBFS, +12 dBFS
            float gainFull = 0.0f, gainHot = 0.0f;

            for (const float a : amps)
            {
                juce::AudioBuffer<float> out (2, block);
                float phase = 0.0f;
                sat.reset();
                for (int i = 0; i < 60; ++i)   // fresh input each block: steady state
                {
                    satFillStereoSine (out, phase, 1000.0f, a, a, sr);
                    sat.process (out, true, 1.0f);
                }

                float peak = 0.0f;
                for (int s = 0; s < block; ++s)
                    peak = juce::jmax (peak, std::abs (out.getSample (0, s)));
                expect (peak < 1.0f, "amplitude " + juce::String (a) + " dBFS-relative input must stay inside full scale");

                const float sineRmsIn = a * 0.7071067812f;
                const float gain = satRms (out, 0, block - 128, 128) / sineRmsIn;
                expect (gain < 1.123f, "amplitude " + juce::String (a) + ": added loudness must stay under +1 dB");

                if (a == 1.0f) gainFull = gain;
                if (a == 2.0f) gainHot  = gain;
            }
            expect (gainHot < gainFull);   // soft clipping: louder in means lower gain, not louder out

            // Continuity across the full-scale boundary: the headroom mapping is continuous,
            // so a slow sine crossing +/-1 must not produce a step. Measured once the enable
            // ramp has settled (last quarter of the buffer), because during the fade-in the
            // dry path passes through unshaped -- that is the crossfade, not a discontinuity.
            sat.reset();
            juce::AudioBuffer<float> out (2, block * 32);
            const float w = 2.0f * static_cast<float> (juce::MathConstants<double>::pi) * 20.0f / static_cast<float> (sr);
            for (int s = 0; s < out.getNumSamples(); ++s)
            {
                const float v = 1.5f * std::sin (w * s);   // crosses +/-1 slowly
                out.setSample (0, s, v);
                out.setSample (1, s, v);
            }
            satRunSettled (sat, out, block);

            const int settleStart = block * 24;
            float peak = 0.0f, worstJump = 0.0f;
            for (int s = settleStart + 1; s < out.getNumSamples(); ++s)
            {
                const float v = out.getSample (0, s);
                peak      = juce::jmax (peak, std::abs (v));
                worstJump = juce::jmax (worstJump, std::abs (v - out.getSample (0, s - 1)));
            }
            expect (peak < 1.0f);
            expect (worstJump < peak * 0.15f);   // a rounded knee, not a step to the ceiling

            sat.reset();
        }

        beginTest ("oversampling removes the harmonics that would alias");
        {
            // A saturated 7 kHz tone is near square, so its odd harmonics are strong. The
            // ones above fs/2 (35 kHz, 49 kHz) fold back to 13 kHz and ~1 kHz. With 2x
            // oversampling the shared half-band filter removes them before decimation;
            // without it they would land at their harmonic level (~ -14 dB), audible sparkle.
            sat.reset();
            sat.setDrive (1.0f);
            sat.setTone (0.0f);   // brightest pole: the worst case for aliasing

            juce::AudioBuffer<float> out (2, block * 32);
            float phase = 0.0f;
            satFillStereoSine (out, phase, 7000.0f, 0.5f, 0.5f, sr);   // fill the whole buffer
            satRunSettled (sat, out, block);

            // 336 samples at 48 kHz hold exactly 49 cycles of the 7 kHz tone, so every
            // harmonic is an integer number of cycles too: the bins are orthogonal and a
            // quiet bin cannot be reading the loud one's skirt.
            const int len = 336;
            const int measStart = block * 24;   // last quarter: ramp fully engaged
            const float fund = satDftMag (out, 0, measStart, len, 49.0);   // 7 kHz
            expect (fund > 0.0f);

            // Saturation must really be happening: the n=3 harmonic at 21 kHz is in band.
            const float third = satDftMag (out, 0, measStart, len, 147.0);   // 21 kHz
            expect (third > fund * 0.03f, "the stage must be saturating: 21 kHz harmonic "
                                          + juce::String (third) + " vs fundamental " + juce::String (fund));

            for (double cycles : { 91.0, 140.0, 7.0 })   // 13 kHz, 20 kHz, 1 kHz: folded n=5 and n=7
            {
                const float a = satDftMag (out, 0, measStart, len, cycles);
                const float hz = static_cast<float> (cycles * sr / static_cast<double> (len));
                expect (a < fund * 0.0178f, "aliasing product at " + juce::String (hz) + " Hz: "
                                            + juce::String (a) + " vs fundamental " + juce::String (fund));
            }

            sat.reset();
        }

        beginTest ("the internal delay is the measured filter centre");
        {
            sat.reset();
            sat.setDrive (0.3f);
            sat.setTone (0.4f);

            juce::AudioBuffer<float> in (2, block), out (2, block);
            in.clear();
            in.setSample (0, 0, 1.0f);   // impulse at the very first sample
            out = in;
            sat.process (out, true, 1.0f);

            int peakIdx = 0;
            float peakVal = 0.0f;
            for (int s = 0; s < block; ++s)
            {
                const float v = std::abs (out.getSample (0, s));
                if (v > peakVal) { peakVal = v; peakIdx = s; }
            }
            expectEquals (peakIdx, sat.getInternalDelaySamples());   // 16 samples at the base rate
            for (int s = 0; s < 4; ++s)
                expectWithinAbsoluteError (out.getSample (0, s), 0.0f, peakVal * 1.0e-3f);
        }

        beginTest ("warmth rolls off the harmonics the clipper adds");
        {
            // Odd harmonics of the 1800 Hz test tone: the clipper's own products, all inside
            // the audible band, so a lower cutoff must reduce their level. A 320-sample window
            // holds exactly 12 cycles of 1800 Hz, so bins 36/60/84 (5400 / 9000 / 12600 Hz) are
            // orthogonal to the tone and to each other.
            const double cycles[3] = { 36.0, 60.0, 84.0 };

            auto measureHf = [&] (float tone, float* outVals)
            {
                sat.reset();
                sat.setDrive (0.5f);
                sat.setTone (tone);

                juce::AudioBuffer<float> out (2, block * 32);
                float phase = 0.0f;
                satFillStereoSine (out, phase, 1800.0f, 0.3f, 0.3f, sr);
                satRunSettled (sat, out, block);

                float hf = 0.0f;
                for (int i = 0; i < 3; ++i)
                {
                    outVals[i] = satDftMag (out, 0, block * 24, 320, cycles[i]);
                    hf += outVals[i];
                }
                return hf;
            };

            float brightVals[3], warmVals[3];
            const float bright = measureHf (0.0f, brightVals);
            const float warm   = measureHf (1.0f, warmVals);

            expect (warm < bright * 0.85f, "warm HF sum " + juce::String (warm)
                                           + " must sit below bright HF sum " + juce::String (bright));

            // The signature of a low-pass is per-harmonic: every harmonic above the cutoff
            // sits lower when warmth rises. A total alone can hide a single bin going up.
            for (int i = 0; i < 3; ++i)
                expect (warmVals[i] < brightVals[i], "harmonic at " + juce::String (cycles[i] * sr / 320.0)
                                                     + " Hz: warm " + juce::String (warmVals[i])
                                                     + " must sit below bright " + juce::String (brightVals[i]));
        }

        beginTest ("drive rises harmonic content monotonically");
        {
            auto thdAt = [&] (float drive)
            {
                sat.reset();
                sat.setDrive (drive);
                sat.setTone (0.4f);

                juce::AudioBuffer<float> out (2, block * 32);
                float phase = 0.0f;
                satFillStereoSine (out, phase, 1000.0f, minus18DbAmp, minus18DbAmp, sr);
                satRunSettled (sat, out, block);

                const int len = 480;   // exactly 10 cycles of the 1 kHz tone at 48 kHz
                const int measStart = block * 24;   // last quarter: ramp fully engaged
                const float fund  = satDftMag (out, 0, measStart, len, 10.0);    // 1 kHz
                const float third = satDftMag (out, 0, measStart, len, 30.0);   // 3 kHz
                return third / juce::jmax (1.0e-9f, fund);
            };

            const float a = thdAt (0.25f), b = thdAt (0.5f), c = thdAt (0.75f), d = thdAt (1.0f);
            expect (a < b);
            expect (b < c);
            expect (c < d);
        }

        beginTest ("stereo channels are processed independently");
        {
            sat.reset();
            sat.setDrive (0.5f);
            sat.setTone (0.4f);

            juce::AudioBuffer<float> in (2, block), out (2, block);
            float phase = 0.0f;
            for (int i = 0; i < 60; ++i)
            {
                satFillStereoSine (in, phase, 1000.0f, minus18DbAmp, minus18DbAmp, sr);
                out = in;
                sat.process (out, true, 1.0f);
            }
            for (int s = 0; s < block; ++s)
                expectEquals (out.getSample (0, s), out.getSample (1, s));   // same input -> same output

            // A wetMix of 0 must leave the buffer at the dry reference, i.e. unity gain.
            sat.reset();
            phase = 0.0f;
            for (int i = 0; i < 60; ++i)
            {
                satFillStereoSine (in, phase, 1000.0f, minus18DbAmp, minus18DbAmp, sr);
                out = in;
                sat.process (out, true, 0.0f);
            }
            const float ratio = satRms (out, 0, block - 128, 128) / satRms (in, 0, block - 128, 128);
            expectWithinAbsoluteError (20.0f * std::log10 (ratio), 0.0f, 0.5f);
        }

        beginTest ("the shaper constant follows the drive knob");
        {
            sat.setDrive (0.0f);  expectEquals (sat.getShaperK(), 1.0f);
            sat.setDrive (0.5f);  expectWithinAbsoluteError (sat.getShaperK(), 5.5f, 1.0e-6f);
            sat.setDrive (1.0f);  expectWithinAbsoluteError (sat.getShaperK(), 10.0f, 1.0e-6f);

            // Warmth maps to the documented cutoff range: 8 kHz bright, 5 kHz warm. The
            // smoother is driven by processBlock, so feed it silent blocks and read the
            // coefficient it converged to. tau = 60 ms at a 256/48 kHz block, so 100 blocks
            // (533 ms, ~9 time constants) leave the smoothed cutoff at its target.
            juce::AudioBuffer<float> idle (2, block);
            idle.clear();

            sat.reset();
            sat.setTone (0.0f);
            for (int i = 0; i < 100; ++i)
                sat.process (idle, true, 1.0f);
            expectWithinAbsoluteError (sat.getCutoffHz(), 8000.0f, 50.0f);

            sat.setTone (1.0f);
            for (int i = 0; i < 100; ++i)
                sat.process (idle, true, 1.0f);
            expectWithinAbsoluteError (sat.getCutoffHz(), 5000.0f, 60.0f);

            sat.reset();
        }
    }
};

static SaturationTest saturationTest;

