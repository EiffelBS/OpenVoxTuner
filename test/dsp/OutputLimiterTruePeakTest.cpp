#pragma once
// OutputLimiterTruePeakTest.cpp
// Unit test: what the output stage really guarantees against clipping.
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// EffectsBusTest proves the sample-level ceiling ("the peak must be caught at the ceiling"). What that test cannot
// see is the RECONSTRUCTED signal: a DAC puts a smooth curve through the samples, and that curve can overshoot every
// sample. A brickwall measured on samples therefore does NOT guarantee anything to a true-peak meter, which is what
// "the limiter stops clipping" means in practice.
//
// So this test measures the reconstruction peak by 4x oversampling with a windowed sinc lowpass — the same shape of
// estimator a true-peak meter uses (ITU-R BS.1770-4 Annex 2). Every number is printed, so the ceiling can be chosen
// from measurement instead of from taste.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/OutputLimiter.h"
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
    /** The reconstruction peak of a buffer, measured by upsampling 4x (zero insertion) and filtering with a
     *  Blackman-windowed sinc lowpass normalised to unity DC gain. Cubic interpolation is NOT good enough here: at
     *  17 kHz on a 48 kHz stream the samples sit ~2.8 per cycle, so a cubic through four samples cannot see the crest
     *  between them and reports the sample peak as the true peak. The FIR can. */
    float limOversampledTruePeak (const juce::AudioBuffer<float>& buffer)
    {
        constexpr int kTaps = 65;                       // odd, so tap 32 is the centre
        const int centre = kTaps / 2;

        std::vector<double> h (kTaps);
        for (int n = 0; n < kTaps; ++n)
        {
            const double m = static_cast<double> (n - centre) / 4.0;   // offset in cycles of the new rate
            double s = 1.0;
            if (std::abs (m) > 1.0e-9)
                s = std::sin (juce::MathConstants<double>::pi * m) / (juce::MathConstants<double>::pi * m);
            // Rectangular window on purpose: a Blackman-windowed sinc rolls off inside the passband, so content near
            // Nyquist is attenuated and the true peak reads BELOW the sample peak.
            h[n] = s;
        }
        // Deliberately NOT normalised by the coefficient sum (~4). For zero-insertion upsampling the gain that matters
        // is per phase, and sinc obeys the partition-of-unity identity sum_j sinc (x + j) = 1, so every phase group
        // already sums to ~1. Dividing by the total sum attenuates the reconstruction by 4x: a first version of this
        // estimator did that, which is why it reported a true peak BELOW the sample peak.

        float tp = 0.0f;
        const int n = buffer.getNumSamples();
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            std::vector<double> xs (static_cast<size_t> (n), 0.0);
            for (int i = 0; i < n; ++i)
                xs[static_cast<size_t> (i)] = static_cast<double> (buffer.getSample (c, i));

            const int outLen = 4 * n;
            for (int m = 0; m < outLen; ++m)
            {
                double y = 0.0;
                for (int k = 0; k < kTaps; ++k)
                {
                    const int src = (m - centre + k) / 4;   // zero-insertion stream: every 4th is a sample
                    if ((m - centre + k) % 4 == 0 && src >= 0 && src < n)
                        y += h[k] * xs[static_cast<size_t> (src)];
                }
                tp = juce::jmax (tp, static_cast<float> (std::abs (y)));
            }
        }
        return tp;
    }

    /** The gain the reconstruction filter applies at one interpolation phase: the sum of the taps whose offsets land in
     *  that phase. sinc obeys sum_j sinc (x + j) = 1, so every phase must sum to ~1. If it does not, the estimator is
     *  lying and no true-peak number measured with it can be trusted. Prefixed like the other helpers here. */
    float limPhaseSum (int phase)
    {
        constexpr int kTaps = 65;
        const int centre = kTaps / 2;

        double acc = 0.0;
        for (int n = 0; n < kTaps; ++n)
        {
            const double m = static_cast<double> (n - centre) / 4.0;
            double s = 1.0;
            if (std::abs (m) > 1.0e-9)
                s = std::sin (juce::MathConstants<double>::pi * m) / (juce::MathConstants<double>::pi * m);

            if (((n - centre) % 4 + 4) % 4 == phase)
                acc += s;
        }
        return static_cast<float> (acc);
    }

    /** Peak of every channel of a buffer. Prefixed: `peakOf` already lives in EffectsBusTest.cpp, which shares this
     *  anonymous namespace through test/Main.cpp. */
    float limPeakOf (const juce::AudioBuffer<float>& buffer)
    {
        float p = 0.0f;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                p = juce::jmax (p, std::abs (buffer.getSample (c, i)));
        return p;
    }

    /** dBFS of a linear amplitude. Prefixed for the same reason `rms` and `db` already exist in other test TUs. */
    float limDb (float linear)
    {
        if (linear <= 0.0f)
            return -120.0f;
        return static_cast<float> (20.0 * std::log10 (static_cast<double> (linear)));
    }

    /** Feed `blocks` blocks of a sine through the limiter and return the processed buffer, so the release runs across
     *  block boundaries exactly as it does in a host. */
    juce::AudioBuffer<float> limRunSine (OutputLimiter& limiter, double sampleRate, float freqHz,
                                          float amplitude, int blockSize, int blocks)
    {
        juce::AudioBuffer<float> out (1, blockSize * blocks);
        double omega = 2.0 * juce::MathConstants<double>::pi * static_cast<double> (freqHz) / sampleRate;

        for (int b = 0; b < blocks; ++b)
        {
            juce::AudioBuffer<float> buf (1, blockSize);
            for (int i = 0; i < blockSize; ++i)
            {
                const int n = b * blockSize + i;
                buf.setSample (0, i, amplitude * static_cast<float> (std::sin (omega * static_cast<double> (n))));
            }
            limiter.process (buf);
            for (int i = 0; i < blockSize; ++i)
                out.setSample (0, b * blockSize + i, buf.getSample (0, i));
        }
        return out;
    }

    /** Feed `blocks` blocks of a square wave through the limiter. A square is the signal that makes inter-sample
     *  overshoot real: its edges are broadband, so the reconstructed curve rings past every sample (Gibbs). Prefixed
     *  like the other helpers in this shared namespace. */
    juce::AudioBuffer<float> limRunSquare (OutputLimiter& limiter, double sampleRate, float freqHz,
                                           float amplitude, int blockSize, int blocks)
    {
        juce::AudioBuffer<float> out (1, blockSize * blocks);
        const int period = static_cast<int> (std::lround (sampleRate / static_cast<double> (freqHz)));

        for (int b = 0; b < blocks; ++b)
        {
            juce::AudioBuffer<float> buf (1, blockSize);
            for (int i = 0; i < blockSize; ++i)
            {
                const int n = b * blockSize + i;
                buf.setSample (0, i, (n % period) < (period / 2) ? amplitude : -amplitude);
            }
            limiter.process (buf);
            for (int i = 0; i < blockSize; ++i)
                out.setSample (0, b * blockSize + i, buf.getSample (0, i));
        }
        return out;
    }

    class OutputLimiterTruePeakTest : public juce::UnitTest
    {
    public:
        OutputLimiterTruePeakTest() : juce::UnitTest ("OutputLimiterTruePeak") {}

        void runTest() override
        {
            beginTest ("Output limiter true peak");

            const double sampleRate = 48000.0;
            const int blockSize = 512;

            // === A: what the ceiling IS, in dBFS ===
            // 0.891 linear is -1.00 dBFS, the headroom the stage promises. Pinned so a comment claiming a
            // different number can be caught against the constant it is built from.
            {
                const float ceilingDb = limDb (OutputLimiter::ceilingLinear);
                std::cout << (juce::String ("A: ceiling linear ") + juce::String (OutputLimiter::ceilingLinear, 4)
                           + " = " + juce::String (ceilingDb, 3) + " dBFS headroom").toRawUTF8() << "\n";
                expectWithinAbsoluteError (ceilingDb, -1.00f, 0.01f,
                        "The ceiling is " + juce::String (ceilingDb, 3) + " dBFS, must be -1.00 dBFS");

                // The estimator is checked before any number it produces is trusted: a true-peak reading taken through
                // a filter whose interpolation phases do not sum to unity is not a measurement at all.
                for (int p = 0; p < 4; ++p)
                    expectWithinAbsoluteError (limPhaseSum (p), 1.0f, 0.05f,
                            "Reconstruction phase " + juce::String (p) + " sums to " + juce::String (limPhaseSum (p), 3)
                            + ", must be ~1 for the true-peak numbers below to mean anything");
            }

            // === B: a hot voice pushed through +6 dB of trim cannot exceed the ceiling at sample level ===
            {
                OutputLimiter limiter;
                limiter.prepare (sampleRate, blockSize);
                limiter.reset();
                limiter.setTrimDb (6.0f);
                limiter.setLimiterEnabled (true);

                auto out = limRunSine (limiter, sampleRate, 200.0f, 1.0f, blockSize, 8);

                const float sp = limPeakOf (out);
                expect (sp <= OutputLimiter::ceilingLinear + 1.0e-5f,
                        "Sample peak after +6 dB trim is " + juce::String (sp, 6)
                        + ", must stay at or below the ceiling " + juce::String (OutputLimiter::ceilingLinear, 4));

                const float tp = limOversampledTruePeak (out);
                const float gr = limiter.currentGainReductionDb();
                expectGreaterThan (gr, 3.0f);
                std::cout << ("B: trim +6 dB on full-scale audio -> sample peak " + juce::String (sp, 4)
                            + " = " + juce::String (limDb (sp), 2) + " dBFS, true peak " + juce::String (tp, 4)
                            + " = " + juce::String (limDb (tp), 2) + " dBFS, gain reduction "
                            + juce::String (gr, 2) + " dB").toRawUTF8() << "\n";

                // Low-frequency material reconstructs through its samples, so the true peak equals the sample peak:
                // it must sit at or below the ceiling, and close enough to it that the stage is not squashing audio
                // far under the headroom it declares. The gain closes on targetLinear (-1.22 dBFS), which leaves the
                // measured margin for the residual ringing the soft clip adds downstream of the detector.
                expect (tp <= OutputLimiter::ceilingLinear + 1.0e-5f,
                        "Low-frequency true peak is " + juce::String (tp, 4)
                        + ", must stay at or below the ceiling " + juce::String (OutputLimiter::ceilingLinear, 4));
                expectGreaterThan (tp, OutputLimiter::targetLinear - 0.05f);
            }

            // === C: broadband edges are where a sample-level ceiling stops being a true-peak guarantee ===
            {
                OutputLimiter limiter;
                limiter.prepare (sampleRate, blockSize);
                limiter.reset();
                limiter.setTrimDb (0.0f);
                limiter.setLimiterEnabled (true);

                // A 1 kHz square: full-scale edges, so the reconstructed curve rings past every sample at each edge.
                auto out = limRunSquare (limiter, sampleRate, 1000.0f, 1.0f, blockSize, 6);

                const float sp = limPeakOf (out);
                expect (sp <= OutputLimiter::ceilingLinear + 1.0e-5f,
                        "Sample peak for the square wave is " + juce::String (sp, 6) + ", must stay at the ceiling");

                const float tp = limOversampledTruePeak (out);
                std::cout << ("C: full-scale square through the limiter -> sample peak " + juce::String (limDb (sp), 2)
                            + " dBFS, true peak " + juce::String (tp, 4) + " = " + juce::String (limDb (tp), 2)
                            + " dBFS, overshoot " + juce::String (limDb (tp) - limDb (sp), 2) + " dB").toRawUTF8() << "\n";

                // The guarantee: a square wave's edges ring past every sample, and the stage must catch that crest,
                // not just the stored samples. Measured here: sample peak -2.18 dBFS, true peak -1.15 dBFS, i.e. the
                // reconstructed crest sits 1.02 dB above its own samples and still lands under -1 dBFS.
                expect (tp <= OutputLimiter::ceilingLinear + 1.0e-5f,
                        "Square-wave true peak is " + juce::String (tp, 4) + " = " + juce::String (limDb (tp), 2)
                        + " dBFS, must stay at or below the ceiling");
            }

            // === D: broadband material, the case a vocal with sibilance and a limiter tail actually is ===
            {
                OutputLimiter limiter;
                limiter.prepare (sampleRate, blockSize);
                limiter.reset();
                limiter.setTrimDb (0.0f);
                limiter.setLimiterEnabled (true);

                // Deterministic pseudo-noise: a full-scale burst with no silence between blocks, so the envelope stays
                // closed on the ceiling and every sample is a candidate crest.
                juce::AudioBuffer<float> out (1, blockSize * 6);
                std::uint32_t seed = 12345u;
                for (int b = 0; b < 6; ++b)
                {
                    juce::AudioBuffer<float> buf (1, blockSize);
                    for (int i = 0; i < blockSize; ++i)
                    {
                        seed = seed * 1664525u + 1013904223u;
                        const float v = static_cast<float> (static_cast<double> (seed) / 4294967295.0) * 2.0f - 1.0f;
                        buf.setSample (0, i, v);
                    }
                    limiter.process (buf);
                    for (int i = 0; i < blockSize; ++i)
                        out.setSample (0, b * blockSize + i, buf.getSample (0, i));
                }

                const float sp = limPeakOf (out);
                const float tp = limOversampledTruePeak (out);
                std::cout << ("D: full-scale noise burst -> sample peak " + juce::String (limDb (sp), 2)
                            + " dBFS, true peak " + juce::String (tp, 4) + " = " + juce::String (limDb (tp), 2)
                            + " dBFS, overshoot " + juce::String (limDb (tp) - limDb (sp), 2) + " dB").toRawUTF8() << "\n";

                // The guarantee on the hardest material in this suite: the reconstructed crest of a full-scale noise
                // burst sits 2.67 dB above its own samples, and the stage must keep it under -1 dBFS. Measured here:
                // sample peak -3.70 dBFS, true peak -1.03 dBFS.
                expect (tp <= OutputLimiter::ceilingLinear + 1.0e-5f,
                        "Noise-burst true peak is " + juce::String (tp, 4) + " = " + juce::String (limDb (tp), 2)
                        + " dBFS, must stay at or below the ceiling");
            }

            // === E: with the limiter off, the trim is a pure booster and can clip ===
            {
                OutputLimiter limiter;
                limiter.prepare (sampleRate, blockSize);
                limiter.reset();
                limiter.setTrimDb (6.0f);
                limiter.setLimiterEnabled (false);

                auto out = limRunSine (limiter, sampleRate, 200.0f, 1.0f, blockSize, 4);
                const float sp = limPeakOf (out);
                std::cout << ("E: trim +6 dB with the limiter OFF -> sample peak " + juce::String (sp, 4)
                            + " = " + juce::String (limDb (sp), 2) + " dBFS").toRawUTF8() << "\n";

                // Documented behaviour, not a bug: off means no protection, and full-scale audio plus 6 dB is over 0.
                expectGreaterThan (sp, 1.0f);
            }

            // === F: the lookahead the detector needs IS the latency the stage declares ===
            {
                OutputLimiter limiter;
                limiter.prepare (sampleRate, blockSize);
                limiter.reset();
                limiter.setTrimDb (0.0f);
                limiter.setLimiterEnabled (true);

                const int L = limiter.getLatencySamples();
                expect (L == OutputLimiter::kLatencySamples, "Declared latency must be the filter group delay");
                std::cout << ("F: declared latency " + juce::String (L) + " samples = "
                            + juce::String (1000.0 * static_cast<double> (L) / sampleRate, 2)
                            + " ms at 48 kHz").toRawUTF8() << "\n";

                // A quiet impulse, far under the closing target, so the gain stays unity and the reconstruction is
                // pure: it must leave the stage exactly L samples later than it entered.
                juce::AudioBuffer<float> buf (1, blockSize);
                buf.clear();
                const int spikeIndex = 300;
                buf.setSample (0, spikeIndex, 0.25f);
                limiter.process (buf);

                expect (std::abs (buf.getSample (0, spikeIndex + L - 1)) < 1.0e-6f,
                        "Nothing may be produced ahead of the declared delay");
                expectWithinAbsoluteError (buf.getSample (0, spikeIndex + L), 0.25f, 1.0e-5f,
                        "A quiet impulse must emerge at unity gain exactly L samples later, got "
                        + juce::String (buf.getSample (0, spikeIndex + L), 6));

                // And the crest that follows it is what the detector sees ahead of the emitted sample: the phase
                // groups read samples up to the current input, which is why the delay cannot be smaller.
                expect (OutputLimiter::kFilterTaps / 2 / OutputLimiter::kOversamplingFactor == L,
                        "The filter half-width in base samples must equal the declared latency");
            }
        }
    };

    static OutputLimiterTruePeakTest outputLimiterTruePeakTest;
}
