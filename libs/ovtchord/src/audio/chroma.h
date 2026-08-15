// chroma.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: FFT-based chroma (pitch-class) extraction. Not part of the
// public API.

#pragma once

#include "audio/fft.h"
#include <complex>
#include <vector>

namespace ovtchord
{
    // Extracts a 12-bin chroma vector from a window of audio samples.
    class ChromaExtractor
    {
    public:
        struct Config
        {
            int fftSize = 4096;
            float a4Hz = 440.0f;
            float lowFreqHz = 80.0f;    // frequency range considered
            float highFreqHz = 4000.0f;
        };

        // NOTE: see chord_engine.h — no in-class default argument using Config()
        // (Clang: "default member initializer needed within the definition of the
        // enclosing class"). The no-arg ctor delegates instead.
        explicit ChromaExtractor();
        explicit ChromaExtractor (const Config& cfg);

        // Computes the 12-bin chroma (normalized so the largest bin == 1) and
        // writes it into `out` (must be pre-allocated to 12). No heap
        // allocation on the real-time path: scratch buffers are members.
        void extract (const float* samples, int numSamples, double sampleRate,
                      std::vector<float>& out);

        // Derives a pitch-class set from a chroma vector: bins whose value is
        // >= (maxBin * threshold) are kept. Writes into `out` (no allocation).
        static void pitchClassSetFromChroma (const std::vector<float>& chroma,
                                             float threshold,
                                             std::vector<int>& out);

    private:
        Config cfg;
        FFT fft;
        std::vector<std::complex<float>> spectrum;
        std::vector<float> window; // re-usable Hann window
        std::vector<float> mag;    // re-usable magnitude scratch (size = fftSize/2)
    };
}