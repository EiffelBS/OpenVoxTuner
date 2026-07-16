// AttackAwareTest.cpp
// Unit tests for ovtdsp::AttackAwareEnv (attack-aware correction helper).

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/AttackAwareEnv.h"

class AttackAwareEnvTest : public juce::UnitTest
{
public:
    AttackAwareEnvTest() : juce::UnitTest ("AttackAwareEnv") {}

    void runTest() override
    {
        const float blockDur = 0.046f; // ~2048 samples @ 44.1 kHz

        beginTest ("Disabled always returns gain 1.0 (transparent)");
        {
            ovtdsp::AttackAwareEnv e;
            e.setReleaseSeconds (0.06f);
            e.setEnabled (false);
            expectWithinAbsoluteError (e.process (0.5f, blockDur), 1.0f, 1.0e-6f);
            expectWithinAbsoluteError (e.process (0.9f, blockDur), 1.0f, 1.0e-6f);
        }

        beginTest ("Enabled, steady level keeps gain at 1.0 (no false onset)");
        {
            ovtdsp::AttackAwareEnv e;
            e.setReleaseSeconds (0.06f);
            e.setEnabled (true);
            e.reset();
            float g = e.process (0.4f, blockDur);
            expectWithinAbsoluteError (g, 1.0f, 1.0e-5f);
            g = e.process (0.4f, blockDur);
            expectWithinAbsoluteError (g, 1.0f, 1.0e-5f);
        }

        beginTest ("Onset drops gain to 0, then ramps back over the release time");
        {
            ovtdsp::AttackAwareEnv e;
            e.setReleaseSeconds (0.06f); // rampPerBlock = 0.046/0.06 ~= 0.7667
            e.setEnabled (true);
            e.reset();
            e.process (0.1f, blockDur);    // establish a baseline level
            const float g0 = e.process (0.8f, blockDur); // sharp rise -> onset
            expectWithinAbsoluteError (g0, 0.0f, 1.0e-5f, "gain should drop to 0 at onset");
            const float g1 = e.process (0.8f, blockDur); // ramp up
            expectWithinAbsoluteError (g1, 0.7667f, 1.0e-2f, "gain should ramp toward 1");
            const float g2 = e.process (0.8f, blockDur); // clamps to 1
            expectWithinAbsoluteError (g2, 1.0f, 1.0e-5f, "gain should reach full correction");
        }

        beginTest ("Longer release time ramps back more slowly");
        {
            ovtdsp::AttackAwareEnv e;
            e.setReleaseSeconds (1.0f);  // rampPerBlock = 0.046
            e.setEnabled (true);
            e.reset();
            e.process (0.1f, blockDur);
            e.process (0.8f, blockDur);   // onset -> 0
            const float g = e.process (0.8f, blockDur);
            expectWithinAbsoluteError (g, 0.046f, 1.0e-3f, "slow release: small ramp per block");
            expect (g < 0.7667f, "slow release must ramp slower than the 60ms case");
        }

        beginTest ("reset() restores gain to 1.0 and clears state");
        {
            ovtdsp::AttackAwareEnv e;
            e.setReleaseSeconds (0.06f);
            e.setEnabled (true);
            e.reset();
            e.process (0.1f, blockDur);
            e.process (0.8f, blockDur);   // gain -> 0
            e.reset();
            const float g = e.process (0.1f, blockDur);
            expectWithinAbsoluteError (g, 1.0f, 1.0e-5f, "after reset, gain should be 1");
        }

        beginTest ("Slow swell does not trigger an onset");
        {
            ovtdsp::AttackAwareEnv e;
            e.setReleaseSeconds (0.06f);
            e.setEnabled (true);
            e.reset();
            // Gentle monotonic rise: each step is +10% (below kRiseRatio 1.2) -> no onset.
            float g = 1.0f;
            for (float r = 0.1f; r <= 0.5f; r += 0.04f)
                g = e.process (r, blockDur);
            expectWithinAbsoluteError (g, 1.0f, 1.0e-3f, "slow swell must not ease the correction");
        }
    }
};

static AttackAwareEnvTest attackAwareEnvTest;
