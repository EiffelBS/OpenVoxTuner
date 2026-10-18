#pragma once
// HarmonyLayoutTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The Harmony frame is the band's only growable block, and its two zones are sized by POLICY
// (ui/OVTHarmonyLayout.h): the main zone must hold the widest control it carries - the harmony
// type combo, whose longest entry must never truncate in any shipped language - and the expanded
// zone must hold two full-size dials with air around them. Both are measured here from the same
// numbers the editor's layout reads, so a future rename or font change fails a test instead of
// quietly clipping a word or gluing two knobs together.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

#include "../../Source/ui/OVTHarmonyLayout.h"

class HarmonyLayoutTest : public juce::UnitTest
{
public:
    HarmonyLayoutTest() : juce::UnitTest ("HarmonyLayout") {}

    void runTest() override
    {
        const auto savedLanguage = ovt::currentLanguage();

        const ovt::Language shipped[] = { ovt::Language::English, ovt::Language::French,
                                          ovt::Language::German,  ovt::Language::Spanish,
                                          ovt::Language::Japanese, ovt::Language::Chinese };

        beginTest ("the harmony type combo never truncates, in any shipped language");

        for (const auto language : shipped)
        {
            ovt::currentLanguage() = language;

            const auto items = ovt::harmonyTypeItems();
            expectEquals (items.size(), ovt::kHarmonyTypeCount,
                          "the shipped type list has one entry per harmony_type choice");

            const int longest = ovt::harmonyTypeTextWidth();
            const int need    = ovt::harmonyTypeComboNeed();
            const int pillRow = ovt::harmonyUseVoiceRowNeed();
            const int zone    = ovt::harmonyMainZoneW();

            expectGreaterThan (longest, 0, "the longest type entry paints glyphs");
            expectGreaterOrEqual (zone, ovt::kHarmonyMainZoneFloor,
                                  "the main zone never goes below its floor");
            expectLessOrEqual (need, zone,
                               "the longest type entry plus the arrow fits the main zone");
            expectLessOrEqual (pillRow, zone,
                               "the use-voice caption, switch and swatch fit the main zone");
            expectGreaterOrEqual (zone, pillRow, "the main zone is sized by its widest row, not guessed");

            // Every entry, not only the longest one: a future rename that beats the current
            // longest must widen the frame, and this is what would catch it.
            for (const auto& item : items)
            {
                const int w = static_cast<int> (std::ceil (
                    juce::GlyphArrangement::getStringWidth (ebs::fontComboBox(), item)));
                expectLessOrEqual (w + ovt::kHarmonyComboArrowW, zone,
                                   "every shipped type entry fits the combo");
            }

            std::cout << "       harmony type: " << ovt::languageDisplayName (language)
                      << " longest " << longest << " px, combo needs " << need
                      << " px, use-voice row " << pillRow
                      << " px, main zone " << zone << " px\n";
        }

        beginTest ("the expanded zone holds two full-size dials with air around them");

        // Each dial column must be wide enough for a full-size 54 px dial PLUS its air on both
        // sides: that clearance is what makes two knobs read as two controls.
        expectGreaterOrEqual ((ovt::kHarmonyKnobZoneW - ovt::kHarmonyKnobGap) / 2,
                              54 + 2 * ovt::kHarmonyKnobAir,
                              "each dial column holds a full-size dial with air on both sides");
        expectGreaterOrEqual (ovt::kHarmonyKnobZoneW,
                              2 * 54 + 2 * ovt::kHarmonyKnobAir + ovt::kHarmonyKnobGap,
                              "the preferred knob zone is two 54 px dials plus their air");
        expectLessThan (ovt::kHarmonyKnobZoneFloorW, ovt::kHarmonyKnobZoneW,
                        "the floor sits below the preferred zone, so a tight window shrinks the dials "
                        "before the row scrolls");
        expectGreaterOrEqual ((ovt::kHarmonyKnobZoneFloorW - ovt::kHarmonyKnobGap) / 2, 40,
                              "each dial column still holds a 40 px dial at the floor");

        // The air between the two dials is what the human asked for: it must be real clearance,
        // not the band's own 8 px row gap.
        expectGreaterThan (ovt::kHarmonyKnobGap, 8,
                           "the two dial columns are separated by more than the band's row gap");

        std::cout << "       harmony knob zone: " << ovt::kHarmonyKnobZoneW << " px preferred, "
                  << ovt::kHarmonyKnobZoneFloorW << " px floor, "
                  << ovt::kHarmonyKnobGap << " px between the columns\n";

        ovt::currentLanguage() = savedLanguage;
    }
};

static HarmonyLayoutTest harmonyLayoutTest;
