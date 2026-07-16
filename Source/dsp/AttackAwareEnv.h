// AttackAwareEnv.h
// Attack-aware correction helper for the autotune pipeline.
//
// Problem: a classic autotuner snaps the pitch on every block, including the
// very first milliseconds of a note (the attack transient). That "robotic"
// pitch-lock on the attack is one of the things that makes cheap autotune
// sound artificial. FlexTune (pitch-distance deadband) and Humanize (random
// wobble) already exist as two orthogonal axes; this is a THIRD one: temporal.
//
// Solution: detect onsets from the input level envelope and, on each onset,
// drop a correction-gain envelope to 0, then ramp it back to 1 over a
// user-set release time. The gain is multiplied into the correction amount,
// so the natural attack is left untouched and the note is pulled to pitch
// only after the transient has passed.

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
         *  ramp back from 0 (onset) to full (1) after the attack. */
        void setReleaseSeconds (float sec) noexcept { releaseSec = juce::jmax (0.001f, sec); }

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
            const bool rising = blockRms > prevRms * kRiseRatio;
            const bool onset = rising && blockRms > slowEnv * kOnsetRatio && slowEnv > kMinLevel;

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
                const float rampPerBlock = (blockDurSec > 0.0f)
                    ? juce::jmin (1.0f, blockDurSec / releaseSec) : 1.0f;
                attackGain = std::min (1.0f, attackGain + rampPerBlock);
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
    };
}
