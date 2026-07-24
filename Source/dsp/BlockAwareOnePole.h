// BlockAwareOnePole.h
// Buffer-size-independent one-pole IIR smoother for the audio callback.
//
// Purpose
// =======
// Several audio parameters need to be smoothed every audio block (FlexTune
// multiplier, Humanize random walk, etc.). The naive form
//
//     y = y * 0.95f + x * 0.05f;            // WRONG at small buffers
//
// is buffer-size dependent: the time constant tau = 1/alpha samples is
// multiplied by the buffer length, so the same alpha behaves TWICE as fast
// at 128 samples (2.9 ms @ 44.1 kHz) as it does at 256 samples (5.8 ms).
// This makes the smoothed parameter audibly inconsistent across buffer
// sizes and is a documented cause of glitches when the user switches from
// a 512-sample buffer (e.g. while monitoring) to a 128-sample buffer
// (e.g. for low-latency tracking) without re-tuning the plugin.
//
// This class applies a single IIR step per BLOCK, but with an alpha
// equivalent to N successive per-sample steps. With tau in seconds and
// blockDur = numSamples / sampleRate in seconds, the formula is:
//
//     alpha_block = 1 - exp(-blockDur / tau)
//
// which gives an effective time constant tau that is INDEPENDENT of the
// block size (just like RetargetEnvelope::processBlock).
//
// Skip-when-disabled
// ==================
// When the input x is equal to the current value (e.g. FlexTune is off
// and the multiplier is 1.0, or Humanize is at 0 cents), this helper
// still updates the state to avoid latent error if x changes later.
// However, callers can also short-circuit by checking their own
// enable flag and skipping processBlock() entirely to save a few cycles
// on every audio block.
//
// Performance
// ===========
// One processBlock() call is one transcendental (std::exp) plus one
// multiply + add. The exp is required because alpha must depend on the
// actual block duration. We could cache it, but the duration changes
// at every block on hosts that re-negotiate buffer size, so caching is
// not worth the complexity here.

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
