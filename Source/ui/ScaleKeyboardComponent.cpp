// ScaleKeyboardComponent.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


#include "ScaleKeyboardComponent.h"

namespace ui
{
    // === PianoKeyButton ===

    PianoKeyButton::PianoKeyButton()
        : juce::ToggleButton()
    {
        // JUCE 8 regression: juce::ToggleButton no longer defaults to
        // clickTogglesState = true in its constructor (it did in JUCE 7).
        // Without this explicit call, a real mouse click goes through
        // Button::internalClickCallback() which, with clickTogglesState
        // == false, fires ONLY sendClickMessage(modifiers) and never
        // calls setToggleState. As a result, the AudioParameterBool
        // (custom_i) is never written by the click and the user cannot
        // add/remove notes from the scale in the live plugin. The unit
        // test in ScaleKeyboardComponentTest.cpp uses our
        // triggerClick() override which does the setToggleState
        // explicitly, so the bug was invisible to the test suite.
        setClickingTogglesState (true);

        // Register the internal InteractionListener so that whenever the
        // base class Button::clicked() fires its `sendClickMessage` (which
        // notifies all Button::Listener entries in registration order),
        // our listener runs the `activeInScale` sync + `onUserInteraction`
        // dispatch. The juce::ButtonAttachment is created by the caller
        // AFTER the button constructor, so it gets appended to the
        // listener list AFTER us - but ButtonAttachment::buttonClicked
        // does NOT depend on us, it just pushes the new toggle value
        // to the AudioParameterBool. The order in which we run vs the
        // ButtonAttachment is therefore irrelevant: by the time
        // buttonClicked is called on either of us, the toggle has
        // already been flipped and the new value is in `getToggleState()`.
        // We add the listener explicitly (it is not auto-registered
        // because InteractionListener is a nested struct, not the
        // button itself).
        interactionListener.owner = this;
        addListener (&interactionListener);
    }

    PianoKeyButton::~PianoKeyButton()
    {
        // Detach the listener so any pending events (e.g. a queued
        // buttonClicked) do not dereference a dangling owner pointer
        // if the button is destroyed mid-dispatch.
        removeListener (&interactionListener);
        interactionListener.owner = nullptr;
    }

    void PianoKeyButton::setParentComponent (ScaleKeyboardComponent* parent)
    {
        parentComponent = parent;
    }

    bool PianoKeyButton::isInteractionSuppressed() const
    {
        return parentComponent != nullptr && parentComponent->isUpdatingFromScaleCombo();
    }

    void PianoKeyButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);

        // Active state: use toggle state for Custom mode, activeInScale for preset modes
        bool isActive = activeInScale || getToggleState();

        // Colours
        juce::Colour baseColour;
        if (isBlack) {
            baseColour = isActive ? juce::Colour(0xff0088ff) : juce::Colour(0xff333333); // Blue when active, dark gray otherwise
        } else {
            baseColour = isActive ? juce::Colour(0xff00aaff) : juce::Colour(0xff777777); // Light blue when active, medium gray otherwise
        }

        if (shouldDrawButtonAsDown) {
            baseColour = baseColour.brighter(0.2f);
        } else if (shouldDrawButtonAsHighlighted) {
            baseColour = baseColour.brighter(0.1f);
        }

        // Key drawing
        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 2.0f);

        // Border
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

        // Light 3D effect
        if (!isBlack) {
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.2f), 0, 0,
                                                   juce::Colours::transparentWhite, 0, bounds.getHeight() * 0.2f, false));
            g.fillRoundedRectangle(bounds, 2.0f);
        } else {
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.1f), 0, 0,
                                                   juce::Colours::transparentWhite, bounds.getWidth(), 0, false));
            g.fillRoundedRectangle(bounds, 2.0f);
        }
    }

    // === ScaleKeyboardComponent ===

    ScaleKeyboardComponent::ScaleKeyboardComponent()
    {
        // Note initialization
        // 0:C, 1:C#, 2:D, 3:D#, 4:E, 5:F, 6:F#, 7:G, 8:G#, 9:A, 10:A#, 11:B
        bool isBlackKey[12] = { false, true, false, true, false, false, true, false, true, false, true, false };

        for (int i = 0; i < 12; ++i)
        {
            keys[i].setNoteIndex(i, isBlackKey[i]);
            keys[i].setParentComponent (this);
            addAndMakeVisible(keys[i]);
        }
    }

    void ScaleKeyboardComponent::paint (juce::Graphics& g)
    {
        // Background around the keyboard
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
    }

    void ScaleKeyboardComponent::resized()
    {
        auto bounds = getLocalBounds().reduced(2); // Padding
        float whiteKeyWidth = bounds.getWidth() / 7.0f;
        float blackKeyWidth = whiteKeyWidth * 0.65f;
        float blackKeyHeight = bounds.getHeight() * 0.6f;

        int whiteIndex = 0;

        // First place the white keys
        for (int i = 0; i < 12; ++i)
        {
            if (!keys[i].getIsBlack())
            {
                keys[i].setBounds(bounds.getX() + static_cast<int>(whiteIndex * whiteKeyWidth),
                                  bounds.getY(),
                                  static_cast<int>(whiteKeyWidth) + 1, // +1 to avoid gaps
                                  bounds.getHeight());
                whiteIndex++;
            }
        }

        // Then the black keys on top
        whiteIndex = 0;
        for (int i = 0; i < 12; ++i)
        {
            if (keys[i].getIsBlack())
            {
                // The black key sits between the previous and next white keys
                keys[i].setBounds(bounds.getX() + static_cast<int>(whiteIndex * whiteKeyWidth - blackKeyWidth / 2.0f),
                                  bounds.getY(),
                                  static_cast<int>(blackKeyWidth),
                                  static_cast<int>(blackKeyHeight));
            }
            else
            {
                whiteIndex++;
            }

            // Make sure the black keys are in front
            if (keys[i].getIsBlack()) {
                keys[i].toFront(false);
            }
        }
    }
}




