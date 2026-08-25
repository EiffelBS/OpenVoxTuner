#pragma once
// ScaleKeyboardComponentTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/ui/ScaleKeyboardComponent.h"

class ScaleKeyboardComponentTest : public juce::UnitTest
{
public:
    ScaleKeyboardComponentTest() : juce::UnitTest ("ScaleKeyboardComponent") {}

    void runTest() override
    {
        using ui::PianoKeyButton;
        using ui::ScaleKeyboardComponent;

        // Regression test (2026-07-17, Fix AB): in JUCE 8, the
        // `juce::ToggleButton` constructor no longer sets
        // `clickTogglesState = true` (it did in JUCE 7). Without
        // an explicit call to `setClickingTogglesState(true)` in
        // `PianoKeyButton::PianoKeyButton`, a real mouse click
        // goes through `Button::internalClickCallback` which, with
        // `clickTogglesState == false`, only fires
        // `sendClickMessage(modifiers)` and never calls
        // `setToggleState`. The AudioParameterBool (`custom_i`)
        // connected via `ButtonAttachment` is therefore never
        // written by the click, and the user cannot add/remove
        // notes from the scale in the live plugin. The unit test
        // below catches this by asserting that a freshly
        // constructed `PianoKeyButton` is actually toggleable on
        // click.
        beginTest ("PianoKeyButton is toggleable on click (JUCE 8 regression)");
        {
            PianoKeyButton btn;
            expect (btn.isToggleable(),
                "A freshly constructed PianoKeyButton must be "
                "toggleable on click (clickTogglesState = true). Without "
                "this, a real mouse click only fires sendClickMessage "
                "without ever calling setToggleState, so the "
                "AudioParameterBool custom_i is never written by the "
                "click and the user cannot add/remove notes from the "
                "scale in the plugin.");
        }

        // The rendering logic of PianoKeyButton::paintButton is:
        //   isActive = activeInScale || getToggleState()
        // The invariant we want to verify: after a real user click
        // (mouseDown + mouseUp -> clicked -> sendClickMessage ->
        // all Button::Listener notified), isActive ==
        // getToggleState() AND the onUserInteraction callback is
        // invoked (it is what switches the scale combo to "Custom").
        //
        // To simulate a user click in a portable unit test, we call
        // `triggerClick()`: it is the public method of juce::Button
        // that simulates a mouseDown + mouseUp + clicked. That is
        // exactly what a real mouse triggers at runtime, and thus
        // what the ButtonAttachment observes.

        // 1) In C Natural Minor, key D (index 2) belongs to the
        //    scale: activeInScale=true, getToggleState()=true
        //    (the problematic case reported by the user).
        beginTest ("Click OFF on a preset-scale key: visual follows toggle (OFF state)");
        {
            ScaleKeyboardComponent comp;
            auto& btnD = comp.getButton (2); // D

            // Setup: simulate "C Natural Minor" + toggle ON.
            btnD.setActiveInScale (true);
            btnD.setToggleState (true, juce::dontSendNotification);

            // Capture the onUserInteraction callback (at runtime it
            // switches the scale combo to "Custom" via the `scale`
            // AudioParameterChoice).
            int interactionCount = 0;
            btnD.onUserInteraction = [&interactionCount] { ++interactionCount; };

            // Simulate a full user click via triggerClick().
            // The base class Button::clicked() does setToggleState(!current)
            // + sendClickMessage, which propagates to the ButtonAttachment
            // AND to our InteractionListener.
            btnD.triggerClick();

            // Verification: after triggerClick(), the toggle is OFF,
            // the visual follows (activeInScale == false), and the
            // onUserInteraction callback was called once.
            expect (! btnD.getToggleState(),
                "Toggle of D after the click must be OFF (the base "
                "class Button::clicked() did setToggleState(!current))");
            expect (! btnD.isActiveInScale(),
                "activeInScale must follow the toggle (OFF state) "
                "so the visual correctly reflects the click. Otherwise "
                "paintButton (activeInScale || getToggleState()) keeps "
                "showing the key as active.");
            expect (interactionCount == 1,
                "onUserInteraction must be called once after the "
                "click, to switch the scale combo to Custom. Without "
                "this, the preset -> Custom transition does not happen "
                "and key D stays visually frozen.");
        }

        // 2) In C Natural Minor, key D# (index 3) is NOT part of the
        //    scale: activeInScale=false. If the user wants to enable
        //    it, the ON click must be visible.
        beginTest ("Click ON on an out-of-scale preset key: visual follows toggle (ON state)");
        {
            ScaleKeyboardComponent comp;
            auto& btnDs = comp.getButton (3); // D#

            btnDs.setActiveInScale (false);
            btnDs.setToggleState (false, juce::dontSendNotification);

            int interactionCount = 0;
            btnDs.onUserInteraction = [&interactionCount] { ++interactionCount; };

            // User click -> triggerClick() -> toggle ON + sync.
            btnDs.triggerClick();

            expect (btnDs.getToggleState(),
                "Toggle of D# after the click must be ON");
            expect (btnDs.isActiveInScale(),
                "activeInScale must follow the toggle (ON state) "
                "so the visual correctly reflects the click.");
            expect (interactionCount == 1,
                "onUserInteraction must be called once after the "
                "click, to switch the combo to Custom.");
        }

        // 3) In Custom, toggling freely is allowed. We check that the
        //    invariant holds in both directions.
        beginTest ("Successive toggles in Custom: visual follows the toggle");
        {
            ScaleKeyboardComponent comp;
            auto& btn = comp.getButton (5); // F

            // Setup: F is ON (was part of the previous scale)
            btn.setActiveInScale (true);
            btn.setToggleState (true, juce::dontSendNotification);

            for (int k = 0; k < 3; ++k)
            {
                // User click -> triggerClick()
                btn.triggerClick();
                // The invariant: both must be equal
                expect (btn.isActiveInScale() == btn.getToggleState(),
                    "After iteration " + juce::String (k)
                    + ": activeInScale and getToggleState() must match. "
                    + "activeInScale=" + juce::String (btn.isActiveInScale() ? "true" : "false")
                    + ", getToggleState()=" + juce::String (btn.getToggleState() ? "true" : "false"));
            }
        }

        // 4) Verification that triggerClick() is indeed the right
        //    entry point (it is the only public method simulating a
        //    full mouseDown+mouseUp + clicked). The standard JUCE
        //    logic of Button::clicked() does setToggleState(!)
        //    + sendClickMessage (which notifies the ButtonAttachment
        //    and our InteractionListener). Our InteractionListener
        //    does the rest: sync activeInScale + onUserInteraction.
        beginTest ("triggerClick() is the proper public entry point (simulates a full mouseDown+Up)");
        {
            ScaleKeyboardComponent comp;
            auto& btn = comp.getButton (0); // C

            btn.setActiveInScale (true);
            btn.setToggleState (false, juce::dontSendNotification);

            int interactionCount = 0;
            btn.onUserInteraction = [&interactionCount] { ++interactionCount; };

            // Simulate a user click: JUCE calls triggerClick()
            // which simulates a successful mouseDown + mouseUp cycle.
            // It is the public method (the only accessible one)
            // holding the logic under test.
            btn.triggerClick();

            // After triggerClick(), the base class Button::clicked()
            // has toggled the state, the ButtonAttachment was notified
            // (custom_i updated), the InteractionListener did the
            // activeInScale sync and called onUserInteraction.
            expect (btn.getToggleState(),
                "Toggle of C after triggerClick() must be ON (base "
                "class Button::clicked() toggles !current)");
            expect (btn.isActiveInScale(),
                "activeInScale must follow the toggle after triggerClick() "
                "(the InteractionListener does the sync)");
            expect (interactionCount == 1,
                "onUserInteraction must be called once per "
                "triggerClick() (the InteractionListener makes the call)");
        }

        // 5) Regression (2026-07-17, Fix AC): when the parent
        // ScaleKeyboardComponent is in "updating from scale combo" mode,
        // a programmatic toggle (e.g. scaleBox.onChange rewriting all
        // custom_i) must NOT fire onUserInteraction (which would switch
        // the scale combo back to "Custom" and cancel the user's preset
        // selection). The guard is isUpdatingFromScaleCombo().
        beginTest ("onUserInteraction suppressed during update from combo (Fix AC regression)");
        {
            ScaleKeyboardComponent comp;
            auto& btn = comp.getButton (0); // C

            btn.setActiveInScale (true);
            btn.setToggleState (true, juce::dontSendNotification);

            int interactionCount = 0;
            btn.onUserInteraction = [&interactionCount] { ++interactionCount; };

            // Simulate scaleBox.onChange programmatic update: suppress first.
            comp.setUpdatingFromScaleCombo (true);
            btn.triggerClick(); // would normally toggle + fire callback
            comp.setUpdatingFromScaleCombo (false);

            expect (interactionCount == 0,
                "During a programmatic update from the combo "
                "(setUpdatingFromScaleCombo(true)), a toggle must NOT "
                "call onUserInteraction. Without this, selecting a "
                "preset scale in the combo would reset it back to "
                "'Custom'.");
            expect (btn.getToggleState() == false,
                "The visual toggle must still be applied (OFF state) "
                "even though onUserInteraction is suppressed.");
        }
    }
};

static ScaleKeyboardComponentTest scaleKeyboardComponentTest;


