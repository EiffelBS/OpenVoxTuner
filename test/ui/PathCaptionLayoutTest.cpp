#pragma once
// PathCaptionLayoutTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The voice caption under an Effects column must fit the FIXED cell the block gives it, in
// every language the plugin ships. It is the one line in the bottom row whose text is not
// measured per locale at the frame level, so it is measured here instead: the caption strip,
// its font and the cell floor come from ui/OVTPathCaptions.h, which the editor's own layout
// model reads too. A proof that re-derives its own numbers would prove nothing.

#include <juce_audio_processors/juce_audio_processors.h>

#include "../../Source/ui/OVTPathCaptions.h"

class PathCaptionLayoutTest : public juce::UnitTest
{
public:
    PathCaptionLayoutTest() : juce::UnitTest ("PathCaptionLayout") {}

    void runTest() override
    {
        // The language is a global the whole UI reads through ovt::tr(); the test walks all six
        // shipped ones and puts the previous choice back, so nothing after it is affected.
        const auto savedLanguage = ovt::currentLanguage();

        const juce::Font floorFont   = ovt::createFont (8.0f, true);   // the shrink floor
        const juce::Font labelSmall  = ovt::fontLabelSmall();          // the preferred size

        beginTest ("one caption per column, and every shipped locale fits one 74 px cell");

        const ovt::Language shipped[] = { ovt::Language::English, ovt::Language::French,
                                          ovt::Language::German,  ovt::Language::Spanish,
                                          ovt::Language::Japanese, ovt::Language::Chinese };

        for (const auto language : shipped)
        {
            ovt::currentLanguage() = language;

            const auto texts = ovt::pathCaptionTexts();
            expectEquals (static_cast<int> (texts.size()), 5,
                          "there is exactly one caption per Effects column");

            // The column a caption has to fit is the fixed Effects cell floor. The width model
            // raises the frame only when the strip asks for more than that, which is exactly
            // what this bound proves never happens: the folded row keeps its width in all six
            // languages because the floor still dominates.
            const int needAtFloor = ovt::pathCaptionStripNeed (floorFont);
            expectLessOrEqual (needAtFloor, ovt::kPathCaptionCellW,
                               "the caption strip fits the fixed Effects cell at the 8 pt floor");

            const int room = ovt::pathCaptionTextRoom();
            for (const auto& text : texts)
            {
                expect (text.isNotEmpty(), "a caption line is never empty");
                expectGreaterThan (
                    static_cast<int> (juce::GlyphArrangement::getStringWidth (floorFont, text)), 0,
                    "the caption paints glyphs, not an empty string");
                expectLessOrEqual (
                    static_cast<int> (std::ceil (juce::GlyphArrangement::getStringWidth (floorFont, text))),
                    room,
                    "every caption line fits the room the cell leaves it at the 8 pt floor");
            }

            // Column 3 names two voices, so it is the widest line BY CONSTRUCTION: if a future
            // edit moved the two-voice line elsewhere, the strip measurement would still hold,
            // but this expectation is what documents which line the width model is sized by.
            expectLessOrEqual (ovt::pathCaptionTextWidth (floorFont),
                               ovt::pathCaptionTextRoom() + ovt::kPathCaptionDotD, // sanity: within the cell
                               "the widest caption line stays inside one cell");
        }

        beginTest ("the preferred size shrinks to fit instead of widening the block");
        for (const auto language : shipped)
        {
            ovt::currentLanguage() = language;

            const juce::Font chosen = ovt::pathCaptionFont();
            const int chosenW = ovt::pathCaptionTextWidth (chosen);

            expectLessOrEqual (chosenW, ovt::pathCaptionTextRoom(),
                               "the chosen caption font fits the cell it is painted in");
            expectGreaterOrEqual (chosen.getHeight(), 8.0f * 0.99f,
                                  "the chosen caption font never goes below the 8 pt floor");
            expectLessOrEqual (chosen.getHeight(), labelSmall.getHeight() + 0.01f,
                               "the chosen caption font never exceeds the preferred label size");
        }

        // Restore the language before leaving, whatever it was.
        ovt::currentLanguage() = savedLanguage;
    }
};

static PathCaptionLayoutTest pathCaptionLayoutTest;
