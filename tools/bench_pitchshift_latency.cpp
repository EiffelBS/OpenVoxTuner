// bench_pitchshift_latency.cpp
// OpenVoxTuner — low-latency pitch-shifter analysis benchmark
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Phase 3 of the Hard Tune plan asks whether the pitch shifter can run below the
// current 10-30 ms declared latency. This tool measures the frontier instead of
// guessing:
//
// 1) The SHIPPED shifter (pitch-synchronous OLA in Source/dsp/PitchShifter) at
//    declared latencies 8/10/12/20/30 ms, and at voice fundamentals 82/120/200/
//    400 Hz. It reports the DECLARED latency, the EFFECTIVE latency measured on
//    the signal (AM-envelope cross-correlation, so the pitch period itself does
//    not make the measurement ambiguous), CPU, level, click level, pitch
//    accuracy and spectral distortion against an ideal frequency shift.
//
// 2) A deliberately simple two-tap crossfade delay-line shifter ("Doppler"),
//    the cheapest way to reach ~2.5-4 ms of latency, with the same metrics. It is
//    NOT a product candidate: it exists to price what latency costs in quality.
//
// Metrics, all measured on the OUTPUT of a mono 44.1 kHz stream:
//   - declared / effective latency (ms);
//   - CPU: microseconds of process() per second of audio;
//   - RMS ratio: output vs input level at +1 semitone (dB);
//   - click: max sample-to-sample jump on a 200 Hz sine with semitone steps
//     (the same 0.15 threshold the unit tests use);
//   - pitch: detected output pitch error against f0*ratio (cents);
//   - LSD: log-spectral distortion of the shifted vowel against the ideal
//     frequency-scaled input spectrum, 120 Hz..8 kHz (dB, lower is better).
//
// The tool is informational: it prints the tables and exits 0. The few sanity
// checks that do fail the process (exit code 1) guard against a measurement that
// stopped measuring anything (silent output, wrong pitch).

#include "../Source/dsp/PitchShifter.h"
#include "../Source/dsp/YinPitchDetector.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int    kBlock      = 256;

    double twoPi() { return 2.0 * std::acos (-1.0); }

    // ------------------------------------------------------------------
    // Signals
    // ------------------------------------------------------------------

    /** Harmonic vowel: F0 plus harmonics shaped by three Gaussian formant
        resonances (700/1220/2600 Hz), so the spectrum is voice-like enough for
        the distortion metric. */
    std::vector<float> makeVowel (double f0, double seconds)
    {
        const int n = (int) std::llround (seconds * kSampleRate);
        std::vector<float> v ((size_t) n, 0.0f);

        const double formants[3] = { 700.0, 1220.0, 2600.0 };
        const double bandwidths[3] = { 130.0,  70.0,  160.0 };
        const double gains[3] = { 1.0, 0.6, 0.35 };

        for (int i = 0; i < n; ++i)
        {
            const double t = i / kSampleRate;
            double s = 0.0;
            for (int h = 1; h * f0 < 9000.0; ++h)
            {
                const double f = h * f0;
                double amp = 0.0;
                for (int k = 0; k < 3; ++k)
                {
                    const double d = (f - formants[k]) / bandwidths[k];
                    amp += gains[k] * std::exp (-0.5 * d * d);
                }
                amp = 0.08 + amp;
                s += amp * std::sin (twoPi() * f * t);
            }
            v[(size_t) i] = (float) (0.12 * s);
        }

        // Normalise to a -6 dBFS peak so the metrics do not depend on f0's
        // harmonic count.
        float peak = 0.0f;
        for (auto s : v) peak = juce::jmax (peak, std::abs (s));
        if (peak > 1.0e-6f)
            for (auto& s : v) s *= 0.5f / peak;
        return v;
    }

    /** Sine at f0 with a 8 Hz amplitude modulation: the envelope carries the
        timing, so cross-correlating envelopes measures latency without the
        pitch-period ambiguity of a raw sine. */
    std::vector<float> makeAmSine (double f0, double seconds)
    {
        const int n = (int) std::llround (seconds * kSampleRate);
        std::vector<float> v ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / kSampleRate;
            const double env = 0.55 + 0.45 * std::sin (twoPi() * 8.0 * t);
            v[(size_t) i] = (float) (0.5 * env * std::sin (twoPi() * f0 * t));
        }
        return v;
    }

    std::vector<float> makeSine (double f0, double seconds)
    {
        const int n = (int) std::llround (seconds * kSampleRate);
        std::vector<float> v ((size_t) n, 0.0f);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = (float) (0.5 * std::sin (twoPi() * f0 * i / kSampleRate));
        return v;
    }


    // ------------------------------------------------------------------
    // Shifters under test
    // ------------------------------------------------------------------

    struct Shifter
    {
        virtual ~Shifter() = default;
        virtual const char* name() const = 0;
        virtual void prepare() = 0;
        virtual void setDeclaredLatencyMs (double ms) = 0;
        virtual int  declaredLatencySamples() const = 0;
        virtual void processBlock (const float* in, float* out, int n, float ratio, float f0) = 0;
    };

    /** The shipped pitch-synchronous OLA shifter, wrapped for mono blocks. */
    struct PsolaShifter : Shifter
    {
        ovtdsp::PitchShifter ps;
        juce::AudioBuffer<float> inBuf { 1, kBlock };
        juce::AudioBuffer<float> outBuf { 1, kBlock };
        double latencyMs = 10.0;

        const char* name() const override { return "PSOLA (shipped)"; }

        void prepare() override
        {
            ps.prepare (kSampleRate, kBlock);
            ps.setAttackTimeMs (30.0f);
            ps.setLatencyMs ((float) latencyMs);
        }

        void setDeclaredLatencyMs (double ms) override
        {
            latencyMs = ms;
            ps.setLatencyMs ((float) ms);
        }

        int declaredLatencySamples() const override { return ps.getLatencySamples(); }

        void processBlock (const float* in, float* out, int n, float ratio, float f0) override
        {
            inBuf.setSize (1, n, false, true, false);
            outBuf.setSize (1, n, false, true, false);
            inBuf.copyFrom (0, 0, in, n);
            outBuf.clear();
            ps.process (inBuf, outBuf, ratio, 1.0f, f0);
            std::memcpy (out, outBuf.getReadPointer (0), (size_t) n * sizeof (float));
        }
    };

    /** Two-tap crossfaded delay-line shifter ("Doppler"): the cheapest way to a
        2-4 ms algorithmic latency. The delay of each tap sweeps linearly and
        wraps; a triangular crossfade hides the wrap. Quality is expected to be
        poor - that is exactly the number this benchmark exists to report. */
    struct DopplerShifter : Shifter
    {
        std::vector<float> ring;
        int writeIdx = 0;
        int written = 0;
        double minDelay = 0.0;   // samples
        double sweep = 0.0;      // samples
        double phase = 0.0;
        double ratioSmoothed = 1.0;

        const char* name() const override { return "Doppler 2-tap"; }

        void prepare() override
        {
            ring.assign (1 << 15, 0.0f);
            writeIdx = 0;
            written = 0;
            phase = 0.0;
            ratioSmoothed = 1.0;
            setDelays (declaredBaseMs, declaredSweepMs);
        }

        double declaredBaseMs = 1.0;
        double declaredSweepMs = 3.0;

        void setDelays (double baseMs, double sweepMs)
        {
            declaredBaseMs = baseMs;
            declaredSweepMs = sweepMs;
            minDelay = baseMs * kSampleRate / 1000.0;
            sweep = juce::jmax (1.0, sweepMs * kSampleRate / 1000.0);
        }

        void setDeclaredLatencyMs (double ms) override
        {
            // Applies to the AVERAGE delay of the shifter: min + sweep/2.
            setDelays (ms * 0.4, ms * 1.2);
        }

        int declaredLatencySamples() const override
        {
            return (int) std::lround (minDelay + 0.5 * sweep);
        }

        float readAt (double delay) const
        {
            const double pos = (double) written - delay;
            const int i0 = (int) std::floor (pos);
            const double frac = pos - (double) i0;
            const int mask = (int) ring.size() - 1;
            const float a = ring[(size_t) (i0 & mask)];
            const float b = ring[(size_t) ((i0 + 1) & mask)];
            return (float) ((1.0 - frac) * a + frac * b);
        }

        void processBlock (const float* in, float* out, int n, float ratio, float f0) override
        {
            juce::ignoreUnused (f0);
            for (int i = 0; i < n; ++i)
            {
                ratioSmoothed += 0.0005 * ((double) ratio - ratioSmoothed);

                ring[(size_t) (writeIdx & ((int) ring.size() - 1))] = in[i];
                ++writeIdx;
                ++written;

                // Delay decreases at (ratio - 1) samples per output sample, so
                // phase advances at (ratio - 1) / sweep (negative when shifting
                // up). frac() then makes each tap wrap from max back to min.
                phase -= (ratioSmoothed - 1.0) / sweep;
                const double pA = phase - std::floor (phase);
                const double pB = pA + 0.5 - std::floor (pA + 0.5);

                const double dA = minDelay + sweep * pA;
                const double dB = minDelay + sweep * pB;

                // Triangular crossfade: the tap that wraps has weight 0.
                const double gA = 1.0 - std::abs (2.0 * pA - 1.0);
                const double gB = 1.0 - gA;

                out[i] = (float) (gA * readAt (dA) + gB * readAt (dB));
            }
        }
    };

    // ------------------------------------------------------------------
    // Measurements
    // ------------------------------------------------------------------

    void processAll (Shifter& sh, const std::vector<float>& in, std::vector<float>& out,
                     float ratio, float f0, int warmupSamples = 0)
    {
        out.assign (in.size(), 0.0f);
        std::vector<float> blockIn ((size_t) kBlock), blockOut ((size_t) kBlock);
        for (size_t pos = 0; pos < in.size(); pos += kBlock)
        {
            const int n = (int) std::min<size_t> ((size_t) kBlock, in.size() - pos);
            for (int i = 0; i < n; ++i) blockIn[(size_t) i] = in[pos + (size_t) i];
            sh.processBlock (blockIn.data(), blockOut.data(), n, ratio, f0);
            for (int i = 0; i < n; ++i) out[pos + (size_t) i] = blockOut[(size_t) i];
        }
        juce::ignoreUnused (warmupSamples);
    }

    double rms (const std::vector<float>& v, size_t from = 0, size_t to = (size_t) -1)
    {
        if (to == (size_t) -1 || to > v.size()) to = v.size();
        double sum = 0.0;
        for (size_t i = from; i < to; ++i) sum += (double) v[i] * v[i];
        return std::sqrt (sum / (double) juce::jmax<size_t> (1, to - from));
    }

    /** Amplitude envelope: rectify + one-pole low-pass at ~50 Hz. */
    std::vector<float> envelopeOf (const std::vector<float>& v)
    {
        std::vector<float> env (v.size(), 0.0f);
        const double a = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 50.0 / kSampleRate);
        double state = 0.0;
        for (size_t i = 0; i < v.size(); ++i)
        {
            state += a * (std::abs ((double) v[i]) - state);
            env[i] = (float) state;
        }
        return env;
    }

    /** Effective latency: the lag of the output envelope against the input
        envelope, over the steady part of the signal. */
    double effectiveLatencyMs (const std::vector<float>& in, const std::vector<float>& out,
                               double secondsSkip = 0.6)
    {
        const auto ein = envelopeOf (in);
        const auto eout = envelopeOf (out);

        const size_t skip = (size_t) (secondsSkip * kSampleRate);
        const int maxLag = (int) (0.060 * kSampleRate); // 60 ms
        if (ein.size() < skip + (size_t) maxLag + 2) return -1.0;

        auto envRms = [&] (const std::vector<float>& e) {
            double s = 0.0;
            for (size_t i = skip; i < e.size(); ++i) s += (double) e[i] * e[i];
            return std::sqrt (s / (double) (e.size() - skip));
        };
        if (envRms (ein) < 1.0e-4 || envRms (eout) < 1.0e-4)
            return -1.0;

        double best = -1.0e30;
        int bestLag = 0;
        for (int lag = 0; lag <= maxLag; ++lag)
        {
            double corr = 0.0, na = 0.0, nb = 0.0;
            for (size_t i = skip; i + (size_t) lag < ein.size(); ++i)
            {
                const double a = ein[i];
                const double b = eout[i + (size_t) lag];
                corr += a * b; na += a * a; nb += b * b;
            }
            const double d = std::sqrt (na * nb);
            if (d > 1.0e-12 && corr / d > best)
            {
                best = corr / d;
                bestLag = lag;
            }
        }
        return bestLag * 1000.0 / kSampleRate;
    }

    double cpuUsPerAudioSecond (Shifter& sh, const std::vector<float>& signal, float f0,
                                float ratio)
    {
        // Warm the caches/state, then time 6 seconds of audio.
        std::vector<float> out;
        processAll (sh, signal, out, ratio, f0);

        const auto t0 = std::chrono::high_resolution_clock::now();
        processAll (sh, signal, out, ratio, f0);
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double us = std::chrono::duration<double, std::micro> (t1 - t0).count();
        const double audioSec = (double) signal.size() / kSampleRate;
        return us / audioSec;
    }

    /** Max sample-to-sample jump on a 200 Hz sine with Â±1 semitone steps, after
        a warm-up. Uses the shifter's normal attack envelope (the shipped path). */
    double clickMax (Shifter& sh)
    {
        const double seconds = 3.0;
        auto sine = makeSine (200.0, seconds);
        std::vector<float> out;
        out.assign (sine.size(), 0.0f);

        std::vector<float> blockIn ((size_t) kBlock), blockOut ((size_t) kBlock);
        float ratio = 1.0f;
        for (size_t pos = 0; pos < sine.size(); pos += kBlock)
        {
            const int n = (int) std::min<size_t> ((size_t) kBlock, sine.size() - pos);
            if ((pos / kBlock) % 13 == 0)
                ratio = (ratio == 1.0f) ? 1.05946309f : 1.0f;
            for (int i = 0; i < n; ++i) blockIn[(size_t) i] = sine[pos + (size_t) i];
            sh.processBlock (blockIn.data(), blockOut.data(), n, ratio, 200.0f);
            for (int i = 0; i < n; ++i) out[pos + (size_t) i] = blockOut[(size_t) i];
        }

        float prev = 0.0f, maxJump = 0.0f;
        const size_t skip = (size_t) (0.5 * kSampleRate);
        for (size_t i = skip; i < out.size(); ++i)
        {
            maxJump = juce::jmax (maxJump, std::abs (out[i] - prev));
            prev = out[i];
        }
        return (double) maxJump;
    }

    /** Output pitch error in cents, measured with a YIN detector on the output. */
    double pitchErrorCents (const std::vector<float>& out, double expectedHz)
    {
        ovtdsp::YinPitchDetector det;
        det.prepare (kSampleRate, 2048);
        // A narrow range around the expected pitch keeps the anti-octave heuristic
        // from transposing a clean tone down an octave (see DetectorResponseTest).
        det.setFrequencyRange ((float) (expectedHz * 0.6), (float) (expectedHz * 1.8));

        std::vector<float> pitches;
        const int win = 2048;
        for (size_t pos = 0; pos + (size_t) win <= out.size(); pos += (size_t) win)
        {
            const float f = det.detectPitch (out.data() + pos, win);
            if (f > 0.0f) pitches.push_back (f);
        }
        if (pitches.empty()) return 9999.0;
        std::sort (pitches.begin(), pitches.end());
        const double median = pitches[pitches.size() / 2];
        return 1200.0 * std::log2 (median / expectedHz);
    }

    /** Log-spectral distortion against the ideal frequency scaling of the input:
        |Y(f)| should equal |X(f / ratio)|. Both spectra are mean-normalised in
        the log domain before the difference so only the SHAPE is judged. */
    double spectralDistortionDb (const std::vector<float>& in, const std::vector<float>& out,
                                 double ratio)
    {
        constexpr int fftSize = 4096;
        juce::dsp::FFT fft (12); // 2^12
        std::vector<float> window ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                         * (float) i / (float) (fftSize - 1));

        auto averageSpectrum = [&] (const std::vector<float>& v) {
            std::vector<double> mag ((size_t) fftSize / 2, 0.0);
            std::vector<float> buf ((size_t) fftSize * 2); // JUCE real-only FFT needs 2 * size
            int frames = 0;
            const size_t skip = (size_t) (0.8 * kSampleRate);
            for (size_t pos = skip; pos + (size_t) fftSize <= v.size(); pos += (size_t) (fftSize / 2))
            {
                for (int i = 0; i < fftSize; ++i) buf[(size_t) i] = v[pos + (size_t) i] * window[(size_t) i];
                fft.performFrequencyOnlyForwardTransform (buf.data());
                for (int i = 0; i < fftSize / 2; ++i) mag[(size_t) i] += (double) buf[(size_t) i];
                ++frames;
            }
            for (auto& m : mag) m = (frames > 0) ? m / (double) frames : 0.0;
            return mag;
        };

        const auto inMag = averageSpectrum (in);
        const auto outMag = averageSpectrum (out);

        const double binHz = kSampleRate / (double) fftSize;
        const int loBin = (int) std::ceil (120.0 / binHz);
        const int hiBin = (int) std::floor (8000.0 / binHz);

        double sumOut = 0.0, sumRef = 0.0;
        std::vector<double> logOut, logRef;
        for (int b = loBin; b <= hiBin; ++b)
        {
            const double f = b * binHz;
            const double srcBin = f / ratio / binHz;
            const int i0 = (int) std::floor (srcBin);
            const double frac = srcBin - (double) i0;
            if (i0 < 1 || i0 + 1 >= (int) inMag.size()) continue;
            const double ref = inMag[(size_t) i0] * (1.0 - frac) + inMag[(size_t) (i0 + 1)] * frac;

            const double lo = 20.0 * std::log10 (juce::jmax (1.0e-9, outMag[(size_t) b]));
            const double lr = 20.0 * std::log10 (juce::jmax (1.0e-9, ref));
            logOut.push_back (lo);
            logRef.push_back (lr);
            sumOut += lo;
            sumRef += lr;
        }
        if (logOut.size() < 16) return 9999.0;

        const double meanOut = sumOut / (double) logOut.size();
        const double meanRef = sumRef / (double) logRef.size();
        double acc = 0.0;
        for (size_t i = 0; i < logOut.size(); ++i)
        {
            const double d = (logOut[i] - meanOut) - (logRef[i] - meanRef);
            acc += d * d;
        }
        return std::sqrt (acc / (double) logOut.size());
    }

    struct Row
    {
        std::string label;
        int declaredMs;
        int declaredSamples;
        double effectiveMs;
        double cpuUsPerSec;
        double rmsDb;
        double click;
        double cents;
        double lsdDb;
        double warblePct;
    };

    Row measure (Shifter& sh, double f0, double ratio, const char* label, double latencyMs)
    {
        Row r {};
        r.label = label;
        sh.prepare();
        sh.setDeclaredLatencyMs (latencyMs);
        r.declaredMs = (int) std::lround (sh.declaredLatencySamples() * 1000.0 / kSampleRate);
        r.declaredSamples = sh.declaredLatencySamples();

        // Effective latency: AM sine at f0 (envelope timing, no period ambiguity).
        {
            auto am = makeAmSine (f0, 1.6);
            std::vector<float> out;
            processAll (sh, am, out, (float) ratio, (float) f0);
            r.effectiveMs = effectiveLatencyMs (am, out);
        }

        // CPU on a vowel, then the other measurements.
        {
            auto vowel = makeVowel (f0, 6.0);
            r.cpuUsPerSec = cpuUsPerAudioSecond (sh, vowel, (float) f0, (float) ratio);
        }

        {
            auto vowel = makeVowel (f0, 3.0);
            std::vector<float> out;
            processAll (sh, vowel, out, (float) ratio, (float) f0);
            const size_t skip = (size_t) (0.8 * kSampleRate);
            const double inRms = rms (vowel, skip);
            const double outRms = rms (out, skip);
            r.rmsDb = 20.0 * std::log10 (juce::jmax (1.0e-9, outRms) / juce::jmax (1.0e-9, inRms));
            r.lsdDb = spectralDistortionDb (vowel, out, ratio);

            // Warble: amplitude modulation of the corrected steady note, which the
            // averaged spectrum cannot see. A crossfaded delay-line shifter puts a
            // periodic AM at its wrap rate here; the grain OLA should be quiet.
            const auto env = envelopeOf (out);
            double meanEnv = 0.0, count = 0.0;
            for (size_t i = skip; i < env.size(); ++i) { meanEnv += env[i]; count += 1.0; }
            meanEnv /= juce::jmax (1.0, count);

            // Keep only the 4-30 Hz modulation: the corrected vowel's own harmonic
            // beating lives far above that, so what remains is the shifter's periodic
            // amplitude warble (the crossfade rate for a delay-line shifter).
            const double lp = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 30.0 / kSampleRate);
            const double hp = 1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * 4.0 / kSampleRate);
            double lpState = 0.0, hpState = 0.0, devSq = 0.0;
            for (size_t i = skip; i < env.size(); ++i)
            {
                const double d = env[i] - meanEnv;
                lpState += lp * (d - lpState);
                hpState += hp * (lpState - hpState);
                const double modulation = lpState - hpState;
                devSq += modulation * modulation;
            }
            r.warblePct = 100.0 * std::sqrt (devSq / juce::jmax (1.0, count)) / juce::jmax (1.0e-9, meanEnv);
        }

        r.click = clickMax (sh);

        {
            auto vowel = makeVowel (f0, 2.0);
            std::vector<float> out;
            processAll (sh, vowel, out, (float) ratio, (float) f0);
            r.cents = pitchErrorCents (out, f0 * ratio);
        }

        return r;
    }

    void printTable (const char* title, const std::vector<Row>& rows)
    {
        std::printf ("\n%s\n", title);
        std::printf ("  %-22s %8s %8s %9s %10s %8s %7s %8s %7s %8s\n",
                     "config", "decl.ms", "decl.smp", "eff.ms", "CPU us/s", "RMS dB",
                     "click", "cents", "LSD dB", "warble%");
        std::printf ("  %-22s %8s %8s %9s %10s %8s %7s %8s %7s %8s\n",
                     "----------------------", "--------", "--------", "---------",
                     "----------", "-------", "------", "--------", "-------", "--------");
        for (const auto& r : rows)
            std::printf ("  %-22s %8d %8d %9.2f %10.1f %+8.2f %7.3f %+8.1f %7.2f %8.1f\n",
                         r.label.c_str(), r.declaredMs, r.declaredSamples, r.effectiveMs,
                         r.cpuUsPerSec, r.rmsDb, r.click, r.cents, r.lsdDb, r.warblePct);
        std::printf ("\n");
    }
}

int main()
{
    std::printf ("OpenVoxTuner — low-latency pitch shifter benchmark\n");
    std::printf ("44.1 kHz, mono, block %d; effective latency from AM-envelope correlation\n", kBlock);

    const double ratioUp = 1.0594630943592953; // +1 semitone

    // --- 1) shipped shifter: declared latency sweep at a 200 Hz voice ---------
    std::vector<Row> psolaLatency;
    for (double ms : { 8.0, 10.0, 12.0, 20.0, 30.0 })
    {
        PsolaShifter sh;
        psolaLatency.push_back (measure (sh, 200.0, ratioUp, "PSOLA", ms));
    }
    printTable ("PSOLA: declared-latency sweep", psolaLatency);

    // --- 2) shipped shifter: pitch dependence at the 10 ms mode ---------------
    std::vector<Row> psolaPitch;
    for (double f0 : { 82.41, 120.0, 200.0, 400.0 })
    {
        PsolaShifter sh;
        psolaPitch.push_back (measure (sh, f0, ratioUp, ("PSOLA @10 ms f=" + std::to_string ((int) std::lround (f0))).c_str(), 10.0));
    }
    printTable ("PSOLA: fundamental sweep at declared 10 ms", psolaPitch);

    // --- 3) Doppler prototype --------------------------------------------------
    std::vector<Row> doppler;
    {
        DopplerShifter sh;
        doppler.push_back (measure (sh, 200.0, ratioUp, "Doppler avg 2.5ms", 2.5));
    }
    {
        DopplerShifter sh;
        doppler.push_back (measure (sh, 200.0, ratioUp, "Doppler avg 4.0ms", 4.0));
    }
    printTable ("Doppler two-tap prototype", doppler);

    // --- sanity gates ----------------------------------------------------------
    int failures = 0;
    auto check = [&] (bool ok, const char* what) {
        std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what);
        if (! ok) ++failures;
    };

    const auto& psola10 = psolaLatency[1];
    check (psola10.effectiveMs > 0.0, "PSOLA effective latency was measurable");
    check (std::abs (psola10.cents) < 25.0, "PSOLA @10 ms shifts +1 semitone within 25 cents");
    check (psola10.rmsDb > -6.0 && psola10.rmsDb < 6.0, "PSOLA @10 ms keeps the level within 6 dB");
    check (psolaLatency[3].cpuUsPerSec < 100000.0, "PSOLA CPU under 100 ms per audio second");

    const auto& dop = doppler[0];
    check (dop.effectiveMs > 0.0, "Doppler effective latency was measurable");
    check (dop.effectiveMs < psola10.effectiveMs, "Doppler is faster than the 10 ms PSOLA mode");

    std::printf ("\nRESULT: %s\n", failures == 0 ? "measurements complete" : "SANITY CHECK FAILED");
    return failures == 0 ? 0 : 1;
}
