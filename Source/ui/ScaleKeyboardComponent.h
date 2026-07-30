// ScaleKeyboardComponent.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.


#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "../dsp/NoteUtils.h"

namespace ui
{
    class ScaleKeyboardComponent;

    class PianoKeyButton : public juce::ToggleButton
    {
    public:
        PianoKeyButton();
        ~PianoKeyButton() override;

        void setNoteIndex(int index, bool black)
        {
            noteIndex = index;
            isBlack = black;
        }

        bool getIsBlack() const { return isBlack; }

        /** Set whether this note is in the current scale (used for non-Custom modes). */
        void setActiveInScale (bool active) { activeInScale = active; }
        bool isActiveInScale() const { return activeInScale; }

        /** Set the parent ScaleKeyboardComponent (used to check suppression flag). */
        void setParentComponent (ScaleKeyboardComponent* parent);

        /** Check if interaction should be suppressed (e.g. during programmatic scale update). */
        bool isInteractionSuppressed() const;

        void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        std::function<void()> onUserInteraction;

        // Simulate a full user click cycle (test-only). juce::Button::triggerClick
        // is public and fires sendClickMessage(modifiers), but it does NOT
        // call the protected Button::clicked() (which is the path that does
        // the setToggleState + sendNotification). To exercise the same code
        // paths a real mouse click would, we toggle the state here before
        // delegating to the base class. This is also the right behaviour
        // for any non-mouse caller (e.g. a keyboard shortcut) that wants
        // the button to behave exactly like a clicked button.
        void triggerClick() override
        {
            setToggleState (! getToggleState(), juce::sendNotification);
            juce::Button::triggerClick();
        }

        // No override of `clicked()` here on purpose. `juce::Button::sendClickMessage`
        // (which fires the Button::Listener::buttonClicked callbacks that the
        // juce::ButtonAttachment relies on) is **private** in this version of
        // JUCE, so we cannot call it from a subclass. Instead, the base
        // class `Button::clicked()` itself does the work (setToggleState(!,
        // sendNotification) + sendClickMessage(modifiers)), and we listen to
        // the result via an internal `Button::Listener` (see constructor +
        // ~PianoKeyButton). That listener fires the same `onUserInteraction`
        // callback that the old `clicked()` override used to, plus the
        // `activeInScale` visual sync, but does so in the same dispatch
        // cycle as the ButtonAttachment â€” so both the user-visible toggle
        // AND the AudioParameterBool (custom_i) get updated in one go, with
        // no manual `sendClickMessage` call and no risk of a
        // changeNotification stack overflow.
        //
        // Bug history: in 2026-07-17 Fix R we tried to override `clicked()`
        // and only do `setToggleState(!, dontSendNotification)` +
        // `onUserInteraction()`, skipping the base class entirely. That
        // worked in unit tests but the live plugin showed two regressions:
        //   (a) the user could not toggle preset-scale notes off (D in
        //       C Natural Minor stayed lit because custom_i was never
        //       written);
        //   (b) once in Custom mode, switching to a different scale
        //       preserved stale custom_i values because the
        //       scaleBox.onChange() flow reads custom_i to seed the new
        //       preset â€” but custom_i was never updated by the click.
        // The root cause was that we never fired the Button::Listener
        // chain, so the ButtonAttachment never pushed the new toggle
        // value into the AudioParameterBool. Listening via an internal
        // `Button::Listener` (set up in the constructor) fixes both
        // regressions without any need to call private JUCE internals.

    private:
        // Internal listener that runs after the base class Button::clicked()
        // has dispatched to all the Button::Listener entries. We are
        // appended to the listener list in the constructor, so we run
        // AFTER the juce::ButtonAttachment (which is appended first when
        // the user creates the attachment, AFTER the button constructor).
        // Order matters: the ButtonAttachment pushes the new value to the
        // AudioParameterBool FIRST, then we sync `activeInScale` and fire
        // `onUserInteraction` (which switches the scale combo to "Custom"
        // silently via a rawScale->store(1.0f)).
        struct InteractionListener : public juce::Button::Listener
        {
            PianoKeyButton* owner = nullptr;
            void buttonClicked (juce::Button* b) override
            {
                if (owner == nullptr || b != owner)
                    return;
                // The toggle is now flipped and custom_i is now written
                // (both done by the base class + ButtonAttachment path).
                // Mirror the new state into `activeInScale` so paintButton()
                // reflects the change immediately, without waiting for the
                // next refreshVisualizer() tick (~16 ms later).
                owner->activeInScale = owner->getToggleState();
                
                // Only fire onUserInteraction if not suppressed (e.g. when
                // scaleBox.onChange updates the buttons programmatically).
                if (owner->isInteractionSuppressed())
                    return;
                if (owner->onUserInteraction)
                    owner->onUserInteraction();
            }
        };

        InteractionListener interactionListener;
        ScaleKeyboardComponent* parentComponent = nullptr;
        int noteIndex = 0;
        bool isBlack = false;
        bool activeInScale = true; // default: all notes active (chromatic)

        // Allow isSuppressed() to read parentComponent without making it public.
        friend bool isSuppressed (PianoKeyButton*);
    };

    class ScaleKeyboardComponent : public juce::Component
    {
    public:
        ScaleKeyboardComponent();
        ~ScaleKeyboardComponent() override = default;

        // Forward declaration of friend helper (defined after class)
        friend bool isSuppressed (PianoKeyButton*);

        void paint (juce::Graphics& g) override;
        void resized() override;

        PianoKeyButton& getButton(int index) { return keys[index]; }

        /** Set the suppression flag while programmatically updating button state (e.g. from scaleBox.onChange). */
        void setUpdatingFromScaleCombo (bool updating) { updatingFromScaleComboFlag = updating; }

        /** Check if we are currently updating button state from the scale combo (to suppress onUserInteraction). */
        bool isUpdatingFromScaleCombo() const { return updatingFromScaleComboFlag; }

        /** Update which notes are visually active in the current scale. */
        void setActiveScaleIntervals (const juce::Array<int>& intervals)
        {
            for (int i = 0; i < 12; ++i)
                keys[i].setActiveInScale (intervals.contains (i));
            repaint();
        }

    private:
        std::array<PianoKeyButton, 12> keys;
        bool updatingFromScaleComboFlag = false;
    };
}




