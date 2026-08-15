// chroma.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "audio/chroma.h"
#include <cmath>

namespace ovtchord
{
    namespace
    {
        int ilog2 (int v) { int l = 0; while ((1 << l) < v) ++l; return l; }
    }

    ChromaExtractor::ChromaExtractor() : ChromaExtractor (Config()) {}

    ChromaExtractor::ChromaExtractor (const Config& cfg)
        : cfg (cfg), fft (ilog2 (cfg.fftSize))
    {
        spectrum.resize (static_cast<std::size_t> (cfg.fftSize));
        mag.resize (static_cast<std::size_t> (cfg.fftSize / 2));
    }

    void ChromaExtractor::extract (const float* samples, int numSamples, double sampleRate,
                                   std::vector<float>& out)
    {
        const int n = cfg.fftSize;
        if (numSamples < n) numSamples = n;

        // Build the Hann window on first use.
        if (static_cast<int> (window.size()) != n)
        {
            window.resize (n);
            for (int i = 0; i < n; ++i)
                window[i] = 0.5f * (1.0f - std::cos (2.0f * static_cast<float> (std::acos (-1.0)) * i / n));
        }

        // Apply window + copy into the spectrum (real part).
        const int offset = numSamples - n;
        for (int i = 0; i < n; ++i)
            spectrum[i] = std::complex<float> (samples[offset + i] * window[i], 0.0f);

        fft.forward (spectrum);

        // Compute magnitudes and find the global peak (for a noise floor).
        const int half = n / 2;
        float globalMax = 0.0f;
        for (int b = 1; b < half; ++b)
        {
            mag[static_cast<std::size_t> (b)] = std::abs (spectrum[static_cast<std::size_t> (b)]);
            if (mag[static_cast<std::size_t> (b)] > globalMax) globalMax = mag[static_cast<std::size_t> (b)];
        }
        const float peakThresh = globalMax * 0.1f;

        // Peak-pick local maxima, refine with parabolic interpolation, and fold
        // each peak (plus its harmonics) into the pitch-class chroma.
        // (Peak picking rejects spectral leakage from the Hann window.)
        float chroma[12] = { 0.0f };
        const float binHz = (cfg.a4Hz == 0.0f) ? 440.0f : cfg.a4Hz;
        const double sr = (sampleRate > 0.0) ? sampleRate : 44100.0;
        for (int b = 1; b < half - 1; ++b)
        {
            const float m0 = mag[static_cast<std::size_t> (b - 1)];
            const float m1 = mag[static_cast<std::size_t> (b)];
            const float m2 = mag[static_cast<std::size_t> (b + 1)];
            if (m1 <= m0 || m1 < m2 || m1 < peakThresh)
                continue;

            // Parabolic interpolation for sub-bin frequency accuracy. Critical
            // at low frequencies where one FFT bin can exceed a semitone
            // (e.g. 4096 samples @ 44.1 kHz -> ~10.8 Hz/bin, ~0.5 semitone at
            // 155 Hz), which would otherwise misclassify notes near a bin edge.
            const float denom = m0 - 2.0f * m1 + m2;
            float delta = 0.0f;
            if (std::abs (denom) > 1.0e-12f)
                delta = 0.5f * (m0 - m2) / denom;
            const float bin = static_cast<float> (b) + delta;
            const float freq = static_cast<float> (bin * sr / static_cast<double> (n));
            if (freq < cfg.lowFreqHz || freq > cfg.highFreqHz)
                continue;

            // Harmonic summation: fold the fundamental and its first harmonics
            // into the chroma (weighted 1, 1/2, 1/3, 1/4). This reinforces the
            // pitch class and tolerates missing fundamentals in real audio.
            const float weights[4] = { 1.0f, 0.5f, 0.33f, 0.25f };
            for (int h = 0; h < 4; ++h)
            {
                const float hf = freq * static_cast<float> (h + 1);
                if (hf > cfg.highFreqHz)
                    break;
                float midi = 69.0f + 12.0f * std::log2 (hf / binHz);
                int pc = static_cast<int> (std::lround (midi)) % 12;
                if (pc < 0) pc += 12;
                chroma[pc] += m1 * weights[h];
            }
        }

        // Normalize by the max bin.
        float maxBin = 0.0f;
        for (int i = 0; i < 12; ++i) if (chroma[i] > maxBin) maxBin = chroma[i];
        out.assign (12, 0.0f);
        if (maxBin > 1.0e-9f)
            for (int i = 0; i < 12; ++i) out[i] = chroma[i] / maxBin;
    }

    void ChromaExtractor::pitchClassSetFromChroma (const std::vector<float>& chroma,
                                                   float threshold,
                                                   std::vector<int>& out)
    {
        float maxBin = 0.0f;
        for (float v : chroma) if (v > maxBin) maxBin = v;
        const float cutoff = maxBin * threshold;

        out.clear();
        for (int i = 0; i < 12; ++i)
            if (chroma[i] >= cutoff)
                out.push_back (i);
    }
}