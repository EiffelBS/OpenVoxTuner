// ScaleKeyboardComponent.h
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "../dsp/NoteUtils.h"

namespace ui
{
    class PianoKeyButton : public juce::ToggleButton
    {
    public:
        PianoKeyButton() : juce::ToggleButton() {}
        ~PianoKeyButton() override = default;

        void setNoteIndex(int index, bool black) 
        { 
            noteIndex = index; 
            isBlack = black; 
        }
        
        bool getIsBlack() const { return isBlack; }

        void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        std::function<void()> onUserInteraction;

        void mouseDown (const juce::MouseEvent& e) override
        {
            // Toggle immediately here (don't wait for mouseUp/clicked).
            // JUCE's ToggleButton::mouseDown does NOT toggle — it only records
            // the down state. The actual toggle happens in mouseUp -> clicked().
            // We need the toggle BEFORE onUserInteraction switches to Custom,
            // so the parameter re-sync reads the CORRECT new state.
            setToggleState(!getToggleState(), juce::sendNotification);
            if (onUserInteraction)
                onUserInteraction();
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            // Only clean up visual state — do NOT call clicked() which would
            // toggle the button again (undoing the mouseDown toggle).
            setState (juce::Button::buttonNormal);
        }

    private:
        int noteIndex = 0;
        bool isBlack = false;
    };

    class ScaleKeyboardComponent : public juce::Component
    {
    public:
        ScaleKeyboardComponent();
        ~ScaleKeyboardComponent() override = default;

        void paint (juce::Graphics& g) override;
        void resized() override;

        PianoKeyButton& getButton(int index) { return keys[index]; }

    private:
        std::array<PianoKeyButton, 12> keys;
    };
}
