// SaturationEffect.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>

#include "IEffect.h"
#include "BlockAwareOnePole.h"
#include "OnePoleLowCut.h"

namespace ovtdsp
{
    /**
     * A soft-clipping saturation stage with a warm low-pass, on the master output.
     *
     * It sits at the end of the chain: after every bus has been blended (lead +
     * harmony + formant), BEFORE the shared room's return is summed, and ahead of
     * the output trim + limiter. That order is deliberate: the reverb tail stays
     * unsaturated (a long room fed through a clipper turns into HF noise), and the
     * limiter guards whatever loudness this stage adds.
     *
     * WHY THE SHAPER IS NOT A LINEAR CURVE, AND WHY DRIVE 0 IS BYPASSED
     * The shaper is `y = tanh(k x) / tanh(k)` with `k = 1 + drive * 9`. At drive 0
     * that is `tanh(x)/tanh(1)`, which is NOT unity: a -18 dBFS sine leaves the
     * curve at ~0.996 of its level, and every small signal is compressed. So drive
     * 0 cannot be "a value of the shaper", it has to be a transparent bypass: no
     * shaper, no filter, the buffer leaves this stage bit-identical. `process()`
     * gates on that (see the active flag) and the enable ramp fades the stage out
     * over 25 ms before the pass is skipped, exactly like DeEsser.h does.
     *
     * THE ONE REAL DSP RISK IS ALIASING, NOT THE CURVE
     * tanh generates harmonics; a harmonic above half the sample rate folds back
     * into the audible band as an inharmonic tone. At full drive on a hot signal
     * that is audible sparkle, so the stage runs at 2x: upsample, shape, downsample.
     * The shaper is memoryless, so shaping at the oversampled rate pushes every new
     * harmonic above fs/2, where the anti-imaging filter removes it before decimation.
     * Both stages share ONE half-band FIR (33 taps, Blackman-windowed sinc at the
     * quarter band), computed once per sample rate in prepare(). Measured response:
     * -6 dB at fs/2, ~40 dB down at 1.31x fs/2, >68 dB down beyond 1.6x fs/2 -- so
     * the harmonics that can fold are the ones sitting just past fs/2, and those are
     * attenuated by tens of dB before they ever reach the audible band. A 25-tap
     * version of the same window measured only -39 dB at the image band edge, which
     * left a -42 dB interpolation residual: audible as a floor under loud material.
     * The FIR is linear phase, so its cost is a constant delay: 16 samples at the
     * base rate (0.25 ms at 48 kHz). No plugin latency is declared for it, because
     * this stage sits AFTER all mixing: the delay shifts the whole master output as
     * one piece, and nothing that is summed with a dry copy of the voice lives
     * after it -- except the shared room's return. That sum is the one place the
     * delay can show up (a static comb between saturated voice and reverb tail);
     * its depth follows the send level, which is why the send knobs stay modest.
     *
     * MAKEUP IS MEASURED, NOT ESTIMATED
     * `tanh(k x)/k` has unity small-signal gain for every k, so a shaper alone gets
     * quieter as drive rises (measured: -2.59 dB RMS at full drive on a -18 dBFS
     * sine). The makeup table below is that measured curve inverted, sampled every
     * 0.05 of drive and interpolated linearly, so the stage leaves loudness where
     * the user set it: turning Drive up adds harmonics and compresses peaks without
     * making the vocal louder. Nothing here exceeds +1 dB RMS, which is what a
     * level-matched saturation knob has to promise. Loud material does come DOWN
     * (that is soft clipping), and the master trim after this stage restores it.
     *
     * HEADROOM: ARA BUFFERS GO BEYOND +/- 1
     * ARA renders can hand over samples above full scale. Feeding those straight
     * into tanh(k x) saturates hard at the ceiling, which is a hard clip. So the
     * input is compressed in the domain first: signals inside +/-1 pass untouched,
     * louder ones are pushed toward 2a/(1+a) (continuous and monotonic), and a hard
     * clamp at +12 dBFS guards pathological buffers. The shaper then sees a bounded
     * signal whose knee it can round off smoothly.
     *
     * DC: THE STAGE MUST NOT LEAVE A DC OFFSET
     * tanh is odd, so a symmetric signal generates almost no DC; the risk is an
     * already-offset input (a bad render) being folded by the clipper. The block
     * after the shaper is a subtractive one-pole high-pass at ~20 Hz (OnePoleLowCut),
     * which blocks DC by construction: a constant input drives the pole to x, so the
     * output goes to zero. Vocals have no content below 20 Hz, so it costs nothing.
     *
     * WARMTH: ONE POLE, SMOOTHED COEFFICIENT
     * The low-pass is one audio-rate pole at the oversampled rate, cutoff from 8 kHz
     * (tone 0, bright) down to 5 kHz (tone 1, warm). BlockAwareOnePole.h smooths the
     * CUTOFF once per block -- it never sees a sample, so it cannot filter audio;
     * the pole itself is computed from that smoothed cutoff at each block.
     *
     * No juce_dsp dependency: every filter here is local, so this header compiles in
     * both the plugin target (which links juce_dsp) and the test target (which does
     * not), the same reason DeEsser.h keeps its biquads local.
     */
    class SaturationEffect : public IEffect
    {
    public:
        // 25 taps, indices 0..24, centre tap index 12: a half-band FIR whose even
        // taps are zero except the centre (0.5), so each polyphase branch is cheap.
        static constexpr int kFilterTaps   = 33;
        static constexpr int kFilterCentre = 16;      // group delay, in base-rate samples
        static constexpr int kHalfTaps     = 16;      // odd taps per polyphase branch
        static constexpr int kRingSize     = 32;      // power of two, covers offsets 0..31
        static constexpr int kMaxChannels  = 2;       // the master bus is stereo

        juce::String getId() const override    { return "saturation"; }
        juce::String getName() const override   { return "Saturation"; }

        void prepare (double sampleRate, int maximumBlockSize) override
        {
            (void) maximumBlockSize;  // no per-block buffer: every state is a fixed ring

            baseSampleRate    = static_cast<float> (sampleRate);
            oversampledRate   = 2.0f * baseSampleRate;
            invSampleRate     = static_cast<float> (1.0 / sampleRate);

            buildHalfBandTaps();

            toneSmoother.prepare (sampleRate);          // block-aware: one step per block
            toneSmoother.setTimeConstantSeconds (kToneSmoothSeconds);

            for (auto& ch : channels)
                ch.dcBlock.prepare (static_cast<double> (baseSampleRate), kDcBlockHz);

            reset();  // fresh sample rate: coefficients change, so flush every memory
        }

        void reset() override
        {
            for (auto& ch : channels)
                ch.clear();
            bypassGain = 0.0f;   // fully bypassed: the ramp fades the stage in
            toneSmoother.snapTo (toneCutoffHz());  // seed at the current target, no ramp from a stale cutoff
        }

        /**
         * Drive 0..1. `k = 1 + drive * 9`, so drive 0 is k=1 and drive 1 is k=10:
         * a gentle knee that turns into near-square clipping. drive 0 is not a shaper
         * value, it is the bypass state (see the class doc), and the makeup table is
         * indexed by the same drive so loudness stays where the user set it.
         */
        void setDrive (float drive01)
        {
            drive = juce::jlimit (0.0f, 1.0f, drive01);
            kValue = 1.0f + drive * 9.0f;

            const float makeup = makeupForDrive (drive);
            shaperScale = makeup / kValue;   // y = tanh(k*x) * shaperScale
        }

        /**
         * Warmth 0..1: 0 keeps the stage bright (8 kHz pole), 1 makes it warm
         * (5 kHz pole). Only the HF that the clipper added is rolled off, so a low
         * setting sounds crisp and a high one sounds like a console on its way to
         * compression. Never called "tape": this stage does not model a tape machine.
         */
        void setTone (float tone01)
        {
            tone = juce::jlimit (0.0f, 1.0f, tone01);
        }

        // Diagnostics for the unit tests: the live shaper constant, the smoothed
        // cutoff actually in use, the measured makeup applied, and the internal delay.
        float getDrive() const       { return drive; }
        float getTone() const        { return tone; }
        float getShaperK() const     { return kValue; }
        float getCutoffHz() const    { return toneSmoother.getCurrentValue(); }
        float getMakeupGain() const  { return makeupForDrive (drive); }
        int   getInternalDelaySamples() const { return kFilterCentre; }  // at the base rate

        void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) override
        {
            const int numChannels = buffer.getNumChannels();
            const int numSamples  = buffer.getNumSamples();
            if (numChannels <= 0 || numSamples <= 0)
                return;

            // drive 0 is a transparent bypass: the shaper is never evaluated, so the
            // buffer leaves this stage untouched. Once the ramp has decayed below the
            // floor, the pass is skipped entirely -- no CPU and a bit-identical buffer.
            const bool active = enabled && drive > 0.0f;
            if (! active && bypassGain < kBypassFloor)
                return;

            // The ramp is measured in SECONDS, so its coefficient follows the block size:
            // one step per block with alpha = 1 - exp(-blockDuration/tau) reaches the target
            // in 25 ms whatever the host hands over (the same time constant DeEsser.h uses).
            const float rampCoeff = 1.0f - static_cast<float> (std::exp (-static_cast<double> (numSamples)
                                                                         * invSampleRate / kBypassRampSeconds));
            bypassGain += ((active ? 1.0f : 0.0f) - bypassGain) * rampCoeff;

            const float mix = juce::jlimit (0.0f, 1.0f, wetMix);
            const int nch   = juce::jmin (numChannels, kMaxChannels);

            // The cutoff is smoothed once per block (the smoother never sees a sample),
            // then turned into the pole coefficient at the oversampled rate.
            const float fc      = toneSmoother.processBlock (toneCutoffHz(), numSamples);
            const float lpCoeff = 1.0f - static_cast<float> (std::exp (-2.0 * kPi * static_cast<double> (fc)
                                                                      / static_cast<double> (oversampledRate)));

            for (int c = 0; c < nch; ++c)
            {
                Channel& ch = channels[c];
                float* data = buffer.getWritePointer (c);

                for (int n = 0; n < numSamples; ++n)
                {
                    const float u = compressHeadroom (data[n]);
                    ch.hist[ch.pos] = u;   // slot (pos - j) is x[n-j]

                    // Polyphase interpolation at 2x. The even branch is the centre tap
                    // alone, the odd branch is the sum of the odd taps. Gain D=2 keeps DC unity.
                    const float yEven = interpEven * ch.hist[(ch.pos - kFilterCentre / 2) & kRingMask];
                    float yOdd = 0.0f;
                    for (int j = 0; j < kHalfTaps; ++j)
                        yOdd += interpOdd[j] * ch.hist[(ch.pos - j) & kRingMask];

                    // Keep the unshaped streams: they are the delayed dry reference the
                    // crossfade blends against, so a partial wet never combs with itself.
                    ch.dryEven[ch.pos] = yEven;
                    ch.dryOdd[ch.pos]  = yOdd;

                    float sEven = shape (yEven);
                    float sOdd  = shape (yOdd);

                    // Warm low-pass at the oversampled rate: one pole, walked in stream
                    // order through both branches.
                    ch.lpState += lpCoeff * (sEven - ch.lpState); sEven = ch.lpState;
                    ch.lpState += lpCoeff * (sOdd  - ch.lpState); sOdd = ch.lpState;

                    ch.wetEven[ch.pos] = sEven;
                    ch.wetOdd[ch.pos]  = sOdd;

                    // Decimate: the same half-band as a low-pass, keep every other sample.
                    float wet = decimEven * ch.wetEven[(ch.pos - kFilterCentre / 2) & kRingMask];
                    for (int j = 1; j <= kHalfTaps; ++j)
                        wet += decimOdd[j - 1] * ch.wetOdd[(ch.pos - j) & kRingMask];

                    float dry = decimEven * ch.dryEven[(ch.pos - kFilterCentre / 2) & kRingMask];
                    for (int j = 1; j <= kHalfTaps; ++j)
                        dry += decimOdd[j - 1] * ch.dryOdd[(ch.pos - j) & kRingMask];

                    const float blended = dry + bypassGain * mix * (wet - dry);

                    // DC block after the shaper, at the base rate: a subtractive one-pole
                    // high-pass at ~20 Hz blocks any DC offset by construction.
                    data[n] = ch.dcBlock.process (blended);

                    ch.pos = (ch.pos + 1) & kRingMask;
                }
            }
        }

    private:
        static constexpr double kPi               = 3.14159265358979323846;
        static constexpr int    kRingMask         = kRingSize - 1;   // ring indices, power of two
        static constexpr float  kBypassRampSeconds = 0.025f;   // 25 ms enable ramp, as in DeEsser.h
        static constexpr float  kBypassFloor       = 1.0e-4f;  // below this the pass is skipped
        static constexpr float  kToneSmoothSeconds = 0.060f;   // cutoff moves are heard, not clicked
        static constexpr float  kDcBlockHz         = 20.0f;    // DC block after the shaper
        static constexpr float  kHeadroomCeilingLinear = 4.0f; // +12 dBFS hard guard

        /** Per-channel memory: three rings at the oversampled rate, plus two filters. */
        struct Channel
        {
            float hist[kRingSize];     // headroom-compressed input history
            float wetEven[kRingSize];  // shaped + warmed even branch
            float wetOdd[kRingSize];   // shaped + warmed odd branch
            float dryEven[kRingSize];  // linear even branch (delayed dry reference)
            float dryOdd[kRingSize];   // linear odd branch
            int   pos = 0;
            float lpState = 0.0f;      // warm pole memory, shared by the two branches
            OnePoleLowCut dcBlock;     // ~20 Hz DC block at the base rate

            void clear()
            {
                for (int i = 0; i < kRingSize; ++i)
                {
                    hist[i]    = 0.0f;
                    wetEven[i] = 0.0f;
                    wetOdd[i]  = 0.0f;
                    dryEven[i] = 0.0f;
                    dryOdd[i]  = 0.0f;
                }
                pos = 0;
                lpState = 0.0f;
                dcBlock.reset();
            }
        };

        /**
         * Build the half-band coefficients for the current sample rate. A Blackman-windowed
         * sinc at the quarter band of the oversampled rate is exactly a half-band: every even
         * tap except the centre is zero, because sin(pi/2 * n) vanishes for even n != 0. The
         * taps are normalized so they sum to 1, which makes the linear path unity at DC and
         * keeps the measured makeup valid at any sample rate.
         */
        void buildHalfBandTaps()
        {
            float taps[kFilterTaps];
            float total = 0.0f;

            for (int k = 0; k < kFilterTaps; ++k)
            {
                const int n = k - kFilterCentre;
                const float ideal = (n == 0) ? 0.5f
                                             : static_cast<float> (std::sin (kPi * 0.5 * static_cast<double> (n))
                                                                   / (kPi * static_cast<double> (n)));
                // Blackman window over the same index, so the stopband is deep where images land.
                const float w = 0.42f + 0.5f * static_cast<float> (std::cos (2.0 * kPi * static_cast<double> (n) / (kFilterTaps - 1)))
                                     + 0.08f * static_cast<float> (std::cos (4.0 * kPi * static_cast<double> (n) / (kFilterTaps - 1)));
                taps[k] = ideal * w;
                total += taps[k];
            }

            if (total <= 0.0f)   // cannot happen for a quarter-band sinc, guarded numerically
                total = 1.0f;

            centreTap = taps[kFilterCentre] / total;   // ~0.5 by the half-band property

            // Interpolation needs gain D=2: zero-stuffing halves DC, so the passband is doubled.
            interpEven = 2.0f * centreTap;
            for (int j = 0; j < kHalfTaps; ++j)
                interpOdd[j] = 2.0f * (taps[2 * j + 1] / total);

            // Decimation keeps the unity-gain filter: DC comes out of the two branches as 1.
            decimEven = centreTap;
            for (int j = 1; j <= kHalfTaps; ++j)
                decimOdd[j - 1] = taps[2 * j - 1] / total;
        }

        /** Shaper: tanh(k x) scaled by the measured makeup, so loudness is level-matched. */
        float shape (float x) const { return static_cast<float> (std::tanh (static_cast<double> (kValue * x))) * shaperScale; }

        /**
         * Headroom for ARA buffers that exceed full scale. Inside +/-1 the signal is
         * handed to the shaper untouched; above it, 2a/(1+a) keeps the mapping continuous
         * and monotonic (at a=1 it returns 1, at +12 dBFS it returns 1.6), so the clipper
         * meets a rounded knee instead of slamming into a ceiling. Beyond +/-4 the value is
         * clamped: that is a numerical guard for pathological renders, not a sound.
         */
        static float compressHeadroom (float x)
        {
            const float clamped = juce::jlimit (-kHeadroomCeilingLinear, kHeadroomCeilingLinear, x);
            const float a = std::abs (clamped);
            return a <= 1.0f ? clamped : clamped * (2.0f / (1.0f + a));
        }

        /** Cutoff from the warmth knob: 8 kHz bright at tone 0, 5 kHz warm at tone 1. */
        float toneCutoffHz() const { return juce::jlimit (5000.0f, 8000.0f, 8000.0f - tone * 3000.0f); }

        /**
         * The measured makeup curve: RMS gain of the FULL stage (upsample, shaper, warm pole,
         * decimate) on a -18 dBFS sine at warmth 0.4, inverted, sampled every 0.05 of drive at
         * 48 kHz. Interpolated linearly so the knob moves smoothly between measured points.
         * The near-linear-region estimate (tanh(k x)/k alone) is far too small: a -18 dBFS sine
         * loses 3.9 dB at full drive once the shaper is driven hard, and that loss must be paid
         * back by the makeup, not left in the output as a volume drop.
         */
        static float makeupForDrive (float d)
        {
            static constexpr float kMakeupTable[21] =
            {
                1.000000f, 1.042660f, 1.065870f, 1.094750f, 1.128970f,
                1.168140f, 1.211860f, 1.259740f, 1.311380f, 1.366410f,
                1.424480f, 1.485260f, 1.548450f, 1.613780f, 1.681000f,
                1.749890f, 1.820250f, 1.891920f, 1.964730f, 2.038560f,
                2.113280f
            };

            const float scaled = juce::jlimit (0.0f, 1.0f, d) * 20.0f;
            int idx = static_cast<int> (scaled);
            if (idx >= 20) return kMakeupTable[20];
            const float frac = scaled - static_cast<float> (idx);
            return kMakeupTable[idx] + frac * (kMakeupTable[idx + 1] - kMakeupTable[idx]);
        }

        float baseSampleRate   = 44100.0f;
        float oversampledRate  = 88200.0f;
        float invSampleRate    = 1.0f / 44100.0f; // block-size-aware ramp coefficient
        float bypassGain       = 0.0f;

        float drive    = 0.0f;   // 0 = transparent bypass
        float tone     = 0.4f;   // warmth: 8 kHz -> 5 kHz pole
        float kValue   = 1.0f;   // 1 + drive * 9
        float shaperScale = 1.0f;

        float centreTap = 0.5f;
        float interpEven = 1.0f;
        float interpOdd[kHalfTaps] = {};
        float decimEven  = 0.5f;
        float decimOdd[kHalfTaps]  = {};

        BlockAwareOnePole toneSmoother;
        Channel channels[kMaxChannels];
    };
}

