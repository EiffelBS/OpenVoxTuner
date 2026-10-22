// DelayEffect.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>   // juce::LinearSmoothedValue: the tempo copy ramp. The plugin
                                  // target links juce_dsp (CMakeLists.txt), so this header compiles
                                  // in every plugin TU. Every FILTER stays local (OnePoleLowCut.h),
                                  // for the same reason DeEsser.h and SaturationEffect.h keep their
                                  // biquads and poles local: the test target does not always link juce_dsp.

#include <cmath>

#include "IEffect.h"
#include "OnePoleLowCut.h"

namespace ovtdsp
{
    /**
     * A tempo-synced stereo ping-pong delay on the corrected lead voice.
     *
     * WHY PING-PONG, AND WHY THE CROSS-FEEDBACK IS THE WHOLE POINT
     * Two delay lines (L and R) of the same length. Each line is fed by its own input
     * channel PLUS the tail of the OTHER line, scaled by feedback: the L tail lands in the
     * R line and vice versa. That cross-coupling is what puts the echoes ALTERNATING
     * between the sides -- the classic ping-pong image a vocal needs: repeats dance left/
     * right around the dry voice instead of smearing into a mono blob. A plain dual-mono
     * delay with two independent lines gives a static L/R pair, which on a lead vocal
     * reads as a widening, not as rhythm.
     *
     * THE FEEDBACK LOOP NEEDS A LOW-CUT, NOT JUST A GAIN
     * Feedback is at most 1.0, so the loop can ring for seconds. Every pass keeps the lows,
     * and a vocal send carries sub-bass energy (breath, mic handling, plosive thump) that a
     * delay stores indefinitely: below ~150 Hz the loop builds a DC-leaning rumble that
     * pumps the output stage and muds the low end. So the tail is run through ONE POLE PER
     * CHANNEL (ovtdsp::OnePoleLowCut at 150 Hz, 12 dB/octave) inside the feedback path: the
     * sub-bass decays faster than the audible band, which is exactly how a hardware ping-pong
     * keeps its repeats clean. The pole blocks DC by construction (a constant input drives
     * the state to x, so the output goes to zero), so an offsetting send cannot lock the loop.
     * 150 Hz leaves every fundamental of the singing voice intact -- below F3 at 174 Hz is
     * bass territory the lead does not own.
     *
     * TEMPO MATHS ARE IN BEATS, NEVER IN SECONDS
     * The division knob selects a fraction OF A BEAT, ordered by REPETITION LENGTH (shortest
     * first): {1/3, 0.5, 2/3, 0.75, 1.0, 4/3, 1.5, 2.0, 3.0} for the UI labels
     * {"1/8T","1/8","1/4T","1/8D","1/4","1/2T","1/4D","1/2","1/2D"}. "D" is a DOTTED value
     * (1.5x the plain note, the standard note-value dot) and "T" a TRIPLET (2/3 of the plain
     * value), so "1/4D" at any tempo is one dotted quarter of the host's beat, "1/8T" is two
     * thirds of an eighth and "1/2D" is three beats. Each plain note value has its dotted and
     * its triplet sibling, so the knob is a complete duration ladder rather than a list with
     * exceptions. copySeconds = (60 / bpm) * fraction, so when ARA gives us the host tempo the
     * delay follows it exactly; without ARA the plugin's own BPM register is used. Fractions
     * in beats are what makes this knob read as musical: a change of bpm moves the delay with
     * the song, not against it.
     * The table is sorted by duration rather than grouped by note family, so turning the knob
     * walks the repeats from the shortest slap (1/8T, 8000 samples at 120 BPM) to the longest
     * (1/2D, 72000): a duration axis, not a note-name list. Because the plugin state, presets
     * and A/B slots store the choice INDEX, this spacing is part of the patch: a division saved
     * by a pre-release build that used the earlier five-value order re-maps on load (1/4, the
     * default, moved from index 2 to index 4). The mapping was still being settled before the
     * dev branch merged, which is why it is corrected now rather than frozen with the triplets
     * appended.
     *
     * A COPY LENGTH MUST NEVER JUMP: THE RAMP IS WHY
     * Changing division or tempo re-targets the copy length. Reading an integer sample count
     * and stepping it discontinuously changes the read position by tens of milliseconds in
     * one sample -- a phase jump, audible as a click on any sustained tail. So the copy is
     * held in juce::LinearSmoothedValue<float> with rampSamples = 25 ms (the codebase
     * convention for every enable/mix ramp), and the read-out uses LINEAR INTERPOLATION
     * between neighbouring samples: while the ramp moves, reads land between two stored
     * samples, which is what keeps a changing delay length continuous. Fractional reading is
     * not optional here -- without it the ramp would be inaudible as stair-steps. The
     * interpolation has no filter, so its cost is one multiply-add per sample; it attenuates
     * the top of the band slightly at short copies (the standard delay-line trade-off), which
     * the 150 Hz loop pole makes irrelevant where it matters most: a long tail is read near
     * Nyquist-clean, and any residual HF loss is below the audible vocal band.
     *
     * THE MINIMUM COPY IS A COMB GUARD, NOT A MUSICAL CHOICE
     * Reads are at least max(1, round(0.04 * sr)) samples behind the write: a copy of only a
     * few samples is not an echo, it is a comb filter on the dry voice, and a feedback loop
     * that reads its own write is a zero-delay loop -- mathematically an instant resonance.
     * 40 ms puts the shortest repeat above the flanging zone so what comes back is heard as
     * an echo. At bpm 20 with the longest division (1/2D, 3 beats) that is 9 s, which is why the
     * line buffers are sized for exactly that worst case in prepare(); process() allocates
     * nothing at block time (the project rule: every buffer is sized once in prepare()).
     *
     * NO LATENCY IS DECLARED -- THIS STAGE IS IN-PLACE
     * setLatencySamples is NEVER called from this class, and the wiring does not call it
     * either. The stage runs in-place on the lead bus, dry stays at unity, and the wet ADDS:
     * a send adds, never steals (see IEffect.h), so nothing lives after it that a declared
     * latency would align with. Declaring the copy length as plugin latency would put the
     * whole voice behind the echo, which is not what an echo does. Precedent in the project:
     * SaturationEffect.h declares no latency for its 16-sample FIR group delay for exactly
     * this reason (changelog entry of 2026-10-16). The pitch shifter's and limiter's declared
     * latencies stay untouched.
     *
     * ENABLE IS A 25 MS CROSSFADE, AND FULLY OFF IS BIT-IDENTICAL
     * Same ramp as DeEsser/Saturation: bypassGain moves toward 0/1 with the 25 ms one-pole
     * coefficient computed in prepare(). Once it has decayed below the floor and the switch is
     * off, the OUTPUT WRITE IS SKIPPED ENTIRELY -- the buffer leaves this stage bit-identical,
     * so a disabled delay cannot add DC, noise, or CPU-side change to the voice. The lines are
     * still fed while bypassed (see process()): keeping the write timeline moving means
     * re-enabling resumes on the CURRENT signal instead of resurrecting an old segment, which
     * is what a muted delay does in a real console: the tape keeps running. Cost is two writes
     * per sample -- no interpolation, no feedback, so a bypassed stage stays cheap. The sleep
     * path hands over a cleared buffer (PluginProcessor.cpp ~1662), so feeding zeros drains
     * the tail through its own state; nothing in process() assumes non-zero input.
     *
     * LEVELS: THE WET ADDS, THE OUTPUT STAGE IS THE SAFETY NET
     * Dry is unity and the echo sum is added (the classic ping-pong, not a crossfade), so at
     * mix 1 with high feedback the stage can add up to ~+4 dB. The master trim + true-peak
     * limiter sit after every bus (SaturationEffect.h notes the same reasoning): loudness
     * control stays the user's Output Trim knob, and the default is OFF, so existing projects
     * do not change sound on load -- the same rule as Saturation.
     *
     * THE SEMANTICS THE TESTS ASSERT, STATED AS A CONTRACT
     * process() ADDs: out[c] = dry[c] + bypassGain * mix * tail[c], with dry at unity and
     * mix*tail ADDED -- the stage never multiplies or replaces the dry voice. Fully bypassed
     * (switch off AND the 25 ms ramp decayed below kBypassFloor) leaves the OUTPUT BUFFER
     * UNTOUCHED, so a disabled delay is bit-identical to its input; in that branch only the
     * line memory is written, so the lines keep feeding and re-enabling resumes on the current
     * timeline. The stage declares ZERO latency: setLatencySamples is never called from this
     * class and the wiring must not call it either -- an echo is additive, declaring its copy
     * length as plugin latency would put the whole voice behind the echo (precedent:
     * SaturationEffect.h declares nothing for its FIR group delay, changelog 2026-10-16).
     */
    class DelayEffect : public IEffect
    {
    public:
        /** The send room this stage lives in is stereo: two delay lines. */
        static constexpr int kMaxChannels = 2;

        juce::String getId() const override   { return "delay"; }
        juce::String getName() const override  { return "Delay"; }

        void prepare (double sampleRate, int maximumBlockSize) override;
        void reset() override;
        void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) override;

        /**
         * Tempo in BPM. Clamped to the range the plugin already clamps its own BPM register
         * to (20..400, see PluginProcessor.h setBpm): the prepared capacity is sized for bpm 20
         * at the longest division, so a value inside this ladder always fits the line buffers.
         */
        void setBpm (float bpm)
        {
            const float b = juce::jlimit (kMinBpm, kMaxBpm, bpm);
            if (b != currentBpm)
            {
                currentBpm   = b;
                copyDirty    = true;   // re-target the ramp on the next block, never mid-sample
            }
        }

        /**
         * Division index, ordered by repetition length (shortest first):
         * 0 = "1/8T" (triplet), 1 = "1/8", 2 = "1/4T" (triplet), 3 = "1/8D" (dotted),
         * 4 = "1/4" (the default), 5 = "1/2T" (triplet), 6 = "1/4D" (dotted), 7 = "1/2",
         * 8 = "1/2D" (dotted).
         * Each index is a fraction of a beat, see kDivisionFractions.
         */
        void setDivision (int idx)
        {
            const int i = juce::jlimit (0, kDivisionCount - 1, idx);
            if (i != divisionIdx)
            {
                divisionIdx = i;
                copyDirty   = true;
            }
        }

        /** Wet of the echo, 0..1. The dry path stays at unity: this knob only ADDS. */
        void setMix (float mix01)      { mix = juce::jlimit (0.0f, 1.0f, mix01); }

        /** Cross-feedback amount, 0..1. Above ~0.95 the loop rings for seconds by design. */
        void setFeedback (float fb01)  { feedback = juce::jlimit (0.0f, 1.0f, fb01); }

        // Diagnostics for the unit tests: the copy length currently being read (in samples),
        // the target it is ramping toward, the line capacity prepared, and the enable ramp.
        float getCopySamples() const   { return copyRamp.getCurrentValue(); }
        float getTargetCopySamples() const { return copyRamp.getTargetValue(); }
        int   getLineCapacity() const  { return capacity; }
        bool  isBypassed() const       { return bypassGain < kBypassFloor; }

    private:
        /** Division fractions, IN BEATS, ordered by repetition length for the UI labels
            {"1/8T","1/8","1/4T","1/8D","1/4","1/2T","1/4D","1/2","1/2D"}. The knob turns in
            exactly this order (shortest repeat first), and the default "1/4" sits at index 4. */
        static constexpr int    kDivisionCount     = 9;
        static constexpr float  kDivisionFractions[kDivisionCount] =
            { 1.0f / 3.0f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 3.0f };

        /** BPM ladder the plugin already clamps to, so capacity sizing is provably enough. */
        static constexpr float  kMinBpm = 20.0f;
        static constexpr float  kMaxBpm = 400.0f;

        /** Comb guard: shortest echo the stage will read (see the class doc). */
        static constexpr double kMinDelaySeconds = 0.04;

        /** 25 ms copy ramp and enable ramp, the codebase convention for every knob/envelope. */
        static constexpr double kRampSeconds = 0.025;
        static constexpr float  kBypassFloor = 1.0e-4f;   // below this, the output pass is skipped

        /** Feedback-loop low-cut: kills sub-bass buildup in a long ring (see the class doc). */
        static constexpr float  kFeedbackLowCutHz = 150.0f;

        /** Longest copy prepare() can serve: bpm floor at the longest division (1/2D = 3 beats),
            in seconds. The other divisions are shorter, so 3.0 bounds every entry in the table. */
        static constexpr double maxCopySeconds() { return (60.0 / static_cast<double> (kMinBpm)) * 3.0; }

        /** Smallest power of two >= wanted: a power-of-two block is cache/page friendly, and the ring wrap in
         *  the audio loop stays a single conditional add because the read distance never exceeds capacity. */
        static int nextPowerOfTwo (int wanted);

        double sampleRate = 44100.0;
        int    capacity   = 0;       // line rows, power of two (page-friendly allocation); the ring wrap
                                     // is a single conditional add, because the read distance never exceeds
                                     // capacity -- no per-sample modulo in the audio loop.
        int    minCopySamples = 1;
        int    maxCopySamples = 1;

        /** Shared write index: both lines are the same length on one timeline. */
        int    pos = 0;              // index of the most recently written sample

        /** Stereo delay lines, sized once in prepare(); never resized at block time. */
        juce::AudioBuffer<float> lines;

        /** Copy length in samples, ramped over 25 ms so a tempo/division change cannot click. */
        juce::LinearSmoothedValue<float> copyRamp { 1.0f };

        /**
         * Two-tap crossfade state for a copy-length change. Moving the read pointer itself (the obvious
         * "ramp") glides the pitch: a 1/4 -> 1/2 change at 120 BPM walks 12 000 samples in 1 200 steps,
         * i.e. 10 samples per step, which is an audible zip on sustained material. Instead the tap we
         * were reading is FROZEN at crossCopySamples and the new distance is faded in over the same
         * 25 ms; the timeline is preserved and only the two distances are blended. crossFade == 1
         * means "no crossfade in flight", the steady state (and the state after reset()).
         */
        float crossCopySamples = 0.0f;
        float crossFade        = 1.0f;
        float crossCoeff       = 0.0f;   // same 25 ms constant as the enable ramp

        /** Feedback-path low-cut, one pole per channel. */
        OnePoleLowCut feedbackLowCut[kMaxChannels];

        float currentBpm   = 120.0f;
        int   divisionIdx  = 2;      // "1/4": the musical default
        float mix          = 0.30f;
        float feedback     = 0.35f;

        /** Enable ramp state, same shape as SaturationEffect.h / DeEsser.h. */
        float bypassGain  = 0.0f;
        float bypassCoeff = 0.0f;
        int   rampSamples = 1;

        bool copyDirty    = true;    // recompute the target copy for this sample rate

        /** Recompute the copy length implied by bpm + division and move the read to it.
         *
         *  With allowCrossfade, a change freezes the tap currently being read and fades the new one in
         *  over 25 ms (see the crossFade members); without it (prepare / reset / first arming) the read
         *  is simply seeded on the new distance, because there is no history to glide away from.
         */
        void updateTargetCopy (bool allowCrossfade = true)
        {
            const double beatSeconds   = 60.0 / static_cast<double> (currentBpm);
            const double copySeconds   = beatSeconds * static_cast<double> (kDivisionFractions[divisionIdx]);
            const int    wanted        = static_cast<int> (std::llround (copySeconds * sampleRate));

            // Clamp into the range this instance can serve: the comb floor at one end, and the
            // capacity prepared in prepare() at the other.
            const float target = static_cast<float> (juce::jlimit (minCopySamples, maxCopySamples, wanted));

            if (allowCrossfade && std::abs (target - copyRamp.getTargetValue()) > 0.5f)
            {
                crossCopySamples = copyRamp.getCurrentValue();   // the distance the tail is arriving from
                crossFade        = 0.0f;                        // and the fade that hands over to the new one
            }

            copyRamp.setCurrentAndTargetValue (target);   // no pointer glide: the crossfade does the work
            copyDirty = false;
        }

        /** Read one line at the ramped fractional distance. Linear interpolation between neighbours. */
        float readTailAt (int channel, int integerDistance, float fraction) const
        {
            const auto* data = lines.getReadPointer (channel);
            int idxA = pos - integerDistance;          // data[pos] is the newest sample
            if (idxA < 0) idxA += capacity;            // distance never exceeds capacity, one wrap is enough
            int idxB = idxA - 1;
            if (idxB < 0) idxB += capacity;
            return data[idxA] + fraction * (data[idxB] - data[idxA]);
        }

        /** The echo pair for this sample: the steady tap, or the 25 ms two-tap crossfade that hands a
         *  copy-length change over without gliding the read pointer (see the crossFade members). */
        void readTailPair (float& tailL, float& tailR)
        {
            if (crossFade >= 1.0f)
            {
                tailL = readTailAt (0, copyInteger, copyFraction);
                tailR = readTailAt (1, copyInteger, copyFraction);
                return;
            }

            crossFade += (1.0f - crossFade) * crossCoeff;
            if (crossFade >= 0.9999f) crossFade = 1.0f;

            int oldI = static_cast<int> (crossCopySamples);
            if (oldI < minCopySamples) oldI = minCopySamples;
            const float oldF = juce::jlimit (0.0f, 1.0f, crossCopySamples - static_cast<float> (oldI));

            const float oldL = readTailAt (0, oldI, oldF);
            const float oldR = readTailAt (1, oldI, oldF);

            tailL = oldL + crossFade * (readTailAt (0, copyInteger, copyFraction) - oldL);
            tailR = oldR + crossFade * (readTailAt (1, copyInteger, copyFraction) - oldR);
        }

        /**
         * Write the next sample into both lines, cross-feeding each tail through its own pole.
         * The tails are handed in already read: reading them once per sample is what keeps this
         * loop cheap, and they are read BEFORE the write position moves, so a copy of 0 is not
         * reachable (minCopySamples >= 1 blocks a zero-delay feedback loop).
         */
        void writeWithCrossFeedback (float inL, float inR, float tailL, float tailR)
        {
            // Cross-feedback: the L tail is what the R line hears, and vice versa. Feeding each line its
            // OWN tail would be plain feedback (two independent echoes); crossing them is what turns the
            // pair into one ping-pong cycle R -> L -> R, one copy apart per pass. The INPUT enters the
            // opposite line for the same reason: with the input written straight into its own side, the
            // first repeat would land on the source's side and a centred voice would echo "L, R, L..."
            // starting on L instead of bouncing from the first repeat on.
            const float newL = inR + feedback * feedbackLowCut[0].process (tailR);
            const float newR = inL + feedback * feedbackLowCut[1].process (tailL);

            pos += 1;
            if (pos >= capacity) pos = 0;
            lines.getWritePointer (0)[pos] = newL;
            lines.getWritePointer (1)[pos] = newR;
        }

        /** Per-sample read position shared by the two lines, recomputed once per sample. */
        int   copyInteger  = 1;
        float copyFraction = 0.0f;

        void advanceReadPosition()
        {
            const float cp = copyRamp.getNextValue();
            int    i = static_cast<int> (cp);
            if (i < minCopySamples)      // guard the ramp's very first steps against a sub-floor read
                i = minCopySamples;
            copyInteger  = i;
            copyFraction = juce::jlimit (0.0f, 1.0f, cp - static_cast<float> (i));
        }

        void flushLines() { lines.clear(); pos = 0; copyInteger = minCopySamples; copyFraction = 0.0f; }
    };
}

