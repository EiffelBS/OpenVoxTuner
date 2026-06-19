// LookAndFeel.cpp
#include "LookAndFeel.h"
#include "../PluginEditor.h"
#include <cmath>

namespace ui
{
    AutotuneLookAndFeel::AutotuneLookAndFeel()
    {
        // Setup colors for general UI elements
        setColour (juce::Slider::textBoxTextColourId, OpenVoxTunerAudioProcessorEditor::kText);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, OpenVoxTunerAudioProcessorEditor::kAccentSoft);
        
        setColour (juce::ComboBox::backgroundColourId, OpenVoxTunerAudioProcessorEditor::kBgPanel);
        setColour (juce::ComboBox::outlineColourId, OpenVoxTunerAudioProcessorEditor::kBgPanel.brighter(0.1f));
        setColour (juce::ComboBox::textColourId, OpenVoxTunerAudioProcessorEditor::kText);
        setColour (juce::ComboBox::arrowColourId, OpenVoxTunerAudioProcessorEditor::kAccent);
        
        setColour (juce::PopupMenu::backgroundColourId, OpenVoxTunerAudioProcessorEditor::kBgPanel);
        setColour (juce::PopupMenu::textColourId, OpenVoxTunerAudioProcessorEditor::kText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, OpenVoxTunerAudioProcessorEditor::kAccentSoft);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
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
        g.setColour (OpenVoxTunerAudioProcessorEditor::kBgPanel.darker(0.5f));
        juce::Path backgroundArc;
        backgroundArc.addCentredArc (centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.strokePath (backgroundArc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Draw active track
        if (slider.isEnabled())
        {
            g.setColour (OpenVoxTunerAudioProcessorEditor::kAccent);
            juce::Path valueArc;
            valueArc.addCentredArc (centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.strokePath (valueArc, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draw knob center (dark gradient)
        juce::ColourGradient grad (juce::Colour::fromString("#FF303030"), centreX, centreY - radius,
                                   juce::Colour::fromString("#FF151515"), centreX, centreY + radius, false);
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
        return juce::Font (14.0f, juce::Font::plain);
    }
    
    juce::Font AutotuneLookAndFeel::getLabelFont (juce::Label& label)
    {
        return juce::Font (14.0f, juce::Font::plain);
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
                g.setFont(juce::Font(13.0f, juce::Font::bold));
                textWidth = g.getCurrentFont().getStringWidthFloat(text) + 8.0f; // 8px spacing
            }

            float totalWidth = iconWidth + textWidth;
            float startX = (bounds.getWidth() - totalWidth) * 0.5f;
            
            juce::Point<float> center(startX + radius, bounds.getHeight() * 0.5f);

            // Colors based on state
            juce::Colour glowColor = juce::Colour(0xFFE8D050); // Yellow/Gold
            juce::Colour offColor = OpenVoxTunerAudioProcessorEditor::kText.withAlpha(0.3f);
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
                g.setColour(isOn ? OpenVoxTunerAudioProcessorEditor::kText : OpenVoxTunerAudioProcessorEditor::kText.withAlpha(0.5f));
                g.drawText(text, startX + iconWidth + 8.0f, 0.0f, textWidth, bounds.getHeight(), juce::Justification::centredLeft, true);
            }
        }
        else
        {
            // Standard Checkbox style
            auto size = juce::jmin(16.0f, bounds.getHeight() * 0.7f);
            auto rect = juce::Rectangle<float>(0.0f, (bounds.getHeight() - size) * 0.5f, size, size);
            
            g.setColour(OpenVoxTunerAudioProcessorEditor::kBgPanel.darker(0.2f));
            g.fillRoundedRectangle(rect, 3.0f);
            
            g.setColour(OpenVoxTunerAudioProcessorEditor::kBgPanel.brighter(0.2f));
            g.drawRoundedRectangle(rect, 3.0f, 1.0f);
            
            if (isOn) {
                g.setColour(OpenVoxTunerAudioProcessorEditor::kAccent);
                juce::Path check;
                check.startNewSubPath(rect.getX() + size * 0.2f, rect.getY() + size * 0.5f);
                check.lineTo(rect.getX() + size * 0.4f, rect.getY() + size * 0.7f);
                check.lineTo(rect.getX() + size * 0.8f, rect.getY() + size * 0.3f);
                g.strokePath(check, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            
            g.setColour(OpenVoxTunerAudioProcessorEditor::kText);
            g.setFont(14.0f);
            g.drawText(button.getButtonText(), bounds.withTrimmedLeft(size + 6.0f), juce::Justification::centredLeft);
        }
    }

    void AutotuneLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        const auto bg = OpenVoxTunerAudioProcessorEditor::kBgPanel.withAlpha (0.95f);
        const auto outline = OpenVoxTunerAudioProcessorEditor::kAccentSoft.withAlpha (0.8f);
        const auto fg = OpenVoxTunerAudioProcessorEditor::kText;

        g.setColour (bg);
        g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), 6.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f), 6.0f, 1.0f);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, juce::Font (13.0f), fg);

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
            widestLine = juce::jmax (widestLine, juce::Font (13.0f).getStringWidth (line));

        int width = juce::jlimit (minWidth, maxWidth, widestLine + 16);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (tipText, juce::Font (13.0f), OpenVoxTunerAudioProcessorEditor::kText);

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
}
