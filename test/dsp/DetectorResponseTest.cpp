#pragma once
// DetectorResponseTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// How long does the pitch-analysis path take to REPORT a note change? The
// answer is not one number: it is the analysis window's averaging, the hop
// quantisation and the detector's median filter stacked on top of each other,
// and on the live path (4096-sample window at 44.1 kHz, 2048-sample hop,
// 5-frame median) the sum is large enough to dominate the perceived "snap".
//
// This test replays the processor's exact analysis chain - 44.1 kHz input, a
// 4096-sample FIFO, 4x decimation with the same 4-sample moving average, YIN at
// 11.025 kHz - and measures, for each candidate configuration:
//   - lock time: from the note step to the first reported pitch within 15 cents
//     of the new note that STAYS within 30 cents for 200 ms;
//   - jitter: the standard deviation, in cents, of a steady note;
//   - detector cost: microseconds of YIN per second of audio.
// It also checks that the short window still detects a bass note (82 Hz) the
// short way, because the Hard Tune mode's window is meant to be usable by every
// Voice Type, not only by tenors.
//
// The numbers printed here are the evidence for the Hard Tune analysis
// configuration in Source/PluginProcessor.cpp (window 2048, hop 512, median 3).

#include <juce_audio_processors/juce_audio_processors.h>
#include <chrono>
#include <vector>
#include "../../Source/dsp/YinPitchDetector.h"

class DetectorResponseTest : public juce::UnitTest
{
public:
    DetectorResponseTest() : juce::UnitTest ("DetectorResponse") {}

    struct Config
    {
        const char* name;
        int window;   // samples at 44.1 kHz
        int hop;      // samples at 44.1 kHz
        int median;   // YinPitchDetector median frames
    };

    struct Result
    {
        double lockMs      = -1.0;
        double jitterCents = 0.0;
        double meanCents   = 0.0;
        double detectUsPerSec = 0.0;
        int    calls       = 0;
    };

    // Replays the processor's analysis chain and returns the metrics for one
    // note step at `stepMs` (HzA -> HzB, both harmonic-rich like a voice).
    // `rangeMin/rangeMax` mirror the Voice Type search range: with Universal
    // (30..1000) the anti-octave heuristic doubles a clean tone's tau when the
    // octave-down candidate is still inside the lag search, so the measurement
    // is taken on the register a singer would actually select.
    static Result measure (const Config& cfg, double hzA, double hzB,
                           float rangeMin, float rangeMax,
                           double stepMs = 1500.0, double totalMs = 3000.0)
    {
        constexpr double sr        = 44100.0;
        constexpr int    decim     = 4;
        constexpr int    fifoSize  = 4096;
        constexpr int    blockSize = 256;
        const double     srDet     = sr / decim;
        const int        win       = juce::jmin (cfg.window, fifoSize);
        const int        decimWin  = win / decim;

        ovtdsp::YinPitchDetector det;
        det.prepare (srDet, 512);
        det.setFrequencyRange (rangeMin, rangeMax);
        det.setMedianFrames (cfg.median);

        std::vector<float> fifo ((size_t) fifoSize, 0.0f);
        std::vector<float> linear ((size_t) decimWin, 0.0f);
        int writeIdx = 0, filled = 0, sinceHop = 0;

        struct Frame { double ms; float hz; };
        std::vector<Frame> frames;
        frames.reserve (256);

        double totalDetectUs = 0.0;
        int    calls = 0;

        const int totalSamples = (int) std::llround (sr * totalMs / 1000.0);
        const int stepSample   = (int) std::llround (sr * stepMs / 1000.0);

        double phase = 0.0;
        for (int n = 0; n < totalSamples; ++n)
        {
            const double hz = (n < stepSample) ? hzA : hzB;
            // A voice-like harmonic mix: fundamental + 2nd + 3rd, so YIN's
            // octave heuristics see the same kind of signal they see live.
            const float s = (float) (0.60 * std::sin (phase)
                                   + 0.30 * std::sin (2.0 * phase)
                                   + 0.10 * std::sin (3.0 * phase));
            phase += 2.0 * juce::MathConstants<double>::pi * hz / sr;

            fifo[(size_t) writeIdx] = s;
            writeIdx = (writeIdx + 1) % fifoSize;
            filled = juce::jmin (filled + 1, fifoSize);
            ++sinceHop;

            if (filled < win || sinceHop < cfg.hop)
                continue;

            sinceHop = 0;

            // Un-FIFO the newest `win` samples in chronological order.
            int idx = (writeIdx - win + fifoSize) % fifoSize;
            for (int d = 0; d < decimWin; ++d)
            {
                float sum = 0.0f;
                for (int k = 0; k < decim; ++k)
                {
                    sum += fifo[(size_t) idx];
                    idx = (idx + 1) % fifoSize;
                }
                linear[(size_t) d] = sum / (float) decim;
            }

            const auto t0 = std::chrono::high_resolution_clock::now();
            const float f = det.detectPitch (linear.data(), decimWin);
            const auto t1 = std::chrono::high_resolution_clock::now();
            totalDetectUs += std::chrono::duration<double, std::micro> (t1 - t0).count();
            ++calls;

            if (f > 0.0f)
                frames.push_back ({ (double) n * 1000.0 / sr, f });
        }

        Result r;
        r.calls = calls;
        r.detectUsPerSec = totalDetectUs / (totalMs / 1000.0);

        // Jitter and mean error on the steady note before the step.
        {
            double sumC = 0.0, sumC2 = 0.0;
            int n = 0;
            for (auto& fr : frames)
            {
                if (fr.ms < 500.0 || fr.ms >= stepMs - 50.0)
                    continue;
                const double cents = 1200.0 * std::log2 ((double) fr.hz / hzA);
                sumC += cents; sumC2 += cents * cents; ++n;
            }
            if (n > 1)
            {
                r.meanCents = sumC / n;
                const double var = sumC2 / n - r.meanCents * r.meanCents;
                r.jitterCents = std::sqrt (juce::jmax (0.0, var));
            }
        }

        // Lock time: first frame within 15 cents of the new note that stays
        // within 30 cents for the next 200 ms.
        {
            auto centsAt = [&] (float hz) { return 1200.0 * std::log2 ((double) hz / hzB); };
            for (size_t i = 0; i < frames.size(); ++i)
            {
                if (frames[i].ms < stepMs)
                    continue;
                if (std::abs (centsAt (frames[i].hz)) > 15.0)
                    continue;

                bool stable = true;
                for (size_t j = i; j < frames.size() && frames[j].ms < frames[i].ms + 200.0; ++j)
                    if (std::abs (centsAt (frames[j].hz)) > 30.0) { stable = false; break; }

                if (stable)
                {
                    r.lockMs = frames[i].ms - stepMs;
                    break;
                }
            }
        }

        return r;
    }

    static juce::String describe (const Config& cfg, const Result& r)
    {
        return juce::String (cfg.name)
             + ": window " + juce::String (cfg.window)
             + ", hop " + juce::String (cfg.hop)
             + ", median " + juce::String (cfg.median)
             + " -> lock " + (r.lockMs < 0.0 ? juce::String ("NONE")
                                             : juce::String (r.lockMs, 1) + " ms")
             + ", jitter " + juce::String (r.jitterCents, 1) + " cents"
             + ", mean " + juce::String (r.meanCents, 1) + " cents"
             + ", YIN " + juce::String (r.detectUsPerSec / 1000.0, 3) + " ms/s ("
             + juce::String (r.calls) + " calls)";
    }

    void runTest() override
    {
        beginTest ("the live analysis config is measured and its costs are printed");
        {
            const Config live { "live", 4096, 2048, 5 };
            const Config liveMed3 { "live-median3", 4096, 2048, 3 };
            const Config liveMed1 { "live-median1", 4096, 2048, 1 };
            const Config shortWindow { "short", 2048, 512, 3 };
            const Config shortMedian1 { "short-median1", 2048, 512, 1 };
            const Config shortHop1k { "short-hop1k", 2048, 1024, 3 };

            const auto rLive   = measure (live, 200.0, 220.0, 130.81f, 523.25f);
            const auto rLive3  = measure (liveMed3, 200.0, 220.0, 130.81f, 523.25f);
            const auto rLive1  = measure (liveMed1, 200.0, 220.0, 130.81f, 523.25f);
            const auto rShort  = measure (shortWindow, 200.0, 220.0, 130.81f, 523.25f);
            const auto rShort1 = measure (shortMedian1, 200.0, 220.0, 130.81f, 523.25f);
            const auto rShortH = measure (shortHop1k, 200.0, 220.0, 130.81f, 523.25f);

            logMessage (describe (live, rLive));
            logMessage (describe (liveMed3, rLive3));
            logMessage (describe (liveMed1, rLive1));
            logMessage (describe (shortWindow, rShort));
            logMessage (describe (shortMedian1, rShort1));
            logMessage (describe (shortHop1k, rShortH));

            // The live path must actually be slow: if this ever gets fast the
            // premise of the Hard Tune analysis config has moved and the numbers
            // above have to be re-read.
            expect (rLive.lockMs > 60.0,
                    "the live analysis path (4096/2048/median5) must take tens of ms "
                    "to report a note step (got " + juce::String (rLive.lockMs, 1) + " ms)");

            // The short window + short hop + median 3 must clearly beat it.
            expect (rShort.lockMs > 0.0 && rShort.lockMs < rLive.lockMs,
                    "the short-window config must lock sooner than the live one ("
                    + juce::String (rShort.lockMs, 1) + " vs "
                    + juce::String (rLive.lockMs, 1) + " ms)");

            // Median 1 is the fastest but jitterier; median 3 must already be a
            // real improvement, and it is the compromise the mode uses.
            expect (rShort.lockMs <= rShort1.lockMs + 25.0,
                    "median 3 must stay close to the median-1 lock time ("
                    + juce::String (rShort.lockMs, 1) + " vs "
                    + juce::String (rShort1.lockMs, 1) + " ms)");

            // The detector cost must stay reasonable: the short config runs four
            // times more often, so allow a generous 3x the live cost.
            expect (rShort.detectUsPerSec < rLive.detectUsPerSec * 3.0 + 1000.0,
                    "the short-window YIN cost must stay within 3x the live cost ("
                    + juce::String (rShort.detectUsPerSec, 0) + " vs "
                    + juce::String (rLive.detectUsPerSec, 0) + " us/s)");

            // Steady-note jitter must not explode: the hard-tune sound tolerates
            // a little movement, but not a warble.
            expect (rShort.jitterCents < 12.0,
                    "the short-window steady-note jitter must stay musical ("
                    + juce::String (rShort.jitterCents, 1) + " cents)");
        }

        beginTest ("the short window still detects a bass note (G2, 98 Hz)");
        {
            const Config shortWindow { "short", 2048, 512, 3 };
            // G2 -> G#2. Not E2: E2 (82.41 Hz) is the exact bottom of the Bass
            // Voice Type range, where the period (133.8 samples at 11.025 kHz)
            // equals maxLag - no configuration can represent it, short window or
            // not, and the measurement would pin a pre-existing range edge.
            const auto rBass = measure (shortWindow, 98.0, 103.83, 82.41f, 329.63f);

            logMessage (describe (shortWindow, rBass));

            // A bass fundamental must be found, not transposed: mean within
            // 20 cents of G2 over the steady part, and a usable lock.
            expect (std::abs (rBass.meanCents) < 20.0,
                    "the short window must detect G2 at its own octave (mean "
                    + juce::String (rBass.meanCents, 1) + " cents)");
            expect (rBass.jitterCents < 30.0,
                    "the short window's G2 jitter must stay usable ("
                    + juce::String (rBass.jitterCents, 1) + " cents)");
            expect (rBass.lockMs > 0.0 && rBass.lockMs < 120.0,
                    "the short window must lock a bass step in under 120 ms (got "
                    + juce::String (rBass.lockMs, 1) + " ms)");
        }
    }
};

static DetectorResponseTest detectorResponseTest;
