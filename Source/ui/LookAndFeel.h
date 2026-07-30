// LookAndFeel.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
    class OVTLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        OVTLookAndFeel();

        /** Refresh all LookAndFeel colours to match the current theme. 
            Call this whenever ovt::currentTheme() changes. */
        void refreshThemeColours();

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                               juce::Slider& slider) override;

        void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox& box) override;

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;
        
        juce::Font getComboBoxFont (juce::ComboBox& box) override;
        juce::Font getLabelFont (juce::Label& label) override;
        juce::Font getPopupMenuFont() override;
        
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, 
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText,
                                              juce::Point<int> screenPos,
                                              juce::Rectangle<int> parentArea) override;

        // Modern tab drawing
        void drawTabbedButtonBarBackground (juce::TabbedButtonBar& bar, juce::Graphics& g) override;
        void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                            bool isMouseOver, bool isMouseDown) override;
        void drawTabButtonText (juce::TabBarButton& button, juce::Graphics& g,
                                bool isMouseOver, bool isMouseDown) override;

        void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;

        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               const juce::Slider::SliderStyle style, juce::Slider& slider) override;
    };
}



