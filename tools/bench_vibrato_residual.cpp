// tools/bench_vibrato_residual.cpp
// OpenVoxTuner — Hard Tune vibrato-residual benchmark
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Question: how much of a singer's vibrato does the Hard Tune chain actually
// flatten? The Hard Tune path runs the detector with the short analysis window
// (2048/512, median 3), quantises to the scale, applies the ratio instantly
// (no retarget smoothing, no speed floor) and drives the shipped OLA shifter.
// Intuition says the output must be flat (a constant target), but the shifter
// derives its grain periods from `smoothedF0` - a per-block one-pole with a
// ~290 ms time constant (PitchShifter.h, kF0SmoothAlpha) - while the caller's
// correction ratio is computed against the INSTANTANEOUS detected pitch. The
// quotient is not constant, so part of the vibrato survives (phase shifted).
//
// This benchmark renders a synthetic vibrato tone through the same chain as the
// plugin, measures the input and output vibrato depth (YIN pitch tracking in
// cents, 2.5th/97.5th percentile half-range) and prints the residual for:
//   - the shipped Hard Tune chain (2048/512, median 3);
//   - median 1 (less detector lag);
//   - a 4096-sample analysis window (more lag);
//   - an IDEAL detector: the true instantaneous pitch feeds the shifter, so the
//     only remaining contributor is the shifter's own f0 smoothing.
// The benchmark is informational (like bench_pitchshift_latency): it prints
// tables and fails only its own sanity gates.

#include "../Source/dsp/PitchShifter.h"
#include "../Source/dsp/YinPitchDetector.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    constexpr int    kBlock = 256;
    constexpr double kPi    = 3.14159265358979323846;

    double twoPi() { return 2.0 * kPi; }

    std::vector<float> makeVibrato (double f0, double rateHz, double depthCents,
                                    double sampleRate, double durationSec)
    {
        const int n = (int) std::lround (durationSec * sampleRate);
        std::vector<float> v ((size_t) n, 0.0f);
        double phase = 0.0;

        for (int i = 0; i < n; ++i)
        {
            const double t = i / sampleRate;
            const double cents = depthCents * std::sin (twoPi() * rateHz * t);
            const double inst = f0 * std::pow (2.0, cents / 1200.0);
            phase += twoPi() * inst / sampleRate;
            const double s = 0.60 * std::sin (phase)
                           + 0.28 * std::sin (2.0 * phase)
                           + 0.12 * std::sin (3.0 * phase);
            const double env = std::min ({ 1.0, t / 0.030, (durationSec - t) / 0.050 });
            v[(size_t) i] = (float) (0.45 * std::max (0.0, env) * s);
        }
        return v;
    }

    // ------------------------------------------------------------------
    // Measurement tracker: fine YIN (2048 window, 128 hop, median 1)
    // ------------------------------------------------------------------

    struct Tracker
    {
        ovtdsp::YinPitchDetector det;
        int window = 2048, hop = 128, decim = 4, decimWindow = 512;
        std::vector<float> fifo, linear;
        int writeIdx = 0, filled = 0, sinceHop = 0;

        void prepare (double sampleRate, float fMin, float fMax)
        {
            window = (int) std::lround (2048.0 * sampleRate / 44100.0);
            hop = (int) std::lround (128.0 * sampleRate / 44100.0);
            decimWindow = std::max (2, window / decim);
            const int fifoSize = std::max (4096, window * 2);
            fifo.assign ((size_t) fifoSize, 0.0f);
            linear.assign ((size_t) decimWindow, 0.0f);
            det.prepare (sampleRate / (double) decim, 512);
            det.setFrequencyRange (fMin, fMax);
            det.setMedianFrames (1);
        }

        float push (float x)
        {
            fifo[(size_t) writeIdx] = x;
            writeIdx = (writeIdx + 1) % (int) fifo.size();
            filled = std::min (filled + 1, (int) fifo.size());
            ++sinceHop;

            if (filled < window || sinceHop < hop)
                return 0.0f;
            sinceHop = 0;

            int idx = (writeIdx - window + (int) fifo.size()) % (int) fifo.size();
            for (int d = 0; d < decimWindow; ++d)
            {
                float sum = 0.0f;
                for (int k = 0; k < decim; ++k)
                {
                    sum += fifo[(size_t) idx];
                    idx = (idx + 1) % (int) fifo.size();
                }
                linear[(size_t) d] = sum / (float) decim;
            }
            return det.detectPitch (linear.data(), decimWindow);
        }
    };

    std::vector<std::pair<double, float>> track (const std::vector<float>& x, double sampleRate,
                                                 float fMin, float fMax)
    {
        Tracker t;
        t.prepare (sampleRate, fMin, fMax);
        std::vector<std::pair<double, float>> out;
        for (size_t i = 0; i < x.size(); ++i)
        {
            const float f = t.push (x[i]);
            if (f > 0.0f)
                out.push_back ({ (double) i / sampleRate, f });
        }
        return out;
    }

    double vibratoDepthCents (const std::vector<std::pair<double, float>>& tr, double t0, double t1)
    {
        std::vector<double> cents;
        for (const auto& [t, f] : tr)
            if (t >= t0 && t <= t1)
                cents.push_back (1200.0 * std::log2 ((double) f / 440.0));

        if (cents.size() < 8)
            return 0.0;

        std::sort (cents.begin(), cents.end());
        const size_t lo = cents.size() / 40;               // 2.5th percentile
        const size_t hi = cents.size() - 1 - cents.size() / 40;
        return 0.5 * (cents[hi] - cents[lo]);              // amplitude in cents
    }

    // ------------------------------------------------------------------
    // The chain: YIN detector + chromatic quantiser + shipped OLA shifter
    // ------------------------------------------------------------------

    struct Detection
    {
        long long sample = 0;
        float f0 = 0.0f;
        float ratio = 1.0f;
    };

    float quantiseToChromatic (float hz)
    {
        if (hz <= 0.0f) return 0.0f;
        const double midi = std::round (69.0 + 12.0 * std::log2 ((double) hz / 440.0));
        return (float) (440.0 * std::pow (2.0, (midi - 69.0) / 12.0));
    }

    std::vector<Detection> detect (const std::vector<float>& in, double sampleRate,
                                   int window, int hop, int medianFrames,
                                   float fMin, float fMax)
    {
        const int decim = 4;
        const int decimWindow = std::max (2, window / decim);
        const int fifoSize = std::max (4096, window * 2);

        ovtdsp::YinPitchDetector det;
        det.prepare (sampleRate / (double) decim, 512);
        det.setFrequencyRange (fMin, fMax);
        det.setMedianFrames (medianFrames);

        std::vector<float> fifo ((size_t) fifoSize, 0.0f);
        std::vector<float> linear ((size_t) decimWindow, 0.0f);
        int writeIdx = 0, filled = 0, sinceHop = 0;
        std::vector<Detection> out;

        for (size_t n = 0; n < in.size(); ++n)
        {
            fifo[(size_t) writeIdx] = in[n];
            writeIdx = (writeIdx + 1) % fifoSize;
            filled = std::min (filled + 1, fifoSize);
            ++sinceHop;

            if (filled < window || sinceHop < hop)
                continue;
            sinceHop = 0;

            int idx = (writeIdx - window + fifoSize) % fifoSize;
            for (int d = 0; d < decimWindow; ++d)
            {
                float sum = 0.0f;
                for (int k = 0; k < decim; ++k)
                {
                    sum += fifo[(size_t) idx];
                    idx = (idx + 1) % fifoSize;
                }
                linear[(size_t) d] = sum / (float) decim;
            }

            const float f = det.detectPitch (linear.data(), decimWindow);
            if (f <= 0.0f)
                continue;

            Detection d;
            d.sample = (long long) n;
            d.f0 = f;
            const float target = quantiseToChromatic (f);
            d.ratio = (target > 0.0f) ? juce::jlimit (0.25f, 4.0f, target / f) : 1.0f;
            out.push_back (d);
        }
        return out;
    }

    /** Renders `in` through the shipped shifter. When `idealF0` is non-null, its
        per-block value (true instantaneous pitch) replaces the detector output,
        isolating the shifter's own f0 smoothing. */
    std::vector<float> render (const std::vector<float>& in, double sampleRate, double f0Base,
                               double vibratoRate, double vibratoDepth,
                               const std::vector<Detection>& trajectory, bool idealDetector,
                               bool bypassF0Smoothing)
    {
        std::vector<float> out (in.size(), 0.0f);

        ovtdsp::PitchShifter sh;
        sh.prepare (sampleRate, kBlock);
        sh.setAttackTimeMs (30.0f);
        sh.setLatencyMs (10.0f);
        sh.setRatioJumpEnvelopeEnabled (false); // Hard Tune: no ratio-jump duck
        sh.setF0SmoothingEnabled (! bypassF0Smoothing); // experiment flag (default true)

        juce::AudioBuffer<float> inBuf (1, kBlock), outBuf (1, kBlock);
        size_t detIdx = 0;
        float currentF0 = 0.0f, currentRatio = 1.0f;

        for (size_t pos = 0; pos < in.size(); pos += kBlock)
        {
            const int n = (int) std::min<size_t> ((size_t) kBlock, in.size() - pos);

            if (idealDetector)
            {
                // Perfect detector: the true instantaneous pitch and the ratio
                // against it. The target is the note the vibrato circles.
                const double t = (double) pos / sampleRate;
                const double cents = vibratoDepth * std::sin (twoPi() * vibratoRate * t);
                currentF0 = (float) (f0Base * std::pow (2.0, cents / 1200.0));
                currentRatio = (currentF0 > 0.0f)
                    ? juce::jlimit (0.25f, 4.0f, (float) (quantiseToChromatic (currentF0) / currentF0))
                    : 1.0f;
            }
            else
            {
                while (detIdx < trajectory.size() && trajectory[detIdx].sample <= (long long) pos)
                {
                    currentF0 = trajectory[detIdx].f0;
                    currentRatio = trajectory[detIdx].ratio;
                    ++detIdx;
                }
            }

            inBuf.clear();
            outBuf.clear();
            inBuf.copyFrom (0, 0, in.data() + pos, n);
            sh.process (inBuf, outBuf, currentRatio, 1.0f, currentF0);
            std::memcpy (out.data() + pos, outBuf.getReadPointer (0), (size_t) n * sizeof (float));
        }
        return out;
    }

    struct Row
    {
        std::string config;
        double rate = 5.0, depth = 30.0;
        double inCents = 0.0, outCents = 0.0;
    };
}

int main()
{
    constexpr double sampleRate = 44100.0;
    constexpr double f0 = 220.0;      // A3; a ±50 cent vibrato stays inside the semitone
    constexpr double duration = 2.0;
    const float fMin = (float) (f0 * 0.7), fMax = (float) (f0 * 1.4);

    std::printf ("OpenVoxTuner — Hard Tune vibrato-residual benchmark\n");
    std::printf ("%.0f Hz tone, vibrato depth in cents, %.1f s; depth read as the 2.5-97.5%% half-range\n",
                 sampleRate, duration);
    std::printf ("residual = output depth / input depth (100%% = no flattening at all)\n\n");

    auto run = [&] (const char* config, int window, int hop, int median, bool ideal, bool bypassF0Smoothing = false)
    {
        std::vector<Row> rows;
        for (double rate : { 3.0, 4.0, 5.0, 6.0, 7.0 })
            for (double depth : { 15.0, 30.0, 50.0 })
            {
                const auto in = makeVibrato (f0, rate, depth, sampleRate, duration);
                const auto inTrack = track (in, sampleRate, fMin, fMax);
                const double inCents = vibratoDepthCents (inTrack, 0.8, 1.8);

                std::vector<Detection> trajectory;
                if (! ideal)
                    trajectory = detect (in, sampleRate, window, hop, median, fMin, fMax);

                const auto out = render (in, sampleRate, f0, rate, depth, trajectory, ideal, bypassF0Smoothing);
                const auto outTrack = track (out, sampleRate, fMin, fMax);
                rows.push_back ({ config, rate, depth, inCents, vibratoDepthCents (outTrack, 0.8, 1.8) });
            }

        std::printf ("%s\n", config);
        std::printf ("  rate Hz   depth c   input c   output c   residual\n");
        for (const auto& r : rows)
            std::printf ("  %7.1f   %7.1f   %7.2f   %8.2f   %6.0f %%\n",
                         r.rate, r.depth, r.inCents, r.outCents,
                         100.0 * r.outCents / juce::jmax (1.0e-6, r.inCents));
        std::printf ("\n");
        return rows;
    };

    const auto shipped = run ("previous Hard Tune detector (2048/512, median 3)", 2048, 512, 3, false);
    run ("median 1 (less detector lag)", 2048, 512, 1, false);
    run ("4096 window, hop 1024, median 3 (more lag)", 4096, 1024, 3, false);
    run ("previous detector + f0 smoothing OFF (fix candidate)", 2048, 512, 3, false, true);
    const auto ideal = run ("IDEAL detector (true pitch; shifter smoothing only)", 2048, 512, 3, true);

    // --- Option 1: a shorter Hard Tune analysis window ------------------------
    // The residual is proportional to the detector lag (tau), so the shortest
    // window that still resolves the lowest voice is the interesting candidate.
    // 1024 samples at 44.1 kHz is 23 ms, i.e. 1.9 periods at 82.41 Hz, so the
    // detection-stability section below gates that choice.
    run ("option 1a: 1536/256, median 1", 1536, 256, 1, false);
    run ("option 1b: 1536/256, median 3", 1536, 256, 3, false);
    run ("option 1c: 1024/256, median 1", 1024, 256, 1, false);
    run ("option 1d: 1024/512, median 1", 1024, 512, 1, false);

    // --- Detection stability (the cost of a shorter window) -------------------
    // A steady harmonic tone per register floor: how accurate and octave-safe is
    // the detector at each config? Mean |cents| error and gross errors (>100 c).
    std::printf ("detection accuracy on a steady tone (mean |cents|, max |cents|, gross errors)\n");
    std::printf ("  %-22s", "config\\f0");
    const double pitches[] = { 82.41, 110.0, 130.81, 174.61, 261.63 };
    for (double p : pitches) std::printf ("   %6.0f Hz", p);
    std::printf ("\n");

    auto detectionQuality = [&] (const char* label, int window, int hop, int median)
    {
        std::printf ("  %-22s", label);
        for (double p : pitches)
        {
            const auto tone = makeVibrato (p, 0.0, 0.0, sampleRate, 1.0);
            const auto dets = detect (tone, sampleRate, window, hop, median,
                                      (float) (p * 0.7), (float) (p * 1.4));
            double sum = 0.0, worst = 0.0;
            int gross = 0;
            for (const auto& d : dets)
            {
                if (d.f0 <= 0.0f) continue;
                const double c = std::abs (1200.0 * std::log2 ((double) d.f0 / p));
                sum += c;
                worst = std::max (worst, c);
                if (c > 100.0) ++gross;
            }
            const double mean = dets.empty() ? 999.0 : sum / (double) dets.size();
            std::printf ("  %5.1f/%4.0f/%2d", mean, worst, gross);
        }
        std::printf ("\n");
    };

    detectionQuality ("2048/512 med3 (shipped)", 2048, 512, 3);
    detectionQuality ("2048/512 med1", 2048, 512, 1);
    detectionQuality ("1536/256 med1", 1536, 256, 1);
    detectionQuality ("1024/256 med1", 1024, 256, 1);
    std::printf ("\n");

    // --- sanity gates ----------------------------------------------------------
    int failures = 0;
    auto check = [&] (bool ok, const char* what)
    {
        std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what);
        if (! ok) ++failures;
    };

    check (shipped.size() == 15, "shipped table has 15 rows");
    check (ideal.size() == 15, "ideal-detector table has 15 rows");

    double maxResidual = 0.0;
    for (const auto& r : shipped)
        maxResidual = juce::jmax (maxResidual, r.outCents / juce::jmax (1.0e-6, r.inCents));
    check (maxResidual < 2.5, "shipped residual stays below 250 % (sanity)");

    std::printf ("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
