// CorrectionModeSwitch.h
// OpenVoxTuner UI components
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The three-position Modern / Transparent / Hard switch that sits below the
// Speed and Amount knobs. Extracted verbatim from PluginEditor.cpp (step 1 of
// the source split): the class depends on nothing but JUCE, eiffelbs and the
// i18n tables, so it owns its translation unit.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <eiffelbs/eiffelbs.h>

#include <array>
#include <cmath>
#include <functional>

#include "ui/OVTLanguages.h"

// === Modern / Transparent / Hard graphic switch (studio-console style) ===
// Three-position horizontal switch placed below the Speed/Amount knobs:
//   Modern / Transparent write the `correction_mode` parameter (false / true),
//   Hard writes `hard_tune` on and `correction_mode` off.
// The two parameters stay independent in the APVTS, so host automation on either
// one keeps working; the switch paints the COMPOSITE state (hard wins over
// transparent). Backing buttons keep the existing wiring: their ButtonAttachments
// write the parameters, and the editor owns the undo transaction because onSelect
// is called ONCE per click (a mode change is a single Ctrl+Z). A light Timer keeps
// the visual in sync when a parameter changes from elsewhere.
class CorrectionModeSwitch : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
{
public:
    static constexpr int kNumSegments = 3;

    CorrectionModeSwitch (juce::Button& modeBackingButton,
                          juce::Button& hardBackingButton)
        : modeButton (modeBackingButton)
        , hardButton (hardBackingButton)
    {
        setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
        setWantsKeyboardFocus (false);
        animPos = static_cast<float> (activeSegment());
        targetPos = animPos;
        startTimerHz (60); // 60 Hz for smooth animation
    }

    ~CorrectionModeSwitch() override { stopTimer(); }

    /** Called once per click with the requested composite state. The editor
        wraps the two parameter writes in one undo transaction. */
    std::function<void (bool transparent, bool hard)> onSelect;

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        const float radius = area.getHeight() * 0.5f;

        // Track background (dark, subtle border).
        g.setColour (ebs::bgPanel());
        g.fillRoundedRectangle (area, radius);
        g.setColour (ebs::text().withAlpha (0.18f));
        g.drawRoundedRectangle (area, radius, 1.0f);

        const auto segs = segments();
        const int active = activeSegment();

        // Sliding accent pill - animated position AND width, because the three
        // segments are measured rather than equal thirds (a long translated
        // label such as the Japanese "トランスペアレント" must not clip).
        const int from = juce::jlimit (0, kNumSegments - 1, (int) std::floor (animPos));
        const int to   = juce::jlimit (0, kNumSegments - 1, (int) std::ceil  (animPos));
        const float t  = juce::jlimit (0.0f, 1.0f, animPos - (float) from);
        const float cx = segs[(size_t) from].getCentreX()
                       + (segs[(size_t) to].getCentreX() - segs[(size_t) from].getCentreX()) * t;
        const float w  = segs[(size_t) from].getWidth()
                       + (segs[(size_t) to].getWidth() - segs[(size_t) from].getWidth()) * t;
        auto pillRect = juce::Rectangle<float> (cx - (w - 6.0f) * 0.5f,
                                                area.getY() + 2.0f,
                                                juce::jmax (12.0f, w - 6.0f),
                                                area.getHeight() - 4.0f);
        g.setColour (ebs::accent());
        g.fillRoundedRectangle (pillRect, pillRect.getHeight() * 0.5f);

        // Labels: active = white, inactive = dimmed.
        const auto font = segmentFont (area);
        g.setFont (font);
        const juce::String labels[kNumSegments] = { ovt::tr (ovt::Keys::kLabelModernBtn),
                                                    ovt::tr (ovt::Keys::kLabelTransparentBtn),
                                                    ovt::tr (ovt::Keys::kLabelHardTune) };
        for (int i = 0; i < kNumSegments; ++i)
        {
            g.setColour (i == active ? juce::Colours::white : ebs::text().withAlpha (0.55f));
            g.drawText (labels[i], segs[(size_t) i], juce::Justification::centred);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto segs = segments();
        int clicked = activeSegment();
        for (int i = 0; i < kNumSegments; ++i)
            if (segs[(size_t) i].contains (e.position))
            {
                clicked = i;
                break;
            }

        if (clicked == activeSegment())
            return; // already there: no parameter write, no undo entry

        const bool wantTransparent = (clicked == 1);
        const bool wantHard        = (clicked == 2);

        if (onSelect != nullptr)
        {
            onSelect (wantTransparent, wantHard);
        }
        else
        {
            // Defensive fallback for a future reuse without the editor callback.
            modeButton.setToggleState (wantTransparent, juce::sendNotification);
            hardButton.setToggleState (wantHard, juce::sendNotification);
        }
        targetPos = (float) clicked;
    }

    bool hitTest (int, int) override { return true; }

private:
    /** Composite state: Hard wins over Transparent, which wins over Modern. */
    int activeSegment() const noexcept
    {
        if (hardButton.getToggleState())
            return 2;
        return modeButton.getToggleState() ? 1 : 0;
    }

    juce::Font segmentFont (juce::Rectangle<float> area) const
    {
        // 0.34 x the track height rather than the old 0.40: three labels share the
        // strip now, and the widest one has to fit its segment in all six locales.
        return juce::Font (juce::jmax (8.0f, area.getHeight() * 0.34f), juce::Font::bold);
    }

    std::array<juce::Rectangle<float>, kNumSegments> segments() const
    {
        auto area = getLocalBounds().toFloat().reduced (1.0f);
        const auto font = segmentFont (area);
        const juce::String labels[kNumSegments] = { ovt::tr (ovt::Keys::kLabelModernBtn),
                                                    ovt::tr (ovt::Keys::kLabelTransparentBtn),
                                                    ovt::tr (ovt::Keys::kLabelHardTune) };

        // Natural width (text + air), then scaled to the strip: the long segment
        // takes the room it needs instead of an equal third that would clip it.
        std::array<float, kNumSegments> weights {};
        float total = 0.0f;
        for (int i = 0; i < kNumSegments; ++i)
        {
            weights[(size_t) i] = juce::jmax (24.0f,
                juce::GlyphArrangement::getStringWidth (font, labels[i]) + 12.0f);
            total += weights[(size_t) i];
        }

        std::array<juce::Rectangle<float>, kNumSegments> out {};
        float x = area.getX();
        for (int i = 0; i < kNumSegments; ++i)
        {
            const float w = area.getWidth() * weights[(size_t) i] / total;
            out[(size_t) i] = { x, area.getY(), w, area.getHeight() };
            x += w;
        }
        return out;
    }

    void timerCallback() override
    {
        // Sync with external parameter changes (preset load, host automation).
        const float target = static_cast<float> (activeSegment());
        if (std::abs (target - targetPos) > 0.001f)
            targetPos = target;

        // Animate the pill toward its target segment.
        constexpr float kSmoothing = 0.18f; // ~8-10 frames at 60Hz = smooth but snappy
        if (std::abs (animPos - targetPos) > 0.001f)
        {
            animPos += (targetPos - animPos) * kSmoothing;
            repaint();
        }
    }

    juce::Button& modeButton;
    juce::Button& hardButton;
    float animPos = 0.0f;    // current animated segment index
    float targetPos = 0.0f;  // target segment index
};
