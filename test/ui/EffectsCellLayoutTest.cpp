#pragma once
// EffectsCellLayoutTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// An Effects cell is 74 px wide and its power toggle draws a 16 px tick well, a 6 px gap and
// centred-left text (ebs::LookAndFeel::drawToggleButton) - so the LABEL has 74 - 18 - 2 = 54 px,
// and a longer translated name clips. The activity dot lives in its own 12 px strip under the
// column instead of beside the toggle; this test measures whether that was a SPACE decision or
// merely a choice, in every shipped language, and it is the regression guard for the labels:
// a future rename that no longer fits the cell fails here instead of clipping on screen.

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

#include "../../Source/ui/OVTLanguages.h"

class EffectsCellLayoutTest : public juce::UnitTest
{
public:
    EffectsCellLayoutTest() : juce::UnitTest ("EffectsCellLayout") {}

    void runTest() override
    {
        const auto savedLanguage = ovt::currentLanguage();

        // The cell floor and the dot the strip carries, kept in step with PluginEditor.h by hand:
        // this test documents a LAYOUT DECISION, and the day the floor moves it must be revisited.
        constexpr int cellW      = 74;
        constexpr int tickWellW  = 18;   // 16 px tick well + 6 px gap, minus the shared 4 px
        constexpr int edgeAir    = 2;
        constexpr int dotW       = 9;
        constexpr int dotGap     = 4;

        // Every power toggle in the Effects block, in the order they read on screen.
        const char* const toggleKeys[] = {
            ovt::Keys::kLabelNoiseGate, ovt::Keys::kLabelUpwardComp,
            ovt::Keys::kLabelDouble,    ovt::Keys::kLabelFormantBtn,
            ovt::Keys::kLabelLimiterBtn, ovt::Keys::kLabelDeesser,
            ovt::Keys::kLabelSaturation, ovt::Keys::kLabelDelay
        };
        constexpr int toggleCount = 8;

        const ovt::Language shipped[] = { ovt::Language::English, ovt::Language::French,
                                          ovt::Language::German,  ovt::Language::Spanish,
                                          ovt::Language::Japanese, ovt::Language::Chinese };

        beginTest ("an Effects cell holds its longest toggle label, in every shipped language");

        for (const auto language : shipped)
        {
            ovt::currentLanguage() = language;

            int widest = 0;
            juce::String widestText;
            for (int i = 0; i < toggleCount; ++i)
            {
                const auto text = ovt::tr (toggleKeys[i]);
                const int w = static_cast<int> (std::ceil (
                    juce::GlyphArrangement::getStringWidth (ebs::fontToggleButton(), text)));
                if (w > widest) { widest = w; widestText = text; }
            }

            const int labelRoom = cellW - tickWellW - edgeAir;
            const int labelNeed = widest;
            const int withDot   = tickWellW + widest + dotGap + dotW + edgeAir;

            std::cout << "       effects cell: " << ovt::languageDisplayName (language)
                      << " longest toggle \"" << widestText << "\" " << widest
                      << " px, label room " << labelRoom
                      << " px, with a dot beside it " << withDot << " px\n";

            // Japanese and Chinese overflow the label room TODAY, without any dot: the de-esser and
            // saturation names are 61 and 56 px where the cell leaves 54. The human ACCEPTED this
            // (2026-10-19): both toggles carry a tooltip that names and explains the control
            // (`tooltip.deesser_enable`, `tooltip.saturation_enable`, in all six locales), so the
            // last glyphs may clip while the tooltip carries the meaning. The overflow is still
            // measured exactly, so this test fails if it GROWS - a rename that eats another
            // millimetre of the label is a decision someone has to make on purpose.
            const bool knownOverflow = (language == ovt::Language::Japanese
                                        || language == ovt::Language::Chinese);
            if (knownOverflow)
            {
                const int expected = (language == ovt::Language::Japanese) ? 7 : 2;
                expectEquals (labelNeed - labelRoom, expected,
                              "the accepted label overflow stays exactly as wide as documented "
                              "(the tooltip carries the meaning; a wider overflow must be re-decided)");
            }
            else
            {
                expectLessOrEqual (labelNeed, labelRoom,
                                   "the longest toggle label fits the 74 px cell");
            }

            expectGreaterThan (withDot, cellW,
                               "a dot beside the toggle would not fit the cell, which is why the "
                               "activity dot has its own strip under the column");
        }

        beginTest ("the Delay column's two small labels fit a half-column cell, in every language");

        // The Delay column puts Division and Feedback side by side on one line, so each label has a
        // HALF column (74 px minus the gap between the two cells). "Division" at the shared 11 pt is
        // far wider than that, so the labels shrink their own glyphs - the frame spine's rule - down
        // to an 8 pt floor, the size the output meter's dB line already uses.
        constexpr int halfCellW = 36;   // (74 - 2 px gap) / 2, the widest a half cell can be

        for (const auto language : shipped)
        {
            ovt::currentLanguage() = language;

            const juce::String texts[] = { ovt::tr (ovt::Keys::kLabelDelayDivision),
                                           ovt::tr (ovt::Keys::kLabelDelayFeedback) };

            const auto widestAt = [&texts] (int size)
            {
                const juce::Font font = ovt::createFont (static_cast<float> (size), true);
                int widest = 0;
                for (const auto& text : texts)
                    widest = juce::jmax (widest, static_cast<int> (std::ceil (
                                 juce::GlyphArrangement::getStringWidth (font, text))));
                return widest;
            };

            const int widestAtFloor = widestAt (8);
            int chosen = 8;
            for (int size = 11; size >= 8; --size)
                if (widestAt (size) <= halfCellW) { chosen = size; break; }

            std::cout << "       delay small labels: " << ovt::languageDisplayName (language)
                      << " widest at 11 pt " << widestAt (11)
                      << " px, at 8 pt " << widestAtFloor
                      << " px, chosen size " << chosen << " pt\n";

            // The floor is the promise: if even 8 pt overflowed, the labels would clip.
            expectLessOrEqual (widestAtFloor, halfCellW,
                               "both Delay labels fit a half-column cell at the 8 pt floor");
        }

        ovt::currentLanguage() = savedLanguage;
    }
};

static EffectsCellLayoutTest effectsCellLayoutTest;
