// OVTVerticalCaption.h
// OpenVoxTuner UI component
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// A knob caption set VERTICALLY along the knob's left edge, using the same typographic recipe as
// the frame title spines (ebs::FrameTitleBand): UPPERCASE, accent-free, tracked capitals at 50 %
// alpha, with the glyph size shrinking towards ebs::kBandMinFontSize instead of ever clipping.
// Sharing bandCaps()/kBandLetterSpacing/kBandTextAlpha is the point: a caption inside a frame and
// the frame's own spine are read as one family, so the row never looks like two design systems.
//
// It is deliberately NOT an ebs::FrameTitleBand: a spine names a whole frame and therefore spends
// height on a tinted strip + an inner trim column, while a caption labels one dial and spends the
// 17 px of width a spine does but no tint at all. Five tinted strips inside the Reverb frame would
// read as five sub-frames.
//
// It derives from juce::Label so it rides every i18n path a label already takes
// (translatableLabels in the editor refreshes it when the interface language changes).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// The spine recipe (bandCaps, band sizes, tracking, alpha) lives in the shared design system.
#include <eiffelbs/FrameTitleBand.h>

namespace ovt
{
    class VerticalCaption : public juce::Label
    {
    public:
        VerticalCaption()
        {
            setInterceptsMouseClicks (false, false);   // a caption is read, not clicked
            setWantsKeyboardFocus (false);
            setJustificationType (juce::Justification::centred);
        }

        /** Width a vertical caption asks for: the frame spine's own measured width, so a caption
            and the spine it sits under are one column of pixels across the whole bottom row. */
        static constexpr int captionWidth() { return ebs::FrameTitleBand::bandWidth(); }

        void paint (juce::Graphics& g) override
        {
            const auto text = getText();

            if (text.isEmpty())
                return;

            auto area = getLocalBounds();

            // Same caption the spines draw: uppercase, accents dropped, safe for every locale.
            const auto caps = ebs::bandCaps (text);
            const auto w = float (area.getWidth());
            const auto h = float (area.getHeight());

            // Shrink the glyphs until the name fits the column height: a long translated word pays
            // in glyph size, never in a wider column or in a clipped caption.
            float size = ebs::kBandFontSize;
            int length = ebs::bandTextLength (caps, size);

            while (length > juce::roundToInt (h - 2.0f * ebs::kBandPad) && size > ebs::kBandMinFontSize)
            {
                size -= 1.0f;
                length = ebs::bandTextLength (caps, size);
            }

            const auto font = ebs::createFont (size, true);
            juce::GlyphArrangement glyphs;

            // Tracked glyphs, one call each: JUCE's addLineOfText applies the font's kerning only,
            // and bandTextLength (the fit check above) sums exactly these advances + gaps.
            float cursor = 0.0f;

            for (int i = 0; i < caps.length(); ++i)
            {
                const auto ch = caps.substring (i, i + 1);
                glyphs.addLineOfText (font, ch, cursor, 0.0f);

                if (i < caps.length() - 1)
                    cursor += juce::GlyphArrangement::getStringWidth (font, ch) + ebs::kBandLetterSpacing;
            }

            const auto bb = glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), false);

            // Rotation by -90 runs the caption bottom-to-top, like a book spine: the same transform
            // ebs::FrameTitleBand uses, so a caption and the spine above it read in one direction.
            const auto ty = h * 0.5f + (bb.getX() + bb.getRight()) * 0.5f;
            const auto tx = w * 0.5f - (bb.getY() + bb.getBottom()) * 0.5f;

            g.setColour (isEnabled() ? ebs::text().withMultipliedAlpha (ebs::kBandTextAlpha)
                                     : ebs::textDim());
            glyphs.draw (g, juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                   .translated (tx, ty));
        }

        void resized() override {}   // the caption reads its bounds at paint time, nothing to cache

        JUCE_LEAK_DETECTOR (VerticalCaption)
    };
}
