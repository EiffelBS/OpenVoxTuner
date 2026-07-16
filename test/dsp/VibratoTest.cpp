// VibratoTest.cpp
// Unit tests for ovtdsp::VibratoPreserver (vibrato preservation helper).

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include "../../Source/dsp/VibratoPreserver.h"

class VibratoPreserverTest : public juce::UnitTest
{
public:
    VibratoPreserverTest() : juce::UnitTest ("VibratoPreserver") {}

    void runTest() override
    {
        // --- Center tracking on a steady pitch ------------------------------
        beginTest ("Center tracker converges to a steady pitch");
        {
            ovtdsp::VibratoPreserver vp;
            for (int i = 0; i < 50; ++i)
                vp.update (220.0f);
            expectWithinAbsoluteError (vp.getCenter(), 220.0f, 1.0e-3f,
                                       "center should equal the steady pitch");
        }

        // --- Silence holds the last center, no jump ------------------------
        beginTest ("Silence holds the center (re-attack does not jump)");
        {
            ovtdsp::VibratoPreserver vp;
            for (int i = 0; i < 20; ++i)
                vp.update (330.0f);
            for (int i = 0; i < 10; ++i)
                vp.update (0.0f); // silence
            expectWithinAbsoluteError (vp.getCenter(), 330.0f, 1.0e-3f,
                                       "center must hold during silence");
        }

        // --- reset() clears the center -------------------------------------
        beginTest ("reset() clears the tracked center");
        {
            ovtdsp::VibratoPreserver vp;
            for (int i = 0; i < 20; ++i)
                vp.update (440.0f);
            vp.reset();
            expect (vp.getCenter() <= 0.0f, "center should be cleared after reset");
        }

        // --- Low-pass smooths a pitch step --------------------------------
        beginTest ("Low-pass smooths a step change toward the new pitch");
        {
            ovtdsp::VibratoPreserver vp;
            for (int i = 0; i < 50; ++i)
                vp.update (220.0f);
            // Step up to 440; after a few blocks it must move upward but not
            // jump instantly (low-pass is gradual).
            vp.update (440.0f);
            const float afterOne = vp.getCenter();
            expect (afterOne > 220.0f, "center should start rising");
            expect (afterOne < 440.0f, "center should not jump in one block");
            for (int i = 0; i < 200; ++i)
                vp.update (440.0f);
            expectWithinAbsoluteError (vp.getCenter(), 440.0f, 1.0e-2f,
                                       "center should converge to the new pitch");
        }

        // --- blend: preserve == 0 keeps the instantaneous ratio -----------
        beginTest ("blend(preserve=0) returns the instantaneous ratio");
        {
            ovtdsp::VibratoPreserver vp;
            for (int i = 0; i < 50; ++i)
                vp.update (220.0f);
            const float instant = 1.5f; // arbitrary instantaneous ratio
            const float centerRatio = vp.blend (instant, 220.0f, 242.0f, 0.0f);
            expectWithinAbsoluteError (centerRatio, instant, 1.0e-6f,
                                       "preserve=0 must pass through instant ratio");
        }

        // --- blend: preserve == 1 uses the center-based ratio -------------
        beginTest ("blend(preserve=1) uses the constant center ratio");
        {
            ovtdsp::VibratoPreserver vp;
            for (int i = 0; i < 50; ++i)
                vp.update (220.0f);
            const float targetCenter = 242.0f;        // ratioCenter = 1.1
            const float ratioC = vp.blend (1.0f, 220.0f, targetCenter, 1.0f);
            expectWithinAbsoluteError (ratioC, 1.1f, 1.0e-5f,
                                       "preserve=1 must return targetCenter/center");
        }

        // --- Core contract: vibrato is preserved at preserve=1 ------------
        // Classic correction (preserve=0) snaps every instantaneous pitch to
        // the target, flattening the vibrato. The center-based correction
        // (preserve=1) re-applies a constant ratio to f0, so the modulation
        // around the center survives.
        beginTest ("Vibrato modulation survives at preserve=1 (destroyed at 0)");
        {
            ovtdsp::VibratoPreserver vp;
            const float centerHz = 220.0f;
            for (int i = 0; i < 50; ++i)
                vp.update (centerHz);
            const float targetCenter = centerHz * 1.1f; // ratioCenter = 1.1

            // Vibrato: +/- 50 cents around the center pitch.
            const float f0Low  = centerHz * std::pow (2.0f, -50.0f / 1200.0f);
            const float f0High = centerHz * std::pow (2.0f,  50.0f / 1200.0f);

            // Instantaneous "snap to target" ratio for each f0.
            const float instantLow  = targetCenter / f0Low;
            const float instantHigh = targetCenter / f0High;

            // Classic path: output is always the target (flat).
            const float outLowClassic  = f0Low  * vp.blend (instantLow,  f0Low,  targetCenter, 0.0f);
            const float outHighClassic = f0High * vp.blend (instantHigh, f0High, targetCenter, 0.0f);
            expectWithinAbsoluteError (outLowClassic,  targetCenter, 1.0e-3f, "classic: low f0 snapped to target");
            expectWithinAbsoluteError (outHighClassic, targetCenter, 1.0e-3f, "classic: high f0 snapped to target");
            expectWithinAbsoluteError (outLowClassic, outHighClassic, 1.0e-4f,
                                       "classic path destroys the vibrato (flat output)");

            // Vibrato path: output scaled by constant ratioCenter = 1.1.
            const float outLowVib  = f0Low  * vp.blend (instantLow,  f0Low,  targetCenter, 1.0f);
            const float outHighVib = f0High * vp.blend (instantHigh, f0High, targetCenter, 1.0f);
            expectWithinAbsoluteError (outLowVib,  f0Low  * 1.1f, 1.0e-3f, "vibrato: low output = f0*1.1");
            expectWithinAbsoluteError (outHighVib, f0High * 1.1f, 1.0e-3f, "vibrato: high output = f0*1.1");
            // The output modulation (Low<->High) must mirror the input modulation.
            const float inMod  = f0High - f0Low;
            const float outMod = outHighVib - outLowVib;
            expectWithinAbsoluteError (outMod, inMod * 1.1f, 1.0e-2f,
                                       "vibrato: output modulation tracks input * ratioCenter");
            expect (std::abs (outMod) > 1.0e-2f,
                    "vibrato path must retain a non-zero modulation");
        }
    }
};

static VibratoPreserverTest vibratoPreserverTest;
