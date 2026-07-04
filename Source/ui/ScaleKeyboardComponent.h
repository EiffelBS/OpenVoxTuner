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
            // Toggle the button state. This triggers onClick (used by
            // ButtonAttachment), which writes the custom_i parameter via
            // setValueNotifyingHost. AFTER the toggle, switch to Custom
            // mode silently (no notification cascade).
            setToggleState(!getToggleState(), juce::sendNotification);
            if (onUserInteraction)
                onUserInteraction();
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            // Clean up visual state — do NOT toggle again.
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
