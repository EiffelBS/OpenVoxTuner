// audio_processor.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: accumulates audio frames into a sliding analysis window,
// preprocesses, extracts chroma and produces a pitch-class set. Not part of
// the public API.

#pragma once

#include "audio/chroma.h"
#include "audio/preprocess.h"
#include <vector>

namespace ovtchord
{
    class AudioProcessor
    {
    public:
        struct Config
        {
            int windowSize = 8192;          // analysis window (samples)
            int hopSize = 1024;             // hop between analyses
            double sampleRate = 44100.0;
            float chromaThreshold = 0.35f;  // chroma bin cutoff (rel. to max)
        };

        explicit AudioProcessor (const Config& cfg = Config());

        void setSampleRate (double sampleRate);

        // Feed a frame; returns true when a new pitch-class set was produced
        // (available via lastPitchClasses()).
        bool process (const float* samples, int numSamples);

        const std::vector<int>& lastPitchClasses() const { return pcs; }

        void reset();

    private:
        Config cfg;
        AudioPreprocessor pre;
        ChromaExtractor chroma;
        std::vector<float> fifo;
        int fifoCount = 0;
        std::vector<float> analysisWindow;
        std::vector<int> pcs;
        // Re-usable scratch buffers (no heap allocation on the real-time path).
        std::vector<float> workFrame;  // preprocessed input frame (per block)
        std::vector<float> chromaVec;  // 12-bin chroma output
    };
}