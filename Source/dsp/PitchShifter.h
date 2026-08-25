// PitchShifter.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "IPitchShifter.h"
#include "BlockAwareOnePole.h"

namespace ovtdsp
{
    /**
     * PitchShifter based on the PSOLA algorithm (Pitch Synchronous Overlap-Add).
     * Legacy in-house implementation from Phase 4.
     */
    class PitchShifter
    {
    public:
        PitchShifter();
        ~PitchShifter() = default;

        void prepare (double sampleRate, int maximumBlockSize);        
        void reset();
        // Reset the internal state WITHOUT re-arming the startup fade-in.
        // Use on seek/preset/setting change mid-session, to avoid a new
        // fade-in over an already full-amplitude signal (which would
        // generate a pop). prepare() calls the full reset().
        void resetSoft();
        void process (juce::AudioBuffer<float>& buffer, float pitchRatio, float formantRatio, float f0);
        void process (const juce::AudioBuffer<float>& input, juce::AudioBuffer<float>& output, float pitchRatio, float formantRatio, float f0);
        void setLatencyMs (float newLatencyMs);
        // Force-create a test grain (for debug): will create a single active grain
        void forceCreateTestGrain();
        int getLatencySamples() const { return latencySamples; }

        // Set attack time for voice onset (default 30ms). 0 = no attack envelope.
        void setAttackTimeMs (float ms);

        // Enable/disable the internal attack envelope. When disabled, the
        // PitchShifter does NOT apply its own onset fade-in/down at note
        // onsets or pitch jumps. This is used to coordinate with an
        // external attack-aware correction helper (e.g. ovtdsp::AttackAwareEnv):
        // when the helper already controls the correction gain, having
        // BOTH the helper and the internal envelope run at the same time
        // creates a "double attenuation" that the user perceives as a
        // scratchy artifact, especially at low Amount values. Default: ON
        // (backward-compatible behaviour).
        void setAttackEnvelopeEnabled (bool enabled) noexcept
        {
            attackEnvelopeEnabled = enabled;
            // When the envelope is disabled, snap its gain to 1.0 so the
            // output is never muted.
            if (! enabled) attackEnvelope.forceOpen();
        }

        bool isAttackEnvelopeEnabled() const noexcept { return attackEnvelopeEnabled; }

        // 2026-07-23: external attack-gain driver (Fix AW - see
        // PluginProcessor.cpp for the call site). When set, the value is
        // used as the BLOCK-LEVEL target for the envelope gain, with a
        // block-aware one-pole smoother (TC = externalAttackTauSeconds,
        // default 15 ms) absorbing the per-block jumps from the external
        // source. Crucially, the modulation is applied to the OUTPUT
        // multiplier, NOT to the OLA target ratio, so the OLA chain's
        // grain spacing is stable across the transition (no re-alignment
        // clicks). Pass a value in [0, 1]; pass a NEGATIVE value to
        // disable the external driver and fall back to the internal
        // envelope (or the raw output if `attackEnvelopeEnabled` is off).
        //
        // This is the architectural fix for the "Speed=0 + Attack=10 ms
        // scratch" bug: the original code modulated the amount (which
        // multiplied into targetRatio) and disabled the internal envelope
        // to avoid double-attenuation, leaving the OLA chain with no
        // smoothing at all when the user's Speed knob was 0.
        void setExternalAttackGain (float gain, float blockDurSec) noexcept
        {
            if (gain < 0.0f)
            {
                externalAttackEnabled = false;
                return;
            }
            externalAttackEnabled = true;
            const float clamped = juce::jlimit (0.0f, 1.0f, gain);
            // Single-step IIR smoother on the per-block target. The
            // smoother's TC (default 15 ms) is independent of the
            // buffer size, so the per-block jumps from the external
            // source (AttackAwareEnv's IIR ramp) are absorbed smoothly
            // without per-sample discontinuities. The output multiplier
            // is constant within the block, so the OLA chain's grain
            // spacing is stable across the transition.
            attackEnvelope.gain = externalAttackSmoother.step (clamped, blockDurSec);
        }

        // Set the time constant (in seconds) of the external attack
        // smoother. Default is 0.015 (15 ms). Should be called from
        // prepare() once the sample rate is known (it does NOT depend on
        // the sample rate, but the smoother's internal alpha does).
        void setExternalAttackTauSeconds (float tauSec) noexcept
        {
            externalAttackTauSeconds = juce::jmax (0.001f, tauSec);
            externalAttackSmoother.setTimeConstantSeconds (externalAttackTauSeconds);
        }

        // Reset the external attack smoother (e.g. on transport stop or
        // preset change). After reset, the next setExternalAttackGain
        // call snaps the smoother to the new target.
        void resetExternalAttackGain() noexcept
        {
            externalAttackSmoother.snapTo (1.0f);
            attackEnvelope.forceOpen();
            externalAttackEnabled = false;
        }

    private:
        double sampleRate = 44100.0;
        int latencySamples = 0;
        float latencyMs = 20.0f;

        // COLA sum of the KBD window (beta=6) at 50% overlap. The grain gain
        // is computed so that the sum of windows equals 2.0 (Hann case);
        // since the KBD has a different COLA sum, we measure it once in
        // prepare() and take it into account to avoid over-gain/clip.
        // Sentinel: -1.0 = not yet computed (computed on first prepare()).
        double kbdColaSum = -1.0;
        
        static constexpr int bufferSize = 65536; 
        static constexpr int bufferMask = bufferSize - 1;
        juce::AudioBuffer<float> ringBuffer;
        
        uint64_t absoluteWritePos = 0;
        // Total number of samples written to the ring since the last
        // reset. Used to know whether the available history covers the
        // requested latency (avoids reading zeros before the signal starts).
        int64_t totalWritten = 0;
        
        float currentRatio = 1.0f;
        float currentFormantRatio = 1.0f;

        // Smoothed input pitch (one-pole) used to compute
        // targetF0 / Tin / Tout. Without it, a brutal f0 jump (new note
        // or attack) changes the grain periods at once -> discontinuity
        // in the OLA -> click. Smoothing softens the period transition
        // without coloring the timbre (the correction ratio is already
        // smoothed by RetargetEnvelope downstream).
        //
        // 2026-07-23 (Fix BA): alpha was 0.002 per block, giving TC =
        // 1 / (0.002 * 172 blocks/sec) = ~2.9 seconds. That was way too
        // slow: at 64-256 sample buffers, the user reported audible pops
        // at every note onset because smoothedF0 took several seconds
        // to converge to the new f0, during which the OLA chain targeted
        // WRONG periods (mid-transition between the old and new pitch).
        // The new alpha of 0.02 gives TC = 1 / (0.02 * 172) = ~290 ms,
        // which is fast enough to follow typical note attacks (5-30 ms
        // attack time on most instruments) within a few blocks, and slow
        // enough to remain a lowpass filter for the 5Hz vibrato (|H(5Hz)|
        // = 0.62, so the smoothedF0 follows the vibrato at 62% of its
        // amplitude - sufficient because the YIN pitch detector already
        // does its own smoothing, and the VibratoPreserver does the
        // vibrato preservation on the targetRatio, not on smoothedF0).
        float smoothedF0 = 0.0f;
        static constexpr float kF0SmoothAlpha = 0.02f;
        
        double outPhase = 0.0;
        double lastGrainCenter = 0.0;

        // 2026-07-23 (Fix BB): previous pitch ratio (per block), used to
        // detect sudden changes in `pitchRatio` (e.g. FlexTune transitions
        // out of/into the deadband, humanize random walk, vibrato
        // preservation switches). When the per-block delta exceeds a
        // threshold (3% in this implementation), the internal attack
        // envelope is armed to mask the OLA re-organisation. Without
        // this, the OLA chain "snaps" to a new period every time the
        // smoother output changes by more than ~1% per block, producing
        // user-reported "pop/clicks at pitch changes" issue with
        // Flex>0. Note: the ONSET detection (f0 transitions to voiced
        // or >2-semitone jumps) is independent and still works.
        float lastPitchRatio = 1.0f;

        // Attack envelope state
        float attackMs = 30.0f;
        bool attackEnvelopeEnabled = true; // Disable when an external helper (AttackAwareEnv) owns the envelope.
        bool wasVoiced = false;
        float lastF0 = 0.0f; // For detecting sudden pitch jumps (note attacks)

        // 2026-07-23: external attack-gain smoother (Fix AW). The smoother
        // is a BlockAwareOnePole (TC = 15 ms by default) that absorbs the
        // per-block jumps from the external source (AttackAwareEnv). The
        // smoothed value is the OUTPUT multiplier, not a target-ratio
        // modulation, so the OLA chain's grain spacing is stable.
        BlockAwareOnePole externalAttackSmoother;
        bool externalAttackEnabled = false;
        // 15 ms TC. Matches the average user expectation for "fast but
        // not clicky" attack. Can be tuned at the call site via
        // setExternalAttackGainTauSeconds().
        float externalAttackTauSeconds = 0.015f;

        // Startup fade-in: ring buffer starts empty (zeros), so first N samples
        // are garbage. Fade in over first ~20ms to avoid click on plugin start.
        int startupSamplesRemaining = 0;
        float startupGain = 0.0f;
        double startupAlpha = 0.0;

        // True only after the first prepare() (plugin startup). Allows
        // distinguishing a session reset (seek/preset) from a real startup.
        bool firstPrepareDone = false;

        // -----------------------------------------------------------------
        // Explicit state machines
        // -----------------------------------------------------------------

        // Hysteresis + debounce voice-activity detector. Converts the raw
        // per-frame f0 (0 = unvoiced frame) into a filtered voiced flag.
        //
        // Why not a plain threshold: at note starts the pitch flickers
        // around the voiced/unvoiced threshold ("vocal flutter"); without
        // filtering, the voiced flag would toggle back and forth and re-arm
        // the attack envelope in a loop -> repeated clicks. Two mechanisms:
        //   - HYSTERESIS: different rise (45 Hz) / fall (35 Hz) thresholds.
        //   - DEBOUNCE: kDebounceSamples consecutive samples (~6 ms @44.1k)
        //     must agree before the state actually changes.
        struct VoiceActivityDetector
        {
            static constexpr float kOnThreshold     = 45.0f; // rising edge
            static constexpr float kOffThreshold    = 35.0f; // falling edge
            static constexpr int   kDebounceSamples = 256;   // ~6 ms @44.1k

            bool voiced  = false; // filtered voiced state
            int  counter = 0;     // consecutive-agreement sample counter

            // Feed one f0 sample; returns the filtered voiced state.
            bool processSample (float f0) noexcept
            {
                const bool rawVoiced = (f0 > kOnThreshold);
                if (rawVoiced && !voiced)
                {
                    // Rising: require kDebounceSamples consecutive samples
                    // above the threshold before validating.
                    if (++counter >= kDebounceSamples)
                        voiced = true;
                }
                else if (! rawVoiced && voiced)
                {
                    // Falling: lower threshold (hysteresis) + debounce.
                    if (f0 < kOffThreshold)
                    {
                        if (++counter >= kDebounceSamples)
                            voiced = false;
                    }
                    else
                    {
                        // Between the two thresholds: keep state, reset counter.
                        counter = 0;
                    }
                }
                else
                {
                    // No state change in progress: do not count.
                    counter = 0;
                }
                return voiced;
            }

            void reset() noexcept { voiced = false; counter = 0; }
        };

        // Explicit lifecycle of the output attack envelope.
        //
        // Historically this was spread over four loose members (attackGain,
        // slowAttackSamplesRemaining, attackRampDownSamplesRemaining and
        // attackAlpha); their interaction formed an implicit automaton that
        // was hard to follow. The phases below make it explicit:
        //
        //   Open          - steady state: gain == 1, no timer pending.
        //   RampDown      - right after an onset/jump event: the one-pole
        //                   TARGETS 0 for ~15-20 ms so the gain dips smoothly
        //                   (1.0 -> ~0.17-0.86) instead of being hard-reset
        //                   to 0 (which clicked - see Fix G).
        //   RecoverSlow   - then climbs back toward 1 with the slow ~80 ms TC,
        //                   masking the OLA re-organisation window (20-50 ms
        //                   after a jump, local sum fluctuates by up to 0.4 -
        //                   see Fix K2/O).
        //   RecoverNormal - final climb with the user-facing attackMs TC.
        //                   This is also the note-on fade-in path after the
        //                   block-level snapToZero().
        //
        // Arming entry points (guards applied at the call sites in process()):
        //   armForOnset()     - voice onset or > 2 semitone jump: 150/20 ms.
        //   armForRatioJump() - caller ratio changed > 3% in one block
        //                       (FlexTune deadband etc.): 100/15 ms, NO OLA
        //                       chain reset (would reintroduce the "trumpet"
        //                       artifact - see Fix BB).
        //   snapToZero()      - block-level silence->voice boundary: hard
        //                       snap to 0, then RecoverNormal fades the note
        //                       in over attackMs.
        //
        // External-driver mode (ovtdsp::AttackAwareEnv, Fix AW): the helper
        // pushes a smoothed block-level gain through
        // PitchShifter::setExternalAttackGain(), which stores it directly in
        // `gain`; processSample() is NOT called in that mode, only
        // clearTimers(), so the internal lifecycle stays inert.
        struct AttackEnvelope
        {
            // Current lifecycle phase (derived from the timers + gain).
            enum class Phase { Open, RampDown, RecoverSlow, RecoverNormal };

            static constexpr double kSlowTimeConstantSec = 0.080;   // RecoverSlow TC
            // Event arming durations.
            static constexpr double kOnsetRecoverSec      = 0.150;
            static constexpr double kOnsetRampDownSec     = 0.020;
            static constexpr double kRatioJumpRecoverSec  = 0.100;
            static constexpr double kRatioJumpRampDownSec = 0.015;

            float  gain              = 0.0f; // output multiplier in [0, 1]
            double normalAlpha       = 0.0;  // one-pole alpha from attackMs
            double slowAlpha         = 0.0;  // one-pole alpha from kSlowTimeConstantSec
            int    recoverRemaining  = 0;    // RecoverSlow samples left
            int    rampDownRemaining = 0;    // RampDown samples left

            // Recompute the one-pole alphas for a new sample rate / attack
            // time. Does NOT touch the gain or the timers (matches the old
            // setAttackTimeMs semantics).
            void computeAlphas (double sr, float attackTimeMs)
            {
                normalAlpha = (attackTimeMs > 0.0f)
                    ? (1.0 - std::exp (-1.0 / ((attackTimeMs * 0.001) * sr)))
                    : 0.0;
                slowAlpha = 1.0 - std::exp (-1.0 / (kSlowTimeConstantSec * sr));
            }

            // Initial gain: faded-down when an attack time is configured
            // (the note-on fade-in will bring it back up), fully open when
            // there is no envelope (attackMs == 0).
            void initGain (bool hasAttackEnvelope) noexcept
            {
                gain = hasAttackEnvelope ? 0.0f : 1.0f;
            }

            void armForOnset (double sr) noexcept
            {
                recoverRemaining  = static_cast<int> (sr * kOnsetRecoverSec);
                rampDownRemaining = static_cast<int> (sr * kOnsetRampDownSec);
            }

            void armForRatioJump (double sr) noexcept
            {
                recoverRemaining  = static_cast<int> (sr * kRatioJumpRecoverSec);
                rampDownRemaining = static_cast<int> (sr * kRatioJumpRampDownSec);
            }

            // Block-level note-on: hard snap to 0 (the output was silent
            // during the preceding gap, so no step is audible), then the
            // normal-alpha fade-in takes over.
            void snapToZero() noexcept { gain = 0.0f; }

            // Neutralise the timers (external-driver mode keeps them inert).
            void clearTimers() noexcept { recoverRemaining = 0; rampDownRemaining = 0; }

            // Pin the gain fully open (envelope disabled / bypassed).
            void forceOpen() noexcept { gain = 1.0f; }

            Phase phase() const noexcept
            {
                if (rampDownRemaining > 0) return Phase::RampDown;
                if (recoverRemaining > 0)  return Phase::RecoverSlow;
                return (gain >= 1.0f) ? Phase::Open : Phase::RecoverNormal;
            }

            // Advance the automaton by one sample and return the gain to
            // apply to the output. Arithmetic is identical to the original
            // inline implementation (slowAttackSamplesRemaining /
            // attackRampDownSamplesRemaining version).
            float processSample() noexcept
            {
                const double alpha  = (recoverRemaining > 0) ? slowAlpha : normalAlpha;
                const float  target = (rampDownRemaining > 0) ? 0.0f : 1.0f;
                if (rampDownRemaining > 0) --rampDownRemaining;
                if (recoverRemaining  > 0) --recoverRemaining;
                if (gain < 1.0f || target < 1.0f)
                    gain += (target - gain) * static_cast<float> (alpha);
                else
                    gain = 1.0f;
                return gain;
            }
        };

        // Instances of the two automata above. They drive the per-sample
        // behaviour of process(); see the struct comments for the full
        // lifecycle documentation.
        AttackEnvelope attackEnvelope;
        VoiceActivityDetector voiceDetector;

        struct Grain {
            double readPos = 0.0;
            double speed = 1.0;
            double phase = 0.0;
            double phaseInc = 0.0;
            double gain = 1.0;
            bool active = false;
            // Per-grain attack: fade in over first N samples to avoid clicks
            // when reading from ring buffer positions that may have discontinuities
            float attackGain = 0.0f;
            double attackAlpha = 0.0;
        };
        static constexpr int MAX_GRAINS = 32;
        Grain grains[MAX_GRAINS];
        
        double findBestOffset (double idealReadPos, double targetToMatch, double searchWindowMs, float f0, double maxOffset) const;
        float getInterpolatedSample(int channel, double readPos) const;

        // Reaction of the OLA grain chain to a voice onset / large pitch
        // jump: clamp outPhase so exactly ONE grain is created at the onset
        // (prevents the "trumpet" grain burst - Fix J) and invalidate
        // lastGrainCenter so the next grain re-aligns on a LOCAL pitch mark
        // (prevents the stale-center mis-alignment click 10-30 ms after the
        // jump - Fix K2/O). Independent of the attack envelope: this always
        // runs, even when an external helper owns the gain.
        void restartGrainChainOnOnset() noexcept;
    };
}


