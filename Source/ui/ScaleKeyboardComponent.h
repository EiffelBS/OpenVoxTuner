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

        void mouseDown (const juce::MouseEvent& e) override
        {
            // Toggle state — onClick handler in PluginEditor will write
            // the parameter and switch to Custom mode.
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
