// OVTPathCaptions.h
// OpenVoxTuner UI module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>

#include "OVTLanguages.h"
#include "OVTFonts.h"

namespace ovt
{
// One caption strip ends every Effects column and every Reverb room: the voice that column
// or room acts on, printed under it, beside the activity dot that says whether that voice is
// passing. Three numbers describe it, and they are shared by the editor's layout model and by
// its offline test, so the proof and the painted pixels cannot drift apart.
//
// The wording is NOT a new string: the voice words are the SHIPPED translations of the voice
// names themselves (the same ones the room headers carry, `kLabelReverbBtn` for the lead and
// `kLegendOutput` for the output), so no locale can end up with two names for one voice. Only
// the separator between the two voices of a mixed column is new, and it is punctuation.
static constexpr int kPathCaptionHeight = 16;   // one line: 8 pt text plus its air
static constexpr int kPathCaptionDotD   = 9;    // the activity dot beside the name
static constexpr int kPathCaptionGap    = 4;    // dot -> text
static constexpr int kPathCaptionAir    = 2;    // keeps the last glyph off the cell edge
static constexpr int kPathCaptionCellW  = 74;   // the narrowest Effects cell a caption must fit

/** The caption lines, in column order. Column 3 is the one Effects column holding two voices -
 *  a lead-side processor (the De-Esser) under the master ceiling - so it names both instead of
 *  picking one and quietly lying about the other. */
inline std::array<juce::String, 5> pathCaptionTexts()
{
    const juce::String lead   = tr (Keys::kLabelReverbBtn);
    const juce::String output = tr (Keys::kLegendOutput);
    const juce::String both   = lead + juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 ")) + output;

    return { lead, lead, both, output, lead };
}

/** Width of the WIDEST caption line at `font`, in pixels. */
inline int pathCaptionTextWidth (const juce::Font& font)
{
    int widest = 0;
    for (const auto& text : pathCaptionTexts())
        widest = juce::jmax (widest, static_cast<int> (std::ceil (
                     juce::GlyphArrangement::getStringWidth (font, text))));
    return widest;
}

/** Width the whole strip asks for at `font`: the dot, its gap, the text, 2 px of air. */
inline int pathCaptionStripNeed (const juce::Font& font)
{
    return kPathCaptionDotD + kPathCaptionGap + pathCaptionTextWidth (font) + kPathCaptionAir;
}

/** Room a caption line has left for its glyphs inside one cell. */
inline int pathCaptionTextRoom (int cellW = kPathCaptionCellW)
{
    return cellW - kPathCaptionDotD - kPathCaptionGap - kPathCaptionAir;
}

/** The font a caption is painted in: the largest size from ovt::fontLabelSmall() down to the
 *  8 pt floor whose widest line still fits the room the cell leaves it. A longer translation
 *  shrinks its own glyphs rather than widening the block - the frame spine's rule, applied to
 *  the one line that has to fit a fixed 74 px cell in six languages at once. */
inline juce::Font pathCaptionFont (int cellW = kPathCaptionCellW)
{
    const int room = juce::jmax (1, pathCaptionTextRoom (cellW));

    for (int size = 11; size >= 8; --size)
        if (pathCaptionTextWidth (createFont (static_cast<float> (size), true)) <= room)
            return createFont (static_cast<float> (size), true);

    return createFont (8.0f, true);
}

} // namespace ovt
