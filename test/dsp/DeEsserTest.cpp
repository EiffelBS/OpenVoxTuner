#pragma once
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include <juce_audio_processors/juce_audio_processors.h>

#include "../../Source/dsp/DeEsser.h"

// Helpers for the De-Esser suite. The file is #included into Main.cpp, so the
// names carry a deEsser prefix to stay clear of the other suites' helpers.

static float deEsserRms (const juce::AudioBuffer<float>& b, int channel)
{
    double sum = 0.0;
    for (int s = 0; s < b.getNumSamples(); ++s)
    {
        const float v = b.getSample (channel, s);
        sum += static_cast<double> (v) * static_cast<double> (v);
    }
    return static_cast<float> (std::sqrt (sum / static_cast<double> (b.getNumSamples())));
}

/** Fill a block with a stereo sine that stays phase-continuous across blocks. */
static void deEsserFillStereoSine (juce::AudioBuffer<float>& b, float& phase,
                                   float freq, float ampL, float ampR, double sr)
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

/**
 * De-Esser unit tests.
 *
 * The module is a broadband gain stage driven by an RMS detector on the sibilance
 * band only, so the suite checks the four properties that define it:
 *  - band isolation: energy outside the band must not trigger reduction;
 *  - reduction depth: a hot band tone ducks to exactly Max reduction;
 *  - hysteresis + stereo link: the gate stays shut between close and open levels,
 *    and the loudest channel drives the gain on BOTH channels;
 *  - click-free toggling: gain is ramped per sample, never stepped.
 */
class DeEsserTest : public juce::UnitTest
{
public:
    DeEsserTest() : juce::UnitTest ("DeEsser") {}

    void runTest() override
    {
        constexpr double sr = 44100.0;
        constexpr int block = 512;
        const float floorGain = std::pow (10.0f, -12.0f / 20.0f); // Max reduction = 12 dB

        // Band amplitudes for fc = 7 kHz (band edges 4.9 / 9.1 kHz, 12 dB/oct skirts).
        // The cascade passes a 7 kHz tone at ~0.64, so:
        const float ampHot = 0.8f;  // band RMS ~0.52, above the +6 dB open level
        const float ampMid = 0.4f;  // band RMS ~0.26, inside the hysteresis window
        const float ampLow = 0.3f;  // 3 kHz tone: band RMS ~0.11, below the close level

        ovtdsp::DeEsser de;
        de.prepare (sr, block);
        de.reset();
        de.setThresholdDb (-25.0f);
        de.setMaxReductionDb (12.0f);
        de.setFrequencyHz (7000.0f);

        juce::AudioBuffer<float> buf (2, block);

        beginTest ("Bypassed leaves the buffer bit-identical");
        // reset() starts fully bypassed, so a disabled de-esser must not touch the audio.
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < block; ++s)
                buf.setSample (c, s, static_cast<float> (s) * 0.001f - 0.25f);
        const juce::AudioBuffer<float> copy (buf);
        de.process (buf, false, 1.0f);
        for (int c = 0; c < 2; ++c)
            for (int s = 0; s < block; ++s)
                expect (buf.getSample (c, s) == copy.getSample (c, s),
                        "bypassed buffer must be bit-identical");

        // Warm-up: silent blocks settle the bypass ramp (25 ms) and the detector.
        for (int i = 0; i < 10; ++i)
        {
            buf.clear();
            de.process (buf, true, 1.0f);
        }

        beginTest ("Band isolation: a 3 kHz tone is not de-essed");
        float phase = 0.0f;
        for (int i = 0; i < 20; ++i)
        {
            deEsserFillStereoSine (buf, phase, 3000.0f, ampLow, ampLow, sr);
            de.process (buf, true, 1.0f);
        }
        expect (! de.isEngaged(), "energy outside the band must not engage the detector");
        expectWithinAbsoluteError (deEsserRms (buf, 0), ampLow * 0.7071f, 0.02f,
                                   "3 kHz tone must pass through at full level");

        beginTest ("Reduction depth: a hot band tone ducks to Max reduction");
        for (int i = 0; i < 40; ++i)
        {
            deEsserFillStereoSine (buf, phase, 7000.0f, ampHot, ampHot, sr);
            de.process (buf, true, 1.0f);
        }
        expect (de.isEngaged(), "hot band energy must engage the detector");
        expectWithinAbsoluteError (de.getCurrentGain(), floorGain, 0.02f,
                                   "gain must reach the reduction floor");
        expectWithinAbsoluteError (deEsserRms (buf, 0), ampHot * 0.7071f * floorGain, 0.015f,
                                   "band tone must duck to the 12 dB floor");

        beginTest ("Hysteresis: the gate stays shut between close and open levels");
        for (int i = 0; i < 30; ++i)
        {
            deEsserFillStereoSine (buf, phase, 7000.0f, ampMid, ampMid, sr);
            de.process (buf, true, 1.0f);
        }
        expect (de.isEngaged(), "a level between close and open must keep the gate shut");
        expectWithinAbsoluteError (deEsserRms (buf, 0), ampMid * 0.7071f * floorGain, 0.015f,
                                   "hysteresis must hold the reduction, not release it");

        beginTest ("Stereo link: the loudest channel ducks both channels equally");
        for (int i = 0; i < 40; ++i)
        {
            deEsserFillStereoSine (buf, phase, 7000.0f, ampMid, ampHot, sr);
            de.process (buf, true, 1.0f);
        }
        const float duckL = deEsserRms (buf, 0) / (ampMid * 0.7071f);
        const float duckR = deEsserRms (buf, 1) / (ampHot  * 0.7071f);
        expectWithinAbsoluteError (duckL, floorGain, 0.02f, "left channel must be ducked to the floor");
        expectWithinAbsoluteError (duckR, floorGain, 0.02f, "right channel must be ducked to the floor");
        expectWithinAbsoluteError (duckL, duckR, 0.01f, "both channels must share one gain reduction");

        beginTest ("Release: silence lets the gate open again");
        for (int i = 0; i < 60; ++i)
        {
            buf.clear();
            de.process (buf, true, 1.0f);
        }
        expect (! de.isEngaged(), "silence must release the gate");
        expectWithinAbsoluteError (de.getCurrentGain(), 1.0f, 0.02f, "gain must return to unity");

        beginTest ("Toggling the power is click-free");
        // The applied gain must stay continuous. A hard gain toggle steps by
        // 1 - reductionFloor = 0.75 in ONE sample; every ramp here (5 ms attack,
        // 25 ms bypass) moves at most ~0.005 per sample. The gain is measured
        // directly as out/in against the raw sine, because a 7 kHz sine at 0.8 has
        // a natural sample-to-sample delta of ~0.76 (2*A*sin(pi*f/sr)) all by
        // itself: signal slope is not a click, and only the gain continuity tells
        // the two apart.
        std::vector<float> ref (block); // the raw sine, captured before processing
        float maxGainStep = 0.0f, maxSampleDelta = 0.0f;
        float prevOut = 0.0f, prevGain = 1.0f;
        bool havePrevOut = false, havePrevGain = false;
        for (int i = 0; i < 12; ++i)
        {
            deEsserFillStereoSine (buf, phase, 7000.0f, ampHot, ampHot, sr);
            for (int s = 0; s < block; ++s) ref[s] = buf.getSample (0, s);
            de.process (buf, i < 6, 1.0f); // on for 6 blocks, off for the next 6
            for (int s = 0; s < block; ++s)
            {
                const float in = ref[s], out = buf.getSample (0, s);
                if (havePrevOut)
                {
                    const float d = std::abs (out - prevOut);
                    if (d > maxSampleDelta) maxSampleDelta = d;
                }
                prevOut = out; havePrevOut = true;

                if (std::abs (in) > 0.05f) // zero crossings: out/in is meaningless there
                {
                    const float g = out / in;
                    if (havePrevGain)
                    {
                        const float dg = std::abs (g - prevGain);
                        if (dg > maxGainStep) maxGainStep = dg;
                    }
                    prevGain = g; havePrevGain = true;
                }
            }
        }
        expect (maxGainStep < 0.01f, "applied gain must ramp per sample, never step");
        expect (maxSampleDelta < 0.80f, "output must stay continuous across the toggle");

        beginTest ("Disabled returns the tone to full level");
        for (int i = 0; i < 20; ++i)
        {
            deEsserFillStereoSine (buf, phase, 7000.0f, ampHot, ampHot, sr);
            de.process (buf, false, 1.0f);
        }
        expectWithinAbsoluteError (deEsserRms (buf, 0), ampHot * 0.7071f, 0.02f,
                                   "disabled de-esser must restore the full level");
    }
};

static DeEsserTest deEsserTest;
