// RetargetEnvelope.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    class RetargetEnvelope
    {
    public:
        RetargetEnvelope();
        ~RetargetEnvelope();

        void prepare (double sampleRate);
        void reset();

        /// Sets the retargeting time in milliseconds.
        void setSpeed (float ms);

        /// Processes one ratio sample and returns the smoothed ratio.
        /// @param targetRatio  target ratio (computed by the quantizer)
        /// @return             ratio applied after smoothing
        float processSample (float targetRatio);

        /// "Block" variant: applies the smoothing while accounting for the
        /// number of samples in the block, so the time constant is
        /// INDEPENDENT of the buffer size.
        /// Without this, calling processSample() once per block with a
        /// per-sample alpha gives an effective response time of tau*numSamples
        /// (7.2s at 144 samples @ 44.1kHz for speed=50ms -> Speed has almost
        /// no effect at small buffer sizes).
        /// @param targetRatio  target ratio
        /// @param numSamples   audio block size
        /// @return             smoothed ratio applied for this block
        float processBlock (float targetRatio, int numSamples);

    private:
        double sampleRate = 44100.0;
        float speedMs = 50.0f;
        float currentValue = 1.0f;
        float alpha = 1.0f; // IIR filter coefficient (0 = no change, 1 = instantaneous)

        // Recomputes alpha from speedMs and sampleRate.
        void recomputeAlpha();
    };
}



