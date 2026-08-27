#pragma once
// LookAndFeelToggleTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


#include <juce_audio_processors/juce_audio_processors.h>
#include <eiffelbs/eiffelbs.h>

class LookAndFeelToggleTest : public juce::UnitTest
{
public:
    LookAndFeelToggleTest() : juce::UnitTest ("LookAndFeelToggle") {}

    void runTest() override
    {
        beginTest ("toggled TextButton honours instance buttonOnColourId "
                   "(Piano Roll contrast regression)");

        // Chip-style setup mirroring OpenVoxTuner's Piano Roll button:
        // instance colours are set for rest AND active states, because the
        // button floats on the theme-invariant dark canvas island.
        ebs::LookAndFeel laf;
        juce::TextButton b { "Piano Roll" };
        b.setLookAndFeel (&laf);
        b.setSize (140, 24);
        b.setColour (juce::TextButton::buttonColourId,   ebs::accentSoft());
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha (0.92f));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4caf50));

        auto countGreen = [] (const juce::Image& img)
        {
            int n = 0;
            const auto rd = juce::Image::BitmapData (img, juce::Image::BitmapData::readOnly);
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                {
                    auto p = rd.getPixelColour (x, y);
                    const int r = p.getRed(), g = p.getGreen(), bl = p.getBlue();
                    if (g > r + 25 && g > bl + 18 && g > 90)
                        ++n;
                }
            return n;
        };

        // Rest state: soft accent-tinted body, NO green pixels.
        b.setToggleState (false, juce::dontSendNotification);
        auto restImg = b.createComponentSnapshot (b.getLocalBounds(), true);
        expectGreaterThan ((int) restImg.getWidth(), 0, "rest snapshot rendered");
        const int greenRest = countGreen (restImg);
        expectEquals (greenRest, 0,
                      "rest state must not contain the active-state green");

        // Toggled state: solid green body (contrast regression guarantee).
        b.setToggleState (true, juce::dontSendNotification);
        auto onImg = b.createComponentSnapshot (b.getLocalBounds(), true);
        const int greenOn = countGreen (onImg);
        expectGreaterThan (greenOn, 400,
                      "toggled state must be unmistakably green");

        std::cout << "       toggle-green px: rest=" << greenRest
                  << " on=" << greenOn << "\n";

        b.setLookAndFeel (nullptr);
    }
};

static LookAndFeelToggleTest lookAndFeelToggleTest;
