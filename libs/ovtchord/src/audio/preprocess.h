// preprocess.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: bandpass + noise gate + gentle AGC. Not part of the public
// API.

#pragma once

#include <vector>

namespace ovtchord
{
    // Simple preprocessing: 2nd-order Butterworth bandpass (HP + LP cascade),
    // a noise gate (skips silent frames) and a smoothed-peak AGC.
    class AudioPreprocessor
    {
    public:
        struct Config
        {
            float lowFreqHz = 80.0f;
            float highFreqHz = 4000.0f;
            float gateThreshold = 1.0e-3f; // peak below this = silence
            float agcTarget = 0.5f;        // normalize toward this peak
        };

        // NOTE: see chord_engine.h — no in-class default argument using Config()
        // (Clang: "default member initializer needed within the definition of the
        // enclosing class"). The no-arg ctor delegates instead.
        explicit AudioPreprocessor();
        explicit AudioPreprocessor (const Config& cfg);

        void setSampleRate (double sampleRate);

        // In-place. Returns false if the frame was gated (kept as-is).
        bool process (std::vector<float>& samples);

        void reset();

    private:
        void updateCoeffs();

        double sr = 44100.0;
        Config cfg;
        // HP and LP biquad coefficients.
        double hp0=1, hp1=0, hp2=0, hpa1=0, hpa2=0;
        double lp0=1, lp1=0, lp2=0, lpa1=0, lpa2=0;
        // HP state.
        double hx1=0, hx2=0, hy1=0, hy2=0;
        // LP state.
        double lx1=0, lx2=0, ly1=0, ly2=0;
        double smoothedPeak = 0.0f;
        bool coeffsValid = false;
    };
}