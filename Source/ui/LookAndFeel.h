// LookAndFeel.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ui
{
    class AutotuneLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        AutotuneLookAndFeel();

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                               juce::Slider& slider) override;

        void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH,
                           juce::ComboBox& box) override;

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;
        
        juce::Font getComboBoxFont (juce::ComboBox& box) override;
        juce::Font getLabelFont (juce::Label& label) override;
        
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, 
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override;
        juce::Rectangle<int> getTooltipBounds (const juce::String& tipText,
                                              juce::Point<int> screenPos,
                                              juce::Rectangle<int> parentArea) override;
    };
}
