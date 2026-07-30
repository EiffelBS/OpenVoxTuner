// BlockAwareOnePole.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <algorithm>

namespace ovtdsp
{
    class BlockAwareOnePole
    {
    public:
        BlockAwareOnePole() = default;

        /** Initialise state. Call from prepareToPlay() to reset the smoother
         *  and remember the sample rate so processBlock() can compute the
         *  block-aware alpha. */
        void prepare (double sr) noexcept
        {
            sampleRate = (sr > 0.0) ? sr : 44100.0;
            reset();
        }

        /** Reset the smoother state to a known value. Call from reset() or
         *  when the parameter stream is interrupted (preset change, bypass). */
        void reset (float initial = 1.0f) noexcept
        {
            currentValue = initial;
            currentAlpha = 0.0f;   // recomputed on next processBlock
        }

        /** Set the desired time constant in seconds. tau = 0 means
         *  "instant response" (the smoothed value snaps to the target). */
        void setTimeConstantSeconds (float sec) noexcept
        {
            // 2026-07-23 (Fix AY): use juce::jmax instead of std::max
            // to avoid the well-known Windows macro conflict (when
            // <windows.h> is transitively included, the `max` macro
            // is defined and breaks `std::max(0.0f, sec)`).
            tauSeconds = juce::jmax (0.0f, sec);
        }

        /** Force the smoother to a specific value without smoothing.
         *  Useful to seed the smoother on a preset change. */
        void snapTo (float v) noexcept
        {
            currentValue = v;
            currentAlpha = 1.0f;
        }

        /** Get the last computed smoothed value. */
        float getCurrentValue() const noexcept { return currentValue; }

        /** Process one block of audio: apply a single IIR step whose alpha
         *  is computed from the block duration so that the effective time
         *  constant (in seconds) is INDEPENDENT of the buffer size.
         *
         *  @param target      the new target value for this block
         *  @param numSamples  number of samples in the block
         *  @return            the smoothed value applied to this block
         */
        float processBlock (float target, int numSamples) noexcept
        {
            if (tauSeconds <= 0.0f || numSamples <= 0)
            {
                currentValue = target;
                currentAlpha = 1.0f;
                return currentValue;
            }

            const double blockDurSec = static_cast<double> (numSamples) / sampleRate;
            // alpha_block = 1 - exp(-blockDur / tau) is mathematically
            // equivalent to N successive per-sample steps with alpha_sample
            // = 1 - exp(-1 / (tau * sampleRate)). See RetargetEnvelope.cpp
            // for the full derivation.
            currentAlpha = static_cast<float> (1.0 - std::exp (-blockDurSec / tauSeconds));
            currentValue += currentAlpha * (target - currentValue);
            return currentValue;
        }

        /** Convenience: skip smoothing and just clamp/return the target.
         *  Used by callers that want to bypass the smoother for the
         *  current block (e.g. when the feature is disabled). */
        float processBypassed (float target) noexcept
        {
            currentValue = target;
            currentAlpha = 1.0f;
            return currentValue;
        }

        /** 2026-07-23 (Fix AW): single-step IIR for callers that have a
         *  per-block target (rather than a per-sample target). The
         *  block duration in seconds is used directly to compute the
         *  alpha, so the time constant is independent of the buffer
         *  size. This is the same arithmetic as processBlock() but
         *  with a scalar (1-element) target.
         *
         *  Useful when an external system updates a single value per
         *  audio block (e.g. AttackAwareEnv's per-block output feeding
         *  into the PitchShifter's attack-gain multiplier). */
        float step (float target, float blockDurSec) noexcept
        {
            if (tauSeconds <= 0.0f || blockDurSec <= 0.0f)
            {
                currentValue = target;
                currentAlpha = 1.0f;
                return currentValue;
            }
            const float alpha = static_cast<float> (1.0 - std::exp (-blockDurSec / tauSeconds));
            currentAlpha = alpha;
            currentValue += alpha * (target - currentValue);
            return currentValue;
        }

    private:
        double sampleRate = 44100.0;
        float  tauSeconds = 0.0f;
        float  currentValue = 1.0f;
        float  currentAlpha = 1.0f;
    };
}



