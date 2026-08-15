// audio_processor.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "audio/audio_processor.h"
#include <algorithm>

namespace ovtchord
{
    AudioProcessor::AudioProcessor() : AudioProcessor (Config()) {}

    AudioProcessor::AudioProcessor (const Config& cfg)
        : cfg (cfg),
          chroma ([&]() {
              ChromaExtractor::Config c;
              c.fftSize = cfg.windowSize;
              return c;
          }())
    {
        pre.setSampleRate (cfg.sampleRate);
        // Preallocate the accumulation buffer (window + headroom for hops).
        fifo.resize (static_cast<std::size_t> (cfg.windowSize + cfg.hopSize * 4), 0.0f);
        analysisWindow.resize (static_cast<std::size_t> (cfg.windowSize), 0.0f);
        // Preallocate the real-time scratch buffers (no per-block allocation).
        workFrame.reserve (static_cast<std::size_t> (cfg.windowSize));
        chromaVec.resize (12);
    }

    void AudioProcessor::setSampleRate (double sampleRate)
    {
        if (sampleRate > 0.0)
        {
            cfg.sampleRate = sampleRate;
            pre.setSampleRate (sampleRate);
        }
    }

    void AudioProcessor::reset()
    {
        fifoCount = 0;
        pcs.clear();
        pre.reset();
    }

    bool AudioProcessor::process (const float* samples, int numSamples)
    {
        if (samples == nullptr || numSamples <= 0)
            return false;

        // Preprocess the incoming frame (continuous filter state). Reuses the
        // pre-allocated workFrame (no heap allocation per block).
        workFrame.assign (samples, samples + numSamples);
        pre.process (workFrame);

        // Append to the FIFO.
        if (fifoCount + numSamples > static_cast<int> (fifo.size()))
            fifo.resize (static_cast<std::size_t> (fifoCount + numSamples));
        std::copy (workFrame.begin(), workFrame.end(), fifo.begin() + fifoCount);
        fifoCount += numSamples;

        bool produced = false;
        while (fifoCount >= cfg.windowSize)
        {
            // Copy the latest window.
            std::copy (fifo.begin() + (fifoCount - cfg.windowSize), fifo.begin() + fifoCount,
                       analysisWindow.begin());

            chroma.extract (analysisWindow.data(), cfg.windowSize, cfg.sampleRate, chromaVec);
            ChromaExtractor::pitchClassSetFromChroma (chromaVec, cfg.chromaThreshold, pcs);
            produced = true;

            // Drop the hop from the front (sliding window).
            std::copy (fifo.begin() + cfg.hopSize, fifo.begin() + fifoCount, fifo.begin());
            fifoCount -= cfg.hopSize;
        }
        return produced;
    }
}