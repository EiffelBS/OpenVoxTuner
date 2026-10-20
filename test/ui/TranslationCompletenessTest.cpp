#pragma once
// TranslationCompletenessTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Every string the English table carries must exist in every shipped language. A missing entry
// raises no error and shows no warning: ovt::tr() falls back to English, so a French, Japanese or
// Chinese user silently reads an English tooltip - and nobody notices, because the plugin looks
// fine. The Chinese table was missing the four curve-editor tooltips and the Japanese table was
// missing one MIDI badge until this test existed.
//
// The second half of the test is the rule the tooltips have to obey: a tooltip says what the
// control does and what it changes, for someone who has never seen the plugin. A tooltip that
// talks about the development history ("the room of the older versions", a formula, a dead
// feature's name) fails here, because that is exactly the text that has to stay out.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

#include "../../Source/ui/OVTLanguages.h"

class TranslationCompletenessTest : public juce::UnitTest
{
public:
    TranslationCompletenessTest() : juce::UnitTest ("TranslationCompleteness") {}

    void runTest() override
    {
        struct LanguageName { ovt::Language language; const char* name; };

        const LanguageName shipped[] = { { ovt::Language::French,   "French" },
                                         { ovt::Language::German,   "German" },
                                         { ovt::Language::Spanish,  "Spanish" },
                                         { ovt::Language::Japanese, "Japanese" },
                                         { ovt::Language::Chinese,  "Chinese" } };

        beginTest ("every English string exists in every shipped language");

        const auto& english = ovt::getTranslations (ovt::Language::English);

        for (const auto& entry : shipped)
        {
            const auto& table = ovt::getTranslations (entry.language);

            for (const auto& string : english)
            {
                expect (table.find (string.first) != table.end(),
                        "missing translation (" + juce::String (entry.name) + "): " + string.first);
            }
        }

        beginTest ("no tooltip talks about the development history");

        // The sentences that must never come back: the reverb of the OLDER VERSIONS (a tooltip
        // that tells the user what the plugin used to be instead of what it does), the DSP
        // formulas, and the names of the controls the row no longer carries.
        const char* const banned[] = { "older version", "ancienne version", "anciennes versions",
                                       "früheren Versionen", "versiones anteriores", "以前のバージョン",
                                       "旧版本", "1/sqrt", "RMS", "Haas", "allpass", "Pre-warp",
                                       "Attack-Aware", "attack-aware", "attack aware",
                                       "FlexTune", "flex, attack-aware" };

        for (const auto& string : english)
        {
            if (string.first.rfind ("tooltip.", 0) != 0)
                continue;

            const juce::String text (juce::CharPointer_UTF8 (string.second));

            for (const auto* phrase : banned)
            {
                expect (! text.containsIgnoreCase (phrase),
                        "tooltip still carries a development reference: " + string.first
                            + " (" + phrase + ")");
            }
        }

        beginTest ("every tooltip explains itself in the shipped languages");

        // A tooltip that is empty says nothing: every control the user can hover must say what
        // it does. The empty string is the only failure here - the wording itself is a matter of
        // taste, not of a test.
        for (const auto& entry : shipped)
        {
            const auto& table = ovt::getTranslations (entry.language);

            for (const auto& string : english)
            {
                if (string.first.rfind ("tooltip.", 0) != 0)
                    continue;

                const auto it = table.find (string.first);
                expect (it != table.end() && juce::String (it->second).isNotEmpty(),
                        "empty tooltip (" + juce::String (entry.name) + "): " + string.first);
            }
        }
    }
};

static TranslationCompletenessTest translationCompletenessTest;
