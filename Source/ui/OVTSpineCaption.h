// OVTSpineCaption.h
// OpenVoxTuner UI components
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The frame spines' caption recipe, applied to the places a control carries a caption of its own:
//   - ovt::VerticalCaption     a knob caption set VERTICALLY along the knob's left edge (Reverb's
//                              Room/Hall amounts), where a 17 px column buys back the height a
//                              horizontal caption would eat.
//   - ovt::PowerToggleCaps     a power switch whose name is drawn in that same style HORIZONTALLY,
//                              because a switch's name is the caption of the effect it arms.
//   - ovt::HorizontalCaption   the caption above a knob or a combo (Speed, Amount, Root, Scale,
//                              Key/Scale Detection), the same horizontal caption outside a switch.
//
// Sharing one recipe is the point: uppercase, accent-free, tracked capitals at 50 % alpha, sized
// between ebs::kBandFontSize and ebs::kBandMinFontSize rather than clipped. A caption inside a frame,
// a spine naming that frame and a switch naming its effect then read as one family instead of three
// design systems living in the same row. The vertical caption shrinks per name (a spine does); the
// switch names all take ONE size (9 pt, the floor the longest of them need), because a horizontal row
// of switches sized one by one reads as a ransom note - see PowerToggleCaps.
//
// Neither class is an ebs::FrameTitleBand: a spine names a whole frame and therefore pays for a tinted
// strip and an inner trim column, while a control caption spends none of that on tint - five tinted
// strips inside the Reverb frame would read as five sub-frames around dials that are one instrument.
//
// ovt::VerticalCaption derives from juce::Label so it rides every i18n path a label already takes
// (translatableLabels in the editor refreshes it when the interface language changes).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// The spine recipe (bandCaps, band sizes, tracking, alpha) lives in the shared design system.
#include <eiffelbs/FrameTitleBand.h>

// ebs::PowerToggle owns the glowing power glyph; PowerToggleCaps only changes how its name is drawn.
#include <eiffelbs/PowerToggle.h>

namespace ovt
{
    namespace spine
    {
        /** The caption as it is drawn: uppercase, accents dropped, safe for every locale. */
        inline juce::String capsOf (const juce::String& text) { return ebs::bandCaps (text); }

        /** One glyph's advance. Both the measuring and the drawing paths use this call, so the fit
            check and the pixels on screen cannot disagree. */
        inline float glyphAdvance (const juce::Font& font, const juce::String& glyph)
        {
            return juce::GlyphArrangement::getStringWidth (font, glyph);
        }

        /** Length the tracked caption takes in ONE direction at one glyph size (pixels): every glyph
            advance plus `tracking` between neighbours - exactly the run layout() draws. */
        inline float trackedLength (const juce::String& caps, float size, float tracking)
        {
            const auto font = ebs::createFont (size, true);
            float total = 0.0f;

            for (int i = 0; i < caps.length(); ++i)
            {
                total += glyphAdvance (font, caps.substring (i, i + 1));

                if (i < caps.length() - 1)
                    total += tracking;
            }

            return total;
        }

        /** Glyph size the caption takes to fit `available` px in one direction: the spines' 13 pt
            shrinking towards their 9 pt floor. A long translated name pays in glyph size, never in a
            bigger cell; a caller whose box is still too tight at the floor clips (it never widens). */
        inline float fittedSize (const juce::String& caps, float available, float tracking)
        {
            float size = ebs::kBandFontSize;

            while (size > ebs::kBandMinFontSize && trackedLength (caps, size, tracking) > available)
                size -= 1.0f;

            return size;
        }

        /** The caption laid out as tracked glyphs, baseline at y = 0 and run starting at x = 0. */
        inline juce::GlyphArrangement layout (const juce::String& caps, float size, float tracking)
        {
            juce::GlyphArrangement glyphs;
            const auto font = ebs::createFont (size, true);
            float cursor = 0.0f;

            for (int i = 0; i < caps.length(); ++i)
            {
                const auto glyph = caps.substring (i, i + 1);
                glyphs.addLineOfText (font, glyph, cursor, 0.0f);

                if (i < caps.length() - 1)
                    cursor += glyphAdvance (font, glyph) + tracking;
            }

            return glyphs;
        }

        /** Strength a caption is read at: the spines' 50 % while idle, full strength on a switch that
            is ON (the state cue ebs::PowerToggle already gives its own name), and dimmed when disabled. */
        inline juce::Colour captionColour (bool engaged, bool enabled)
        {
            if (! enabled)
                return ebs::text().withAlpha (0.25f);

            return engaged ? ebs::text()
                           : ebs::text().withMultipliedAlpha (ebs::kBandTextAlpha);
        }

        /** ONE size for every caption of the bottom row: the switch names, the captions above a knob
            or a combo (Speed, Amount, Root, Scale, Key/Scale Detection) and the vertical Room/Hall
            captions - 11 pt nominal, 9.35 px rendered on macOS (the 0.85 platform scale), the same
            size the frames' normal captions use (ovt::fontCaption()). ONE size is the point: a caption
            names a control, and until now the row said it at four sizes (9 pt for the switch names,
            13 pt for the vertical ones, 11 and 13 pt for the knob captions). Measured at this size,
            the longest switch name in any shipped locale (Japanese "フォルマント") needs 61.1 px, so
            the Effects cell is 82 px (glyph 10.8 + gap 8 + the name). */
        inline constexpr float kCaptionFontSize = 11.0f;

        /** Slightly tighter than the spines' tracking: a caption read as a word at eye level in a row
            needs a little less air than an epithet running up a 17 px column. */
        inline constexpr float kCaptionLetterSpacing = 1.0f;

        /** A caption shrunk to the room its own box offers - the frame spine's rule (a long name pays
            in glyph size, never in a bigger cell), but starting at the CAPTION size instead of the
            spine's 13 pt, so a vertical caption and a horizontal one belong to the same family. */
        inline float fittedCaptionSize (const juce::String& caps, float available, float tracking)
        {
            float size = kCaptionFontSize;

            while (size > 8.0f && trackedLength (caps, size, tracking) > available)
                size -= 1.0f;

            return size;
        }

    }

    /** A knob caption set vertically along the knob's left edge, in the frame spines' typography. */
    class VerticalCaption : public juce::Label
    {
    public:
        VerticalCaption()
        {
            setInterceptsMouseClicks (false, false);   // a caption is read, not clicked
            setWantsKeyboardFocus (false);
            setJustificationType (juce::Justification::centred);
        }

        /** Width a vertical caption asks for: the frame spine's own measured width, so a caption and
            the spine it sits under are one column of pixels across the whole bottom row. */
        static constexpr int captionWidth() { return ebs::FrameTitleBand::bandWidth(); }

        void paint (juce::Graphics& g) override
        {
            const auto text = getText();

            if (text.isEmpty())
                return;

            auto area = getLocalBounds();
            const auto w = float (area.getWidth());
            const auto h = float (area.getHeight());

            // Same caption the spines draw, shrunk to the height this column actually offers.
            const auto caps = spine::capsOf (text);
            const auto glyphs = spine::layout (caps,
                                               spine::fittedCaptionSize (caps, h - 2.0f * ebs::kBandPad,
                                                                        spine::kCaptionLetterSpacing),
                                               spine::kCaptionLetterSpacing);

            if (glyphs.getNumGlyphs() == 0)
                return;

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

    /** A caption set HORIZONTALLY, above the knob or combo it names, in the frame spines' typography:
        the same one-size tracked capitals as the power-switch names, at the spines' quiet 50 % - the
        caption is a label, not a title. A caption that names a switch brightens with that switch (see
        setEngagedSource), so "Key/Scale Detection" reads engaged exactly like the switch names do.

        Justification is honoured horizontally (centred by default, left for the detection label) and
        always centred vertically, so the same class serves a caption over a knob, over a combo and
        beside a power glyph. */
    class HorizontalCaption : public juce::Label
    {
    public:
        HorizontalCaption()
        {
            setJustificationType (juce::Justification::centred);
            setWantsKeyboardFocus (false);
        }

        /** Optional state cue, read at paint time so no one has to sync it: a caption whose control
            engages brightens to full text colour while that control is on, like a switch's own name. */
        void setEngagedSource (std::function<bool()> source) { engagedSource = std::move (source); }

        void paint (juce::Graphics& g) override
        {
            const auto caps = spine::capsOf (getText());

            if (caps.isEmpty())
                return;

            const auto area   = getLocalBounds().toFloat();
            const auto glyphs = spine::layout (caps, spine::kCaptionFontSize, spine::kCaptionLetterSpacing);

            if (glyphs.getNumGlyphs() == 0)
                return;

            const auto bb = glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), false);
            const auto just = getJustificationType();

            // Horizontal: honour the Label's justification (the glyph run starts at x = 0). Vertical:
            // the caption strip is always a single line, so centre the ink, not the font box.
            float x = -bb.getX();

            if (just.testFlags (juce::Justification::horizontallyCentred))
                x = (area.getWidth() - bb.getWidth()) * 0.5f - bb.getX();
            else if (just.testFlags (juce::Justification::right))
                x = area.getWidth() - bb.getRight();

            const auto y = area.getHeight() * 0.5f - (bb.getY() + bb.getBottom()) * 0.5f;

            g.setColour (spine::captionColour (engagedSource != nullptr && engagedSource(), isEnabled()));

            // Clipped like the switch names, for the same reason: a caption never spills over the
            // control beside it, whatever a future translation does.
            g.saveState();
            g.reduceClipRegion (getLocalBounds());
            glyphs.draw (g, juce::AffineTransform::translation (x, y));
            g.restoreState();
        }

        void resized() override {}   // the caption reads its bounds at paint time, nothing to cache

        JUCE_LEAK_DETECTOR (HorizontalCaption)

    private:
        std::function<bool()> engagedSource;
    };

    /** An ebs::PowerToggle whose name is drawn in the frame spines' typography instead of the design
        system's mixed-case label. Behaviour, colour ids and the glowing power glyph are
        ebs::PowerToggle's; only the name is restyled.

        ONE glyph size for every switch name, whatever the name and whatever the language: the spines'
        9 pt floor - the size the longest names (FOLLOW LEAD, SATURATION) take anyway once fitted - and
        a slightly tighter tracking than the spines'. Sizing each name to its own cell was incoherent
        in practice: a short "GATE" grew to 13 pt while "SATURATION" sat at 9 pt in the cell next to
        it, so the row read as a ransom note instead of one caption family. The guard for a name that
        overflows anyway is the same clip below; it never changes the size. */
    class PowerToggleCaps : public ebs::PowerToggle
    {
    public:
        /** The gap ebs::PowerToggle keeps between the power glyph and its name. */
        static constexpr float kCaptionGap = 8.0f;

        // The one caption size and the slightly tighter tracking are spine::kCaptionFontSize /
        // spine::kCaptionLetterSpacing, shared with HorizontalCaption: a switch name and the caption
        // above a knob beside it are the same word class, so they must not drift apart.

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const bool engaged = getToggleState();

            const float radius    = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.3f;
            const float iconWidth = radius * 2.0f;

            // The name, as the spines would set it, at the one shared caption size.
            const auto caps         = spine::capsOf (getButtonText());
            const auto captionWidth = caps.isEmpty()
                                        ? 0.0f
                                        : spine::trackedLength (caps, spine::kCaptionFontSize,
                                                                spine::kCaptionLetterSpacing);

            const float totalWidth = iconWidth + (caps.isEmpty() ? 0.0f : kCaptionGap + captionWidth);
            const float startX     = (bounds.getWidth() - totalWidth) * 0.5f;
            const float captionX   = startX + iconWidth + kCaptionGap;

            // Glyph and glow are copied from ebs::PowerToggle::paint on purpose: the design system owns
            // that widget (and the host must not fork it), but it paints name and glyph in one block, so
            // restyling the name locally means repeating the glyph. Both blocks are kept character-
            // identical to the EBS file; when eiffelbs-ui grows a caption style this class collapses into
            // a flag on the shared widget.
            juce::Point<float> center (startX + radius, bounds.getHeight() * 0.5f);

            juce::Colour glowColour   = ebs::widgetColour (*this, ebs::PowerToggle::glowColourId,
                                                           juce::Colour (0xFFE8D050));
            juce::Colour activeColour = engaged ? glowColour : ebs::text().withAlpha (0.3f);

            if (! isEnabled())
            {
                activeColour = activeColour.withAlpha (0.25f);
                glowColour   = glowColour.withAlpha (0.0f);
            }

            if (engaged && isEnabled())
            {
                juce::ColourGradient glowGrad (glowColour.withAlpha (0.4f),
                                               center.x, center.y,
                                               glowColour.withAlpha (0.0f),
                                               center.x, center.y + radius * 1.5f, true);
                g.setGradientFill (glowGrad);
                g.fillEllipse (center.x - radius * 1.5f, center.y - radius * 1.5f,
                               radius * 3.0f, radius * 3.0f);
            }

            g.setColour (activeColour);

            juce::Path powerArc;
            const float gapAngle = juce::MathConstants<float>::pi * 0.25f;
            powerArc.addCentredArc (center.x, center.y, radius, radius, 0.0f,
                                    gapAngle, juce::MathConstants<float>::pi * 2.0f - gapAngle, true);
            g.strokePath (powerArc, juce::PathStrokeType (2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));

            juce::Path powerLine;
            powerLine.startNewSubPath (center.x, center.y - radius * 0.2f);
            powerLine.lineTo (center.x, center.y - radius * 1.2f);
            g.strokePath (powerLine, juce::PathStrokeType (2.0f,
                              juce::PathStrokeType::curved,
                              juce::PathStrokeType::rounded));

            if (caps.isEmpty())
                return;

            auto glyphs = spine::layout (caps, spine::kCaptionFontSize, spine::kCaptionLetterSpacing);

            if (glyphs.getNumGlyphs() == 0)
                return;

            const auto bb = glyphs.getBoundingBox (0, glyphs.getNumGlyphs(), false);
            const auto ty = bounds.getHeight() * 0.5f - (bb.getY() + bb.getBottom()) * 0.5f;

            g.setColour (spine::captionColour (engaged, isEnabled()));

            // A name too long for the room it is given is clipped at the cell edge, never drawn over the
            // glyph: the switch keeps a clean power icon even in the widest translation, and no name
            // changes size to buy room.
            g.saveState();
            g.reduceClipRegion (juce::Rectangle<int> (juce::roundToInt (captionX), 0,
                                                      juce::jmax (0, juce::roundToInt (bounds.getRight() - captionX)),
                                                      getHeight()));
            glyphs.draw (g, juce::AffineTransform::translation (captionX, ty));
            g.restoreState();
        }

        JUCE_LEAK_DETECTOR (PowerToggleCaps)
    };
}
