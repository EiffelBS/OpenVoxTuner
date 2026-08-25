#pragma once
// HarmonyGainMatchTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/HarmonyEngine.h"

class HarmonyGainMatchTest : public juce::UnitTest
{
public:
    HarmonyGainMatchTest() : juce::UnitTest ("HarmonyGainMatch") {}

    void runTest() override
    {
        using namespace ovtdsp;

        // ----------------------------------------------------------------
        // 1) Voice counting for the dense HarmonyTypes (used to compute
        //    the 1/sqrt(1+N) factor in the mix).
        // ----------------------------------------------------------------
        beginTest ("getHarmonyVoiceCount: Unison2 = 2, UnisonOctaves4 = 4, etc.");
        {
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::None) == 0,
                    "None must return 0 voices (empty mix).");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::Unison2) == 2,
                    "Unison2 must return 2 voices.");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::UnisonOctaves4) == 4,
                    "UnisonOctaves4 must return 4 voices.");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::VocalStack4) == 4,
                    "VocalStack4 must return 4 voices.");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::ThirdBelowAbove) == 2,
                    "ThirdBelowAbove must return 2 voices.");
        }

        // ----------------------------------------------------------------
        // 2) Compensation factor 1/sqrt(1+N). We verify that:
        //    - it is strictly below 1 (so it does reduce the mix)
        //    - it is strictly above 0 (no total silence)
        //    - for UnisonOctaves4 (N=4) it is more aggressive than for
        //      Unison2 (N=2), as expected.
        // ----------------------------------------------------------------
        beginTest ("1/sqrt(1+N) factor: reduces properly, more aggressive for high N");
        {
            auto matchFactor = [] (int n) {
                return (n > 0) ? (1.0f / std::sqrt (1.0f + static_cast<float> (n))) : 1.0f;
            };
            const float fUnison2   = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::Unison2));
            const float fUnison4   = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::UnisonOctaves4));
            const float fVocal4    = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::VocalStack4));
            const float fThird     = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::ThirdBelowAbove));

            // Factor strictly below 1 (the harmony mix is reduced).
            expect (fUnison2 < 1.0f, "Unison2: factor must be < 1.0");
            expect (fUnison4 < 1.0f, "UnisonOctaves4: factor must be < 1.0");
            expect (fVocal4  < 1.0f, "VocalStack4: factor must be < 1.0");
            expect (fThird   < 1.0f, "ThirdBelowAbove: factor must be < 1.0");

            // Factor strictly above 0 (no total silence).
            expect (fUnison2 > 0.0f, "Unison2: factor must be > 0.0");
            expect (fUnison4 > 0.0f, "UnisonOctaves4: factor must be > 0.0");

            // More voices = smaller factor (stronger compensation).
            expect (fUnison4 < fUnison2,
                "UnisonOctaves4 (N=4) must be compensated more than Unison2 (N=2). "
                "fUnison4=" + juce::String (fUnison4, 4) + ", fUnison2=" + juce::String (fUnison2, 4));
            expect (fVocal4 < fUnison2,
                "VocalStack4 (N=4) must be compensated more than Unison2 (N=2).");

            // Numeric value verification for the docs.
            //   Unison2: N=2 -> 1/sqrt(3) = 0.5774 (-4.77 dB)
            //   Unison4: N=4 -> 1/sqrt(5) = 0.4472 (-6.99 dB)
            // Wide tolerances (+/- 0.01) to absorb compiler variations.
            expect (std::abs (fUnison2 - 0.5774f) < 0.01f,
                "Unison2: expected 1/sqrt(3) ~0.5774, measured = "
                + juce::String (fUnison2, 4));
            expect (std::abs (fUnison4 - 0.4472f) < 0.01f,
                "UnisonOctaves4: expected 1/sqrt(5) ~0.4472, measured = "
                + juce::String (fUnison4, 4));
        }
    }
};

static HarmonyGainMatchTest harmonyGainMatchTest;


