// tools/DopplerProbe.h
// OpenVoxTuner — analysis-only low-latency pitch-shifter probe
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A deliberately simple two-tap crossfaded delay-line pitch shifter. It exists
// to price what 2-4 ms of algorithmic latency costs in quality (Phase 3 of the
// Hard Tune plan); it is NOT a product candidate and is not compiled into the
// plugin.
//
// How it works: while the write head advances one sample, a tap's delay ramps by
// (1 - ratio) samples, so the read head advances `ratio` samples per output
// sample (pitch up when ratio > 1). Each tap sweeps linearly across
// [minDelay, minDelay + sweep] and wraps; a triangular crossfade gives the
// wrapping tap zero weight. The average delay is `minDelay + sweep / 2`.
//
// Known character (see docs/low-latency-shifter-analysis.md): flanging, comb
// filtering, amplitude warble at the wrap rate. No formant control: formants
// follow the pitch shift. That is the point of the probe.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace ovtprobe
{
    class DopplerShifter
    {
    public:
        void prepare (double sampleRate)
        {
            sr = (sampleRate > 0.0) ? sampleRate : 44100.0;
            ring.assign ((size_t) 1 << 15, 0.0f);
            mask = (int) ring.size() - 1;
            writeIdx = 0;
            written = 0;
            phase = 0.0;
            ratioSmoothed = 1.0;
        }

        /** minDelay + sweep/2 = the average (declared) delay. */
        void setDelays (double baseMs, double sweepMs)
        {
            minDelay = std::max (0.0, baseMs) * sr / 1000.0;
            sweep = std::max (1.0, sweepMs * sr / 1000.0);
        }

        /** Sets the average delay: base = 0.4 x ms, sweep = 1.2 x ms. */
        void setLatencyMs (double averageMs)
        {
            setDelays (averageMs * 0.4, averageMs * 1.2);
        }

        int latencySamples() const
        {
            return (int) std::lround (minDelay + 0.5 * sweep);
        }

        double averageLatencyMs() const
        {
            return (minDelay + 0.5 * sweep) * 1000.0 / sr;
        }

        void process (const float* in, float* out, int n, float ratio)
        {
            for (int i = 0; i < n; ++i)
            {
                ratioSmoothed += 0.0005 * ((double) ratio - ratioSmoothed);

                ring[(size_t) (writeIdx & mask)] = in[i];
                ++writeIdx;
                ++written;

                // Delay decreases at (ratio - 1) samples per sample; the phase
                // advance maps that onto the sweep span and wraps each tap.
                phase -= (ratioSmoothed - 1.0) / sweep;
                const double pA = phase - std::floor (phase);
                const double pB = pA + 0.5 - std::floor (pA + 0.5);

                const double dA = minDelay + sweep * pA;
                const double dB = minDelay + sweep * pB;

                const double gA = 1.0 - std::abs (2.0 * pA - 1.0);
                const double gB = 1.0 - gA;

                out[i] = (float) (gA * readAt (dA) + gB * readAt (dB));
            }
        }

    private:
        float readAt (double delay) const
        {
            double pos = (double) written - delay;
            if (pos < 0.0) pos = 0.0; // before the stream starts
            const int i0 = (int) std::floor (pos);
            const double frac = pos - (double) i0;
            const float a = ring[(size_t) (i0 & mask)];
            const float b = ring[(size_t) ((i0 + 1) & mask)];
            return (float) ((1.0 - frac) * a + frac * b);
        }

        double sr = 44100.0;
        std::vector<float> ring;
        int mask = 0;
        int writeIdx = 0;
        long long written = 0;
        double minDelay = 0.0; // samples
        double sweep = 0.0;    // samples
        double phase = 0.0;
        double ratioSmoothed = 1.0;
    };
}
