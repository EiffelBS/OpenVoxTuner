// AttackAwareEnv.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>

namespace ovtdsp
{
    class AttackAwareEnv
    {
    public:
        /** Set the release time (seconds): how long the correction takes to
         *  ramp back from 0 (onset) to full (1) after the attack.
         *  Semantics: this is the IIR time constant (the time to reach
         *  ~63% of the way back to 1). The total time to settle to within
         *  5% of 1 is ~3 * releaseSec. Minimum is 5 ms to keep alpha in
         *  a safe range. */
        void setReleaseSeconds (float sec) noexcept { releaseSec = juce::jmax (0.005f, sec); }

        /** Enable / disable attack-aware correction. When disabled the helper
         *  always returns a gain of 1.0 (no effect on the correction). */
        void setEnabled (bool e) noexcept
        {
            enabled = e;
            if (!enabled)
                attackGain = 1.0f; // transparent when off
        }

        bool isEnabled() const noexcept { return enabled; }

        /** Reset the internal state (e.g. on transport seek / preset load). */
        void reset() noexcept
        {
            attackGain = 1.0f;
            slowEnv = 0.0f;
            prevRms = 0.0f;
        }

        /** Process one analysis block.
         *  @param blockRms     RMS level of the input block (0..1).
         *  @param blockDurSec  Duration of one block in seconds (numSamples / sampleRate).
         *  @return correction gain in [0, 1]; multiply into the correction amount.
         */
        float process (float blockRms, float blockDurSec) noexcept
        {
            if (!enabled)
            {
                prevRms = blockRms;
                return 1.0f;
            }

            // Onset = a rising edge of the level that also clearly exceeds the
            // slow-following envelope (so a slow swell does not count as an onset).
            // The slowEnv > kMinLevel guard avoids a false onset on the very first
            // block after reset/start, when slowEnv is still ~0.
            //
            // 2026-07-23 (Fix AX â€” "silence instead of scratch" bug): the
            // original check also fired on EVERY rising block during a
            // sustained attack (e.g. a singer's note attack is 5-15 blocks
            // of continuously rising level). With the IIR ramp, each
            // onset resets attackGain to 0, so the gain stayed at 0 for
            // the entire attack and the output was silent instead of
            // scratchy. The fix is to add a "ready" guard: only fire an
            // onset when attackGain is already at (or very close to) 1.0.
            // In other words, an onset can ONLY be detected when the
            // helper is in the "ready to be triggered" state, not when
            // it's already in the middle of a fade-in from a previous
            // onset. This matches the intent (one onset per note attack,
            // not one per rising block) and produces the expected
            // behaviour: a single, brief mute at the start of each note,
            // followed by a smooth IIR ramp back to full.
            const bool rising = blockRms > prevRms * kRiseRatio;
            const bool onset = rising
                            && blockRms > slowEnv * kOnsetRatio
                            && slowEnv > kMinLevel
                            && attackGain > kReadyThreshold;

            if (onset)
            {
                // On the attack block itself, fully ease the correction off
                // (gain 0) and do NOT ramp yet: the natural attack transient is
                // left untouched. Ramping starts on the following blocks.
                attackGain = 0.0f;
            }
            else
            {
                // Ramp the correction gain back up to full over the release time.
                // Use an EXPONENTIAL (IIR) ramp so the curve is C0-smooth at the
                // start (no step at onset+1 block), matching the RetargetEnvelope's
                // ratio ramp. With a 10 ms release the first non-onset block at
                // 256 samples now contributes `1 - exp(-5.8/10) = 0.44` instead of
                // the old `5.8/10 = 0.58` (still a step in absolute terms but the
                // IIR ensures the SECOND-order derivative is also smooth, which
                // is what the OLA chain needs to absorb the transition without
                // artifacts).
                if (blockDurSec > 0.0f && releaseSec > 0.0f)
                {
                    const float alpha = static_cast<float> (1.0 - std::exp (-blockDurSec / releaseSec));
                    // Standard IIR step toward the target (1.0). The result is
                    // bounded by [attackGain, 1.0] because alpha is in [0, 1].
                    attackGain = attackGain + alpha * (1.0f - attackGain);
                }
                else
                {
                    // Degenerate (release=0 or blockDur=0): snap to 1 immediately.
                    attackGain = 1.0f;
                }
                // Numerical safety: clamp to [0, 1].
                if (attackGain > 1.0f) attackGain = 1.0f;
                if (attackGain < 0.0f) attackGain = 0.0f;
            }

            prevRms = blockRms;
            slowEnv = slowEnv + kSlowCoeff * (blockRms - slowEnv);
            return attackGain;
        }

    private:
        bool  enabled = false;
        float releaseSec = 0.06f;  // 60 ms default
        float attackGain = 1.0f;   // correction gain (0 = no correction at onset)
        float slowEnv = 0.0f;      // slow level follower
        float prevRms = 0.0f;      // previous block RMS (rising-edge detection)

        static constexpr float kRiseRatio  = 1.2f;   // block must rise 20% to count as rising
        static constexpr float kOnsetRatio = 1.5f;   // must exceed slow env by 50%
        static constexpr float kMinLevel   = 1.0e-4f;// ignore near-silence
        static constexpr float kSlowCoeff  = 0.002f; // slow follower time constant
        // 2026-07-23 (Fix AX): the helper must be "ready" (attackGain at or
        // very close to 1.0) before it can fire another onset. This prevents
        // a sustained attack (many blocks of rising level) from triggering
        // a continuous stream of onsets, which would keep attackGain at 0
        // and silence the output. 0.9 means we require at least 90% of the
        // ramp to have completed before the next onset can fire; with a
        // 60 ms release and 5.8 ms block, that's ~150 ms of "cooldown"
        // after the helper fires â€” well-matched to the perceived length
        // of a vocal note attack.
        static constexpr float kReadyThreshold = 0.9f;
    };
}



