// VibratoPreserver.h
// Vibrato-preservation helper for the autotune correction pipeline.
//
// Problem: a standard autotuner snaps the *instantaneous* detected pitch
// (f0_in) to the nearest scale note every analysis block. Because vibrato is a
// fast pitch modulation around a center, that per-block snapping flattens the
// modulation and the vibrato disappears.
//
// Solution: maintain a smoothed *center* pitch (a one-pole low-pass of f0_in
// that removes the vibrato LFO), compute the correction ratio against that
// center, then re-apply it to the instantaneous pitch. The blend amount lets
// the user dial between full instantaneous correction (0%, classic behaviour)
// and full vibrato preservation (100%).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    class VibratoPreserver
    {
    public:
        // One-pole low-pass coefficient applied once per pitch-analysis block.
        // ~0.2 gives a time constant of roughly 200 ms at a 44.1 kHz / 2048-sample
        // block rate, which removes typical 4-8 Hz vibrato while still tracking
        // note-to-note changes within a fraction of a second.
        static constexpr float kSmoothing = 0.2f;

        /** Feed a newly detected pitch into the center tracker.
         *  @param f0 detected instantaneous pitch in Hz. Pass <= 0 for silence
         *           (the center is held so re-attacks do not jump). */
        void update (float f0)
        {
            if (f0 <= 0.0f)
                return;                 // silence: keep the last center
            if (center <= 0.0f)
                center = f0;            // initialise on a fresh attack
            else
                center += kSmoothing * (f0 - center);
        }

        /** Reset the tracked center (e.g. on transport seek / preset load). */
        void reset() noexcept { center = 0.0f; }

        /** Current smoothed center pitch (Hz). */
        float getCenter() const noexcept { return center; }

        /** Blend the instantaneous autotune ratio toward the vibrato-preserving
         *  (center-based) ratio.
         *
         *  @param targetRatioInstant ratio_full = quantize(f0) / f0
         *  @param f0                  instantaneous pitch (Hz)
         *  @param targetCenter        quantize(center) in auto mode, or the
         *                             curve target evaluated at the center pitch
         *  @param preserve            blend amount, 0..1 (0 = off / classic,
         *                             1 = full vibrato preservation)
         *  @return blended ratio. At preserve == 0 this is targetRatioInstant;
         *          at preserve == 1 it is targetCenter / center, which keeps the
         *          f0 * ratio modulation intact so the vibrato survives.
         */
        float blend (float targetRatioInstant, float f0, float targetCenter,
                     float preserve) const noexcept
        {
            if (preserve <= 0.0f || center <= 0.0f || f0 <= 0.0f)
                return targetRatioInstant;
            const float ratioCenter = targetCenter / center;
            return targetRatioInstant * (1.0f - preserve) + ratioCenter * preserve;
        }

    private:
        float center = 0.0f;
    };
}
