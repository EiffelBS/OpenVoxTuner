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
            // Switch to Custom mode FIRST, then toggle silently.
            // This avoids a feedback loop where setValueNotifyingHost on the
            // scale parameter causes a parameter sync that resets the button state.
            if (onUserInteraction)
                onUserInteraction();
            setToggleState(!getToggleState(), juce::sendNotification);
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
