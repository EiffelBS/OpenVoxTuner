// bench_deesser_placement.cpp
// OpenVoxTuner — de-esser placement benchmark (before vs after the reverb)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The De-Esser sits BEFORE ReverbEffect in the effects chain: it de-esses the
// corrected lead voice, not the reverb tail. This tool measures what that
// placement buys, and what it costs, against the alternative order.
//
// Signal: a synthetic voiced voice — a 180 Hz harmonic stack with 7 short
// sibilant bursts (band-limited noise in the 5..8 kHz region), followed by a
// 400 ms window where the input goes silent so the reverb decay is measured on
// its own.
//
// Chains compared:
//   bypass   : reverb only, de-esser off                      -> reference
//   before   : de-esser -> reverb  (the shipped order)
//   after    : reverb -> de-esser  (de-essing the wet tail too)
//
// Metrics, all measured on the OUTPUT:
//   - sibilance: RMS of the summed 5.5 / 6.5 / 7.5 kHz band-pass bank;
//   - total RMS: loudness impact;
//   - tail: the last 400 ms (input silent) — sibilance, total, and the BODY band
//     (400 / 800 / 1600 Hz), so the room's low-mid content is measured apart from
//     its brightness;
//   - pumping: peak-to-trough swing of the de-esser gain inside the tail window.
//
// Settings used by every chain: threshold -35 dB, max reduction 12 dB, band
// centre 7 kHz, reverb room 0.4 / damping 0.6 / wet 0.3. The bursts are set hot
// (a strong un-de-essed "s"), the voice is kept around -8 dBFS.
//
// Gates (exit code 1 on failure):
//   - the voice alone must never reach the detector close point, the bursts must
//     clear the open point (band-limited detection is the whole design);
//   - "before" must cut burst sibilance by at least 6 dB against bypass;
//   - "before" must clean the tail harshness at least 4 dB deeper than "after":
//     cutting the esses before they enter the room is what the placement buys;
//   - "before" must keep more loudness and not dull the tail body more than
//     "after" (de-essing the wet tail costs level and low-mid content);
//   - "before" must show no pumping in the tail window, and "after" must pump the
//     tail strictly more than "before".

#include "../Source/dsp/DeEsser.h"
#include "../Source/dsp/ReverbEffect.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

namespace
{
    const double sr = 44100.0;
    const int block = 512;
    const int tailSamples = 400 * 441 / 100;            // 400 ms tail-only window
    const int totalSamples = 2 * 44100 + tailSamples;    // ~2.4 s
    const int tailStart = totalSamples - tailSamples;
    const int burstLen = int (0.090 * sr);  // 90 ms sibilant burst
    const int burstGap = int (0.250 * sr);   // one burst every 250 ms
    const int firstBurst = int (0.30 * sr);   // first burst onset

    const double twoPi() { return 2.0 * std::acos (-1.0); }

    // RBJ band-pass biquad (BPF), coefficients pre-normalised by a0.
    struct BandPass
    {
        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;

        void set (float freqHz, float q)
        {
            const double w = twoPi() * freqHz / sr;
            const double cw = std::cos (w), sw = std::sin (w);
            const double alpha = sw / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = float (alpha / a0);
            b1 = 0.0f;
            b2 = float (-alpha / a0);
            a1 = float (-2.0 * cw / a0);
            a2 = float ((1.0 - alpha) / a0);
        }

        void reset() { x1 = x2 = y1 = y2 = 0.0f; }

        float process (float x)
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x; y2 = y1; y1 = y;
            return y;
        }
    };

    // Sum of three band-passes covering the 5..8 kHz sibilance region.
    struct SibilanceMeter
    {
        BandPass bps[3];
        void reset() { for (auto& b : bps) b.reset(); }
        float process (float x)
        {
            return bps[0].process (x) + bps[1].process (x) + bps[2].process (x);
        }
    };

    // Sum of three band-passes covering the 400 / 800 / 1600 Hz voice body. Measured
    // in the tail window: a de-esser that rides the wet tail ducks the body of the
    // room, which is the audible cost of placing it after the reverb.
    struct BodyMeter
    {
        BandPass bps[3];
        void reset() { for (auto& b : bps) b.reset(); }
        float process (float x)
        {
            return bps[0].process (x) + bps[1].process (x) + bps[2].process (x);
        }
    };

    double rmsOf (const std::vector<float>& vals)
    {
        if (vals.empty()) return 0.0;
        double acc = 0.0;
        for (float v : vals) acc += double (v) * double (v);
        return std::sqrt (acc / double (vals.size()));
    }

    // Build the test signal: harmonic voice + 7 sibilant bursts, then silence.
    std::vector<float> makeSignal()
    {
        std::mt19937 rng (12345);
        std::normal_distribution<float> noise (0.0f, 1.0f);

        std::vector<float> out (totalSamples, 0.0f);
        const double f0 = 180.0;
        const int voiceSamples = tailStart;

        BandPass hp, lp;
        hp.set (5000.0f, 0.7071f); hp.reset();
        lp.set (9000.0f, 0.7071f); lp.reset();
        std::vector<float> burst (burstLen);
        for (int i = 0; i < burstLen; ++i)
            burst[i] = 0.36f * lp.process (hp.process (noise (rng))); // hot, un-de-essed "s"

        double ph = 0.0;
        for (int i = 0; i < voiceSamples; ++i)
        {
            float v = 0.0f;
            for (int k = 1; k <= 10; ++k)
                v += float (1.0 / std::pow (double (k), 1.6)) * float (std::sin (ph * k));
            v *= 0.35f / 2.4f; // keep the voice around -8 dBFS
            ph += twoPi() * f0 / sr;
            if (ph > twoPi()) ph -= twoPi();
            out[i] = v;
        }

        for (int b = 0; b < 7; ++b)
        {
            const int start = int (0.30 * sr) + b * burstGap;
            if (start + burstLen >= voiceSamples) break;
            for (int i = 0; i < burstLen; ++i) out[start + i] += burst[i];
        }
        return out; // the last 400 ms are input silence: reverb tail only
    }

    struct Result
    {
        double sibilanceRms = 0.0, totalRms = 0.0;
        double tailSibilanceRms = 0.0, tailTotalRms = 0.0, tailBodyRms = 0.0;
        double gainSwing = 0.0;
        double detVoiceMax = 0.0;   // detector peak before the first burst (voice only)
        double detBurstMax = 0.0;    // detector peak inside the burst windows
        double detTailMax = 0.0;     // detector peak in the tail-only window
    };

    // Run one chain over the whole signal and collect the metrics.
    // deEsserOn=false gives the bypass reference (the de-esser stays fully muted).
    Result runChain (const std::vector<float>& in, bool deEsserOn, bool deEsserFirst)
    {
        ovtdsp::DeEsser de;
        ovtdsp::ReverbEffect rev;
        de.prepare (sr, block); de.reset();
        rev.prepare (sr, block); rev.reset();
        de.setThresholdDb (-35.0f);
        de.setMaxReductionDb (12.0f);
        de.setFrequencyHz (7000.0f);
        rev.setRoomSize (0.4f);
        rev.setDamping (0.6f);

        SibilanceMeter meter;
        for (int i = 0; i < 3; ++i) meter.bps[i].set (5500.0f + 1000.0f * i, 1.2f);
        meter.reset();

        BodyMeter body;
        for (int i = 0; i < 3; ++i) body.bps[i].set (400.0f + 400.0f * i, 1.2f);
        body.reset();

        const int lastBurstEnd = firstBurst + 6 * burstGap + burstLen;
        std::vector<float> highAll, totalAll, highTail, totalTail, bodyTail;
        float gainMin = 1.0f, gainMax = 0.0f;
        Result r;

        juce::AudioBuffer<float> buf (2, block);
        for (int off = 0; off < totalSamples; off += block)
        {
            for (int s = 0; s < block; ++s)
            {
                const int idx = off + s;
                const float v = idx < totalSamples ? in[idx] : 0.0f;
                buf.setSample (0, s, v);
                buf.setSample (1, s, v);
            }

            if (deEsserFirst)
            {
                de.process (buf, deEsserOn, 1.0f);
                rev.process (buf, true, 0.3f);
            }
            else
            {
                rev.process (buf, true, 0.3f);
                de.process (buf, deEsserOn, 1.0f);
            }

            for (int s = 0; s < block; ++s)
            {
                const float y = buf.getSample (0, s);
                const float band = meter.process (y);
                highAll.push_back (band);
                totalAll.push_back (y);
                if (off + s >= tailStart)
                {
                    highTail.push_back (band);
                    totalTail.push_back (y);
                    bodyTail.push_back (body.process (y));
                }
            }

            // Detector and gain are per-sample states, sampled once per block.
            const float d = de.getDetector(), g = de.getCurrentGain();
            if (off < firstBurst && d > r.detVoiceMax) r.detVoiceMax = d;
            if (off >= firstBurst && off <= lastBurstEnd && d > r.detBurstMax) r.detBurstMax = d;
            if (off >= tailStart)
            {
                if (d > r.detTailMax) r.detTailMax = d;
                if (g < gainMin) gainMin = g;
                if (g > gainMax) gainMax = g;
            }
        }

        r.sibilanceRms = rmsOf (highAll);
        r.totalRms = rmsOf (totalAll);
        r.tailSibilanceRms = rmsOf (highTail);
        r.tailTotalRms = rmsOf (totalTail);
        r.tailBodyRms = rmsOf (bodyTail);
        r.gainSwing = deEsserOn ? gainMax - gainMin : 0.0;
        return r;
    }

    double db (double ratio) { return ratio > 0.0 ? 20.0 * std::log10 (ratio) : -120.0; }
}

int main()
{
    const auto in = makeSignal();

    const Result bypass  = runChain (in, false, true);
    const Result before  = runChain (in, true, true);   // de-esser -> reverb (shipped)
    const Result after   = runChain (in, true, false);  // reverb -> de-esser

    const double closeLevel = std::pow (10.0, -35.0 / 20.0);  // detector close point
    const double openLevel  = closeLevel * 2.0;               // +6 dB open point

    std::printf ("De-Esser placement bench (sr %.0f, block %d, threshold -35 dB, max reduction 12 dB, fc 7 kHz, reverb room 0.4 damp 0.6 wet 0.3)\n",
                 sr, block);
    std::printf ("chain     sibilance   total    tail sibilance  tail body    tail gain swing\n");
    std::printf ("bypass    %+7.2f dB  %+7.2f dB   %+7.2f dB     %+7.2f dB    %.4f\n",
                 0.0, 0.0, 0.0, 0.0, bypass.gainSwing);

    auto row = [&] (const char* name, const Result& r)
    {
        std::printf ("%-8s %+7.2f dB  %+7.2f dB   %+7.2f dB     %+7.2f dB    %.4f\n",
                     name,
                     db (r.sibilanceRms / bypass.sibilanceRms),
                     db (r.totalRms / bypass.totalRms),
                     db (r.tailSibilanceRms / bypass.tailSibilanceRms),
                     db (r.tailBodyRms / bypass.tailBodyRms),
                     r.gainSwing);
    };
    row ("before", before);
    row ("after", after);

    std::printf ("detector peaks (band RMS, dBFS): voice %.2f  burst %.2f  tail %.2f   close %+6.2f dB  open %+6.2f dB\n",
                 db (before.detVoiceMax), db (before.detBurstMax), db (before.detTailMax),
                 db (closeLevel), db (openLevel));

    const double cutBefore     = db (before.sibilanceRms / bypass.sibilanceRms);
    const double tailCutBefore = db (before.tailSibilanceRms / bypass.tailSibilanceRms);
    const double tailCutAfter  = db (after.tailSibilanceRms / bypass.tailSibilanceRms);
    const double bodyKeep      = db (before.tailBodyRms / bypass.tailBodyRms);
    const double bodyDullAfter = db (after.tailBodyRms / bypass.tailBodyRms);
    const double loudBefore    = db (before.totalRms / bypass.totalRms);
    const double loudAfter     = db (after.totalRms / bypass.totalRms);

    bool ok = true;
    if (! (cutBefore <= -6.0))        { std::printf ("GATE FAIL: 'before' must cut burst sibilance by >= 6 dB, got %.2f\n", cutBefore); ok = false; }
    if (! (tailCutBefore <= tailCutAfter - 4.0)) { std::printf ("GATE FAIL: 'before' must clean the tail harshness >= 4 dB deeper than 'after' (%.2f vs %.2f)\n", tailCutBefore, tailCutAfter); ok = false; }
    if (! (loudBefore > loudAfter))   { std::printf ("GATE FAIL: 'before' must keep more loudness than 'after' (%.2f vs %.2f)\n", loudBefore, loudAfter); ok = false; }
    if (! (bodyKeep >= bodyDullAfter - 0.5)) { std::printf ("GATE FAIL: 'before' must not dull the tail body more than 'after' (%.2f vs %.2f)\n", bodyKeep, bodyDullAfter); ok = false; }
    if (! (before.gainSwing < 0.05))  { std::printf ("GATE FAIL: 'before' must not pump in the tail window, swing %.4f\n", before.gainSwing); ok = false; }
    if (! (after.gainSwing > before.gainSwing + 0.02)) { std::printf ("GATE FAIL: 'after' must pump the tail more than 'before' (%.4f vs %.4f)\n", after.gainSwing, before.gainSwing); ok = false; }
    if (! (before.detVoiceMax < closeLevel)) { std::printf ("GATE FAIL: the voice alone must never trigger the detector, peak %.2f dB vs close %.2f dB\n", db (before.detVoiceMax), db (closeLevel)); ok = false; }
    if (! (before.detBurstMax > openLevel))  { std::printf ("GATE FAIL: the bursts must trigger the detector, peak %.2f dB vs open %.2f dB\n", db (before.detBurstMax), db (openLevel)); ok = false; }

    std::printf ("%s\n", ok ? "PLACEMENT GATES PASS" : "PLACEMENT GATES FAIL");
    return ok ? 0 : 1;
}
