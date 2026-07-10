// LookAndFeel.cpp
#include "LookAndFeel.h"
#include "OVTFonts.h"
#include "OVTTheme.h"
#include "../PluginEditor.h"
#include <cmath>

namespace ui
{
    AutotuneLookAndFeel::AutotuneLookAndFeel()
    {
        // Setup colors for general UI elements
        setColour (juce::Slider::textBoxTextColourId, ovt::text());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, ovt::accentSoft());
        
        setColour (juce::ComboBox::backgroundColourId, ovt::bgDark());
        setColour (juce::ComboBox::outlineColourId, ovt::bgPanel());
        setColour (juce::ComboBox::textColourId, ovt::text());
        setColour (juce::ComboBox::arrowColourId, ovt::accent());
        
        // Tooltip: transparent background so our drawTooltip rounded rect is the only fill
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TooltipWindow::textColourId, ovt::text());

        // Popup menus (combo dropdowns, hamburger menu, presets menu)
        setColour (juce::PopupMenu::backgroundColourId, ovt::bgDark());
        setColour (juce::PopupMenu::textColourId, ovt::text());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ovt::accentSoft());
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    }

    void AutotuneLookAndFeel::refreshThemeColours()
    {
        setColour (juce::Slider::textBoxTextColourId, ovt::text());
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, ovt::accentSoft());
        setColour (juce::Slider::rotarySliderFillColourId, ovt::accent());
        setColour (juce::Slider::rotarySliderOutlineColourId, ovt::accentSoft());
        setColour (juce::Slider::thumbColourId, juce::Colours::white);
        
        setColour (juce::ComboBox::backgroundColourId, ovt::bgDark());
        setColour (juce::ComboBox::outlineColourId, ovt::bgPanel());
        setColour (juce::ComboBox::textColourId, ovt::text());
        setColour (juce::ComboBox::arrowColourId, ovt::accent());
        
        // Popup menus (combo dropdowns, hamburger menu, presets menu)
        setColour (juce::PopupMenu::backgroundColourId, ovt::bgDark());
        setColour (juce::PopupMenu::textColourId, ovt::text());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ovt::accentSoft());
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

        setColour (juce::Label::textColourId, ovt::text());
    }

    void AutotuneLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                                                juce::Slider& slider)
    {
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
        auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - 4.0f;
        auto centreX = bounds.getCentreX();
        auto centreY = bounds.getCentreY();
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // Draw track background
        g.setColour (ovt::bgPanel().darker(0.5f));
        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.strokePath (backgroundArc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw active track
        if (slider.isEnabled())
        {
            g.setColour (ovt::accent());
            juce::Path valueArc;
            valueArc.addCentredArc (centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.strokePath (valueArc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draw knob center (dark gradient)
        const auto knobLight = ovt::isDark() ? juce::Colour::fromString("#FF303030") : juce::Colour::fromString("#FF505050");
        const auto knobDark  = ovt::isDark() ? juce::Colour::fromString("#FF151515") : juce::Colour::fromString("#FF383838");
        juce::ColourGradient grad (knobLight, centreX, centreY - radius,
                                   knobDark, centreX, centreY + radius, false);
        g.setGradientFill (grad);
        g.fillEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f);

        // Knob shadow/outline
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (rx + 4.0f, ry + 4.0f, rw - 8.0f, rw - 8.0f, 2.0f);

        // Draw pointer (line instead of dot for modern look)
        juce::Path p;
        auto pointerLength = radius - 6.0f;
        auto pointerThickness = 2.5f;
        p.startNewSubPath(0.0f, -radius + 8.0f);
        p.lineTo(0.0f, -radius + 8.0f + pointerLength * 0.4f);
        p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
        g.setColour (slider.isEnabled() ? juce::Colours::white : juce::Colours::grey.withAlpha(0.5f));
        g.strokePath(p, juce::PathStrokeType(pointerThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void AutotuneLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                            int buttonX, int buttonY, int buttonW, int buttonH,
                                            juce::ComboBox& box)
    {
        auto cornerSize = 4.0f;
        juce::Rectangle<int> boxBounds (0, 0, width, height);

        g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle (boxBounds.toFloat(), cornerSize);

        g.setColour (box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (boxBounds.toFloat().reduced (0.5f, 0.5f), cornerSize, 1.0f);

        juce::Path path;
        auto x = buttonX + buttonW * 0.5f;
        auto y = buttonY + buttonH * 0.5f;
        auto w = juce::jmin (buttonW, buttonH) * 0.25f;

        path.addTriangle (x - w, y - w * 0.5f,
                          x + w, y - w * 0.5f,
                          x, y + w * 0.5f);

        g.setColour (box.findColour (juce::ComboBox::arrowColourId).withAlpha (box.isEnabled() ? 1.0f : 0.3f));
        g.fillPath (path);
    }

    void AutotuneLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (1, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
    
    juce::Font AutotuneLookAndFeel::getComboBoxFont (juce::ComboBox& box)
    {
        return ovt::fontComboBox();
    }
    
    juce::Font AutotuneLookAndFeel::getLabelFont (juce::Label& label)
    {
        return ovt::fontComboBox();
    }

    void AutotuneLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        auto bounds = button.getLocalBounds().toFloat();
        bool isOn = button.getToggleState();
        bool isPowerIcon = button.getName() == "PowerButton" || button.getButtonText() == "ON" || button.getButtonText() == "Power";

        if (isPowerIcon)
        {
            // Custom Power Button (Gate / Formant style)
            juce::String text = button.getButtonText();
            bool hasText = text.isNotEmpty() && text != "ON" && text != "Power";

            float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.4f;
            float iconWidth = radius * 2.0f;
            
            float textWidth = 0.0f;
            if (hasText)
            {
                g.setFont(ovt::fontLabel());
                textWidth = g.getCurrentFont().getStringWidthFloat(text) + 8.0f; // 8px spacing
            }

            float totalWidth = iconWidth + textWidth;
            float startX = (bounds.getWidth() - totalWidth) * 0.5f;
            
            juce::Point<float> center(startX + radius, bounds.getHeight() * 0.5f);

            // Colors based on state
            juce::Colour glowColor = juce::Colour(0xFFE8D050); // Yellow/Gold
            juce::Colour offColor = ovt::text().withAlpha(0.3f);
            juce::Colour activeColor = isOn ? glowColor : offColor;

            // Draw Glow if ON
            if (isOn) {
                juce::ColourGradient glowGrad(glowColor.withAlpha(0.4f), center.x, center.y,
                                              glowColor.withAlpha(0.0f), center.x, center.y + radius * 1.5f, true);
                g.setGradientFill(glowGrad);
                g.fillEllipse(center.x - radius * 1.5f, center.y - radius * 1.5f, radius * 3.0f, radius * 3.0f);
            }

            // Draw Power Icon (Circle with gap + line)
            g.setColour(activeColor);

            juce::Path powerArc;
            float gapAngle = juce::MathConstants<float>::pi * 0.25f;
            powerArc.addCentredArc(center.x, center.y, radius, radius, 0.0f, gapAngle, juce::MathConstants<float>::pi * 2.0f - gapAngle, true);
            g.strokePath(powerArc, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::Path powerLine;
            powerLine.startNewSubPath(center.x, center.y - radius * 0.2f);
            powerLine.lineTo(center.x, center.y - radius * 1.2f);
            g.strokePath(powerLine, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Draw Text
            if (hasText)
            {
                g.setColour(isOn ? ovt::text() : ovt::text().withAlpha(0.5f));
                g.drawText(text, startX + iconWidth + 8.0f, 0.0f, textWidth, bounds.getHeight(), juce::Justification::centredLeft, true);
            }
        }
        else
        {
            // Standard Checkbox style (always dark: used in curve editor)
            auto size = juce::jmin(16.0f, bounds.getHeight() * 0.7f);
            auto rect = juce::Rectangle<float>(0.0f, (bounds.getHeight() - size) * 0.5f, size, size);
            
            g.setColour(juce::Colour (0xff191b1e));
            g.fillRoundedRectangle(rect, 3.0f);
            
            g.setColour(juce::Colour (0xff555555));
            g.drawRoundedRectangle(rect, 3.0f, 1.0f);
            
            if (isOn) {
                g.setColour(ovt::accent());
                juce::Path check;
                check.startNewSubPath(rect.getX() + size * 0.2f, rect.getY() + size * 0.5f);
                check.lineTo(rect.getX() + size * 0.4f, rect.getY() + size * 0.7f);
                check.lineTo(rect.getX() + size * 0.8f, rect.getY() + size * 0.3f);
                g.strokePath(check, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            
            g.setColour(button.findColour(juce::ToggleButton::textColourId));
            g.setFont(ovt::fontToggleButton());
            g.drawText(button.getButtonText(), bounds.withTrimmedLeft(size + 6.0f), juce::Justification::centredLeft);
        }
    }

    void AutotuneLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        const auto fg = ovt::text();

        g.setColour (ovt::bgDark());
        g.fillRect (0, 0, width, height);

        g.setColour (ovt::accentSoft());
        g.drawRect (0, 0, width, height, 1);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, ovt::fontTooltip(), fg);

        juce::TextLayout layout;
        layout.createLayout (s, (float) width - 16.0f);
        layout.draw (g, juce::Rectangle<float> (8.0f, 6.0f, (float) width - 16.0f, (float) height - 12.0f));
    }

    juce::Rectangle<int> AutotuneLookAndFeel::getTooltipBounds (const juce::String& tipText,
                                                                juce::Point<int> screenPos,
                                                                juce::Rectangle<int> parentArea)
    {
        const int maxWidth = 360;
        const int minWidth = 120;

        int widestLine = 0;
        juce::StringArray lines;
        lines.addLines (tipText);
        for (const auto& line : lines)
            widestLine = juce::jmax (widestLine, ovt::fontTooltip().getStringWidth (line));

        int width = juce::jlimit (minWidth, maxWidth, widestLine + 16);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (tipText, ovt::fontTooltip(), ovt::text());

        juce::TextLayout layout;
        layout.createLayout (s, (float) width - 16.0f);
        const int height = (int) std::ceil (layout.getHeight() + 12.0f);

        int x = screenPos.x - width / 2;
        int y = screenPos.y - height - 14;

        if (y < parentArea.getY())
            y = screenPos.y + 24;

        x = juce::jlimit (parentArea.getX(), parentArea.getRight() - width, x);
        y = juce::jlimit (parentArea.getY(), parentArea.getBottom() - height, y);

        return { x, y, width, height };
    }

    void AutotuneLookAndFeel::drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g)
    {
        auto barBounds = bar.getLocalBounds().toFloat();

        // Tab bar background matching the plugin background
        g.setColour (ovt::bgDark());
        g.fillRect (barBounds);

        // Subtle bottom line
        g.setColour (ovt::accentSoft().withAlpha (0.3f));
        g.drawHorizontalLine (barBounds.getBottom() - 1.0f, 0.0f, barBounds.getWidth());
    }

    void AutotuneLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                              bool isMouseOver, bool isMouseDown)
    {
        auto tabBounds = button.getLocalBounds().toFloat();
        const float indent = 2.0f;
        const float tabHeight = tabBounds.getHeight();
        const bool isFrontTab = (button.getToggleState());

        if (isFrontTab)
        {
            // Active tab: filled with accent color, rounded top
            g.setColour (ovt::accent());
            auto activeTab = tabBounds.reduced (indent, 0.0f).removeFromTop (tabHeight - 1.0f);
            g.fillRoundedRectangle (activeTab, 4.0f);
        }
        else if (isMouseOver || isMouseDown)
        {
            // Inactive tab hover: subtle highlight
            g.setColour (ovt::accentSoft().withAlpha (0.15f));
            auto hoverTab = tabBounds.reduced (indent, 0.0f).removeFromTop (tabHeight - 1.0f);
            g.fillRoundedRectangle (hoverTab, 4.0f);
        }

        // IMPORTANT: Draw the tab text (delegates to drawTabButtonText)
        drawTabButtonText (button, g, isMouseOver, isMouseDown);
    }

    void AutotuneLookAndFeel::drawTabButtonText (juce::TabBarButton& button, juce::Graphics& g,
                                                  bool /*isMouseOver*/, bool /*isMouseDown*/)
    {
        auto tabBounds = button.getLocalBounds();
        const bool isFrontTab = (button.getToggleState());

        g.setFont (ovt::fontComboBox());
        g.setColour (isFrontTab ? juce::Colours::white : ovt::textDim());
        g.drawText (button.getButtonText(), tabBounds, juce::Justification::centred, false);
    }

    void AutotuneLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        // Force the dark plugin background for all popup menus
        g.setColour (ovt::bgDark());
        g.fillRect (0, 0, width, height);

        // Subtle border
        g.setColour (ovt::bgPanel());
        g.drawRect (0, 0, width, height, 1);
    }

    void AutotuneLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                 float sliderPos, float minSliderPos, float maxSliderPos,
                                                 const juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        // Custom rendering only for the morph slider (name "Morph").
        // All other LinearHorizontal sliders use the default rendering.
        if (style != juce::Slider::LinearHorizontal || slider.getName() != "Morph")
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos,
                                               minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const float trackHeight = 4.0f;
        const float thumbWidth = 12.0f;
        const float trackRadius = trackHeight * 0.5f;

        auto trackTop = (float) y + ((float) height - trackHeight) * 0.5f;
        auto trackLeft = (float) x + thumbWidth * 0.5f;
        auto trackRight = (float) x + (float) width - thumbWidth * 0.5f;
        auto trackWidth = trackRight - trackLeft;

        juce::Rectangle<float> trackBounds (trackLeft, trackTop, trackWidth, trackHeight);

        // Full track background (both filled and unfilled portions)
        g.setColour (slider.findColour (juce::Slider::backgroundColourId)
                         .brighter (0.15f));
        g.fillRoundedRectangle (trackBounds, trackRadius);

        // Filled portion (from start to thumb position)
        const float fillWidth = juce::jmax (0.0f, sliderPos - trackLeft);
        if (fillWidth > 0.0f)
        {
            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.fillRoundedRectangle (juce::Rectangle<float> (trackLeft, trackTop, fillWidth, trackHeight),
                                    trackRadius);
        }

        // Thumb
        auto thumbCentreY = trackTop + trackHeight * 0.5f;
        g.setColour (slider.findColour (juce::Slider::thumbColourId));
        g.fillEllipse (sliderPos - thumbWidth * 0.5f, thumbCentreY - thumbWidth * 0.5f,
                        thumbWidth, thumbWidth);
    }
}
