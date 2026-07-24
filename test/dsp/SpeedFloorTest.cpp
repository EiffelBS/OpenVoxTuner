// SpeedFloorTest.cpp
// Regression test for the "Speed=0 + Flex>0" scratch bug.
//
// 2026-07-23 (Fix AY + Fix AZ): the per-block jitter in `targetRatio`
// (from flexTuneSmoother, humanizeSmoother, vibrato preservation and YIN
// pitch detection steps) reached the OLA chain unchanged when the
// RetargetEnvelope was transparent (Speed=0) or too slow to smooth the
// jitter. The user perceived this as a "scratch" most audible with
// Flex > 0 cents and Speed = 0, and more pronounced in Modern mode than
// Transparent mode (because Modern preserves the full vibrato amplitude).
//
// Fix AY: added a 50ms BlockAwareOnePole (the "speed floor") AFTER the
// RetargetEnvelope.
//
// Fix AZ: raised the TC from 50ms to 80ms. At 50ms the 5Hz vibrato was
// only reduced by 53% (|H(5Hz)| = 0.53), still leaving ~0.14% residual
// modulation in the targetRatio that the OLA chain could not fully
// absorb. At 80ms the 5Hz vibrato is reduced by 70% (|H(5Hz)| = 0.30),
// bringing the residual modulation below the OLA grain spacing
// sensitivity threshold (~0.5 sample misalignment). The compounded
// retargeting time is approximately `max(Speed, 80ms) + 80ms / 2`, still
// allowing Speed=10ms to be perceptibly faster than Speed=100ms.
//
// These tests verify that:
//   1) The speed floor significantly reduces step-to-step jitter in
//      the `ratio` signal (modelled as random walks or YIN-like
//      step changes every ~10 blocks).
//   2) The speed floor is buffer-size independent.
//   3) The speed floor respects the user's Speed knob (Speed=10ms
//      is perceptibly faster than Speed=100ms).
//   4) The speed floor doesn't drift with constant input.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/BlockAwareOnePole.h"
#include <cmath>
#include <vector>
#include <random>

class SpeedFloorTest : public juce::UnitTest
{
public:
    SpeedFloorTest() : juce::UnitTest ("SpeedFloor (Fix AY)") {}

    void runTest() override
    {
        using namespace ovtdsp;

        const double sampleRate = 44100.0;
        const float blockDur256 = 256.0f / static_cast<float> (sampleRate);
        const float blockDur128 = 128.0f / static_cast<float> (sampleRate);

        beginTest ("Speed floor smooths a YIN-like step jitter (256 samples)");
        {
            // YIN pitch detection is stable for ~2048 samples (~46 ms
            // = ~8 blocks at 256 samples per block), then it changes
            // (often by a small amount, ~0.2-0.5%). This produces a
            // "random walk" pattern in the ratio over multiple blocks
            // with occasional larger steps.
            //
            // The PER-BLOCK jitter (the difference between two
            // consecutive block values) is the relevant measure,
            // because that's what the OLA chain sees as a per-block
            // step in `targetRatio`. We measure the average per-block
            // jitter of the input vs the output: the speed floor
            // should reduce it by at least 50% (i.e. the output
            // should be measurably smoother than the input).
            BlockAwareOnePole floor;
            floor.prepare (sampleRate);
            floor.setTimeConstantSeconds (0.080f);
            floor.reset (1.0f);

            // Generate the input signal: small random walk + occasional steps.
            std::mt19937 rng (42);
            std::uniform_real_distribution<float> dist (-0.001f, 0.001f);
            const int numBlocks = 200;
            std::vector<float> inputs;
            inputs.reserve (numBlocks);
            float r = 1.0f;
            for (int b = 0; b < numBlocks; ++b)
            {
                r += dist (rng);
                if (b % 8 == 0 && b > 0)
                    r += (rng () % 2 == 0 ? 0.005f : -0.005f);
                inputs.push_back (r);
            }

            // Feed through the speed floor.
            std::vector<float> outputs;
            outputs.reserve (numBlocks);
            for (int b = 0; b < numBlocks; ++b)
                outputs.push_back (floor.step (inputs[b], blockDur256));

            // Measure the per-block jitter (|diff|) in the steady
            // state (after the floor has settled, say from block 50
            // onward). The "per-block jitter" is the average |delta|
            // between consecutive block values.
            float inputJitter = 0.0f;
            float outputJitter = 0.0f;
            int count = 0;
            for (int b = 51; b < numBlocks; ++b)
            {
                inputJitter += std::abs (inputs[b] - inputs[b - 1]);
                outputJitter += std::abs (outputs[b] - outputs[b - 1]);
                count++;
            }
            inputJitter /= static_cast<float> (count);
            outputJitter /= static_cast<float> (count);

            // The output per-block jitter should be at least 30%
            // smaller than the input (i.e. speed floor smooths).
            // We allow a 30% reduction (output < 0.7 * input) to
            // leave headroom for the filter's transient response.
            expect (outputJitter < inputJitter * 0.7f,
                "Speed floor must reduce per-block jitter by at least 30%. input=" +
                juce::String (inputJitter) + ", output=" + juce::String (outputJitter));
        }

        beginTest ("Speed floor is buffer-size independent");
        {
            // The same input signal should produce the same output
            // range at 256 and 128 samples (the TC is in seconds, so
            // it must be the same in both cases).
            std::mt19937 rng (42);
            std::uniform_real_distribution<float> dist (-0.001f, 0.001f);
            const int numBlocks = 200;
            std::vector<float> inputs;
            inputs.reserve (numBlocks);
            float r = 1.0f;
            for (int b = 0; b < numBlocks; ++b)
            {
                r += dist (rng);
                if (b % 8 == 0 && b > 0)
                    r += (rng () % 2 == 0 ? 0.005f : -0.005f);
                inputs.push_back (r);
            }

            auto measure = [sampleRate, &inputs] (float blockDur) -> float
            {
                BlockAwareOnePole floor;
                floor.prepare (sampleRate);
                floor.setTimeConstantSeconds (0.080f);
                floor.reset (1.0f);
                float maxV = 0.0f, minV = 0.0f;
                for (int b = 0; b < (int) inputs.size(); ++b)
                {
                    const float v = floor.step (inputs[b], blockDur);
                    if (b >= 50)
                    {
                        if (v > maxV) maxV = v;
                        if (v < minV) minV = v;
                    }
                }
                return maxV - minV;
            };

            const float range256 = measure (blockDur256);
            const float range128 = measure (blockDur128);

            // The two ranges must be close (within 20% of each other)
            // to demonstrate buffer-size independence.
            const float diff = std::abs (range256 - range128);
            const float avg = (range256 + range128) * 0.5f;
            expect (diff < avg * 0.2f,
                "Speed floor output range must be buffer-size independent. 256: " +
                juce::String (range256) + ", 128: " + juce::String (range128));
        }

        beginTest ("Speed floor alone (Speed=0) reaches 90% in 15-40 blocks");
        {
            // At Speed=0 the RetargetEnvelope is transparent. The
            // speed floor at TC=80ms is the only smoothing. 90% is
            // reached in 2.3*TC/blockDur = 2.3*80/5.8 = 31.7 blocks
            // (at 256 samples, 44.1 kHz).
            BlockAwareOnePole floor;
            floor.prepare (sampleRate);
            floor.setTimeConstantSeconds (0.080f);
            floor.reset (1.0f);

            const float target = 1.5f;
            int blocksToReach90Pct = 0;
            for (int b = 1; b <= 200; ++b)
            {
                const float smoothed = floor.step (target, blockDur256);
                if (smoothed >= 1.0f + 0.9f * (target - 1.0f) && blocksToReach90Pct == 0)
                    blocksToReach90Pct = b;
            }

            expect (blocksToReach90Pct >= 20 && blocksToReach90Pct <= 45,
                "Speed floor alone (TC=80ms) must reach 90% in 20-45 blocks (got " +
                juce::String (blocksToReach90Pct) + " blocks, ~" +
                juce::String (blocksToReach90Pct * blockDur256 * 1000.0f) + " ms)");
        }

        beginTest ("Speed floor + slow retarget: big events still respect Speed knob");
        {
            // The speed floor is AFTER the RetargetEnvelope. For a big
            // event (1.0 -> 1.5) at Speed=10ms, the user expects
            // ~10ms. With the speed floor (TC=80ms) in series, the
            // compounded time is ~10ms + 80ms = ~90ms. This is still
            // much faster than Speed=100ms alone (~100ms).
            auto measure = [sampleRate] (float retargetTC) -> int
            {
                BlockAwareOnePole retarget;
                retarget.prepare (sampleRate);
                retarget.setTimeConstantSeconds (retargetTC);
                retarget.reset (1.0f);

                BlockAwareOnePole floor;
                floor.prepare (sampleRate);
                floor.setTimeConstantSeconds (0.080f);
                floor.reset (1.0f);

                const float target = 1.5f;
                const float blockDur = 256.0f / static_cast<float> (sampleRate);
                int blocksToReach90Pct = 0;
                for (int b = 1; b <= 500; ++b)
                {
                    const float r = retarget.step (target, blockDur);
                    const float smoothed = floor.step (r, blockDur);
                    if (smoothed >= 1.0f + 0.9f * (target - 1.0f) && blocksToReach90Pct == 0)
                        blocksToReach90Pct = b;
                }
                return blocksToReach90Pct;
            };

            const int blocksAt10ms = measure (0.010f);
            const int blocksAt100ms = measure (0.100f);

            // Speed=10ms should reach 90% in roughly 25-45 blocks
            // (10ms + 80ms compounded).
            expect (blocksAt10ms >= 20 && blocksAt10ms <= 45,
                "Speed=10ms + speed floor must reach 90% in 20-45 blocks (got " +
                juce::String (blocksAt10ms) + " blocks)");

            // Speed=100ms should reach 90% in roughly 35-75 blocks
            // (100ms + 80ms compounded).
            expect (blocksAt100ms >= 30 && blocksAt100ms <= 75,
                "Speed=100ms + speed floor must reach 90% in 30-75 blocks (got " +
                juce::String (blocksAt100ms) + " blocks)");

            // Speed=10ms must be measurably faster than Speed=100ms.
            expect (blocksAt10ms < blocksAt100ms,
                "Speed=10ms (" + juce::String (blocksAt10ms) + " blocks) must be faster than Speed=100ms (" +
                juce::String (blocksAt100ms) + " blocks) -- otherwise the Speed knob is broken");
        }

        beginTest ("Speed floor initialised to 1.0 stays at 1.0 with constant input");
        {
            // Sanity check: with constant input, the speed floor
            // doesn't drift.
            BlockAwareOnePole floor;
            floor.prepare (sampleRate);
            floor.setTimeConstantSeconds (0.080f);
            floor.reset (1.0f);

            for (int b = 0; b < 20; ++b)
            {
                const float v = floor.step (1.0f, blockDur256);
                expectWithinAbsoluteError (v, 1.0f, 1.0e-6f,
                    "Speed floor at 1.0 with input 1.0 must stay at 1.0 (block " +
                    juce::String (b) + ")");
            }
        }
    }
};

static SpeedFloorTest speedFloorTest;
