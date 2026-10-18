// OVTHarmonyLayout.h
// OpenVoxTuner UI module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The Harmony frame is the band's only GROWABLE block, and it carries two zones with opposite
// needs: the main zone (always visible: enable, type, use-voice, voices, tone, two toggles) wants
// to be as NARROW as its widest control, and the expanded zone (four dials) wants all the spare
// width the band can hand it, because two dials with air between them read as two controls and two
// dials touching read as one.
//
// Both widths are POLICY, so they live here and the editor's layout and the offline layout test
// read the same numbers - a zone sized by a constant the test cannot see is a zone that drifts.
//
// The main zone's widest control is the harmony TYPE combo, whose longest entry must never be
// truncated in any of the six shipped languages: the combo is measured in the current language,
// never guessed from an English sample.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <eiffelbs/eiffelbs.h>

#include "OVTLanguages.h"
#include "OVTFonts.h"   // fontLabelSmall(): the use-voice caption's font, measured here

namespace ovt
{
// The arrow the shared LookAndFeel draws inside a ComboBox plus the gap between the text and it:
// JUCE reserves that area out of the box's width, so the TEXT budget is the remainder. Measured
// against ebs::LookAndFeel::drawComboBox, which draws its triangle inside the button area JUCE
// hands it.
static constexpr int kHarmonyComboArrowW = 24;

// Floor for the main zone. Below this the enable button and the two power toggles would collide
// even if every combo entry were short; the two rows that really set the width - the type combo
// and the use-voice row - are MEASURED below, so this floor only guards the fixed rows.
static constexpr int kHarmonyMainZoneFloor = 140;

// The use-voice row: its caption in the current language, the gap after it, the switch, the tone
// swatch and the air that keeps the swatch off the switch. The row's three parts are placed left,
// left and right inside one strip, so the strip is exactly their sum.
static constexpr int kHarmonyPillW   = 44;
static constexpr int kHarmonySwatchW = 34;
static constexpr int kHarmonyRowAir  = 6;

// The expanded zone: two knob columns, each holding a FULL-SIZE 54 px dial with air around it.
// kHarmonyKnobAir is the clearance on EACH side of a dial and kHarmonyKnobGap the one BETWEEN the
// two columns, so a column must be 54 + 2 x 12 = 78 px wide and the zone 2 x 78 + 16 = 172 px.
// That is the width the human asked for when the dials read as glued together: 12 + 16 + 12 = 40 px
// of clearance between the two knobs.
static constexpr int kHarmonyKnobAir  = 12;
static constexpr int kHarmonyKnobGap  = 16;
static constexpr int kHarmonyKnobColW = 54 + 2 * kHarmonyKnobAir;
static constexpr int kHarmonyKnobZoneW = 2 * kHarmonyKnobColW + kHarmonyKnobGap;

// The smallest knob zone the frame may be squeezed to before the band row scrolls instead: two
// 40 px dials with 4 px of clearance each side and 8 px between them. Below the preferred zone
// the dials shrink, which is the trade a narrow window forces - never an overlap.
static constexpr int kHarmonyKnobZoneFloorW = 2 * (40 + 2 * 4) + 8;

// The 22 shipped harmony types, in parameter order (harmony_type, choice 0..21). ONE list: the
// combo is populated from these keys and the layout test measures the same strings.
inline const char* const* harmonyTypeKeys()
{
    static const char* const keys[] = {
        Keys::kHarmonyNone, Keys::kHarmony3rdBelow,
        Keys::kHarmony3rdAbove, Keys::kHarmony3rdBelowAbove,
        Keys::kHarmony4thBelow, Keys::kHarmony4thAbove,
        Keys::kHarmony4thBelowAbove, Keys::kHarmony5thBelow,
        Keys::kHarmony5thAbove, Keys::kHarmony5thBelowAbove,
        Keys::kHarmony3rdBelow5thAbove, Keys::kHarmony5thBelow3rdAbove,
        Keys::kHarmonyOctaveBelow, Keys::kHarmonyOctaveAbove,
        Keys::kHarmonyOctaveBelowAbove, Keys::kHarmonyVocalStack3,
        Keys::kHarmonyVocalStack4, Keys::kHarmonyPowerChord,
        Keys::kHarmonyParallel3rd, Keys::kHarmonyDrone,
        Keys::kHarmonyUnison2, Keys::kHarmonyUnisonOctaves4
    };
    return keys;
}

static constexpr int kHarmonyTypeCount = 22;

/** The type entries in the CURRENT language. */
inline juce::StringArray harmonyTypeItems()
{
    juce::StringArray items;
    const char* const* keys = harmonyTypeKeys();
    for (int i = 0; i < kHarmonyTypeCount; ++i)
        items.add (tr (keys[i]));
    return items;
}

/** Width of the WIDEST type entry in the current language, at the combo font. */
inline int harmonyTypeTextWidth()
{
    int widest = 0;
    for (const auto& item : harmonyTypeItems())
        widest = juce::jmax (widest, static_cast<int> (std::ceil (
                     juce::GlyphArrangement::getStringWidth (ebs::fontComboBox(), item))));
    return widest;
}

/** What the type combo needs: its longest entry plus the arrow area. */
inline int harmonyTypeComboNeed()
{
    return harmonyTypeTextWidth() + kHarmonyComboArrowW;
}

/** What the use-voice row needs in the current language: caption + gap + switch + swatch + air. */
inline int harmonyUseVoiceRowNeed()
{
    const int caption = static_cast<int> (std::ceil (
        juce::GlyphArrangement::getStringWidth (fontLabelSmall(), tr (Keys::kLabelUseVoice))));
    return caption + 4 + kHarmonyPillW + kHarmonySwatchW + kHarmonyRowAir;
}

/** The main zone's width: exactly what its widest row asks for, never less than the floor. */
inline int harmonyMainZoneW()
{
    return juce::jmax (kHarmonyMainZoneFloor,
                       juce::jmax (harmonyTypeComboNeed(), harmonyUseVoiceRowNeed()));
}

} // namespace ovt
