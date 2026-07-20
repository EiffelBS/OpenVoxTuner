// OpenVoxKeyEditor.h
// Minimal editor for the OpenVoxKey companion plug-in: a group selector (A/B/C/D)
// and a live display of the detected key/scale. Polled from the processor on a
// timer (the detection runs in the audio thread and the result is published to
// the KeyBridge, but we also expose it directly for the on-screen readout).

#pragma once

#include <cmath>
#include "OpenVoxKeyProcessor.h"

class OpenVoxKeyEditor : public juce::AudioProcessorEditor,
                         private juce::Timer,
                         private juce::ComboBox::Listener,
                         private juce::Button::Listener
{
public:
    // Small "searching for key" animation: three dots that light up in sequence.
    // Shown (instead of the static "-") while audio is present but no key has
    // been detected yet, so the user can tell the plug-in is actively analysing
    // rather than idle / silent.
    class SearchingDots : public juce::Component
    {
    public:
        // phase in [0,1) drives the travelling pulse; setActive toggles painting.
        void setPhase (float p) { if (p != phase) { phase = p; repaint(); } }
        void paint (juce::Graphics& g) override
        {
            const int n = 3;
            const float r = 5.0f;
            const float gap = 16.0f;
            const float totalW = static_cast<float> (n - 1) * gap;
            const float x0 = getLocalBounds().toFloat().getCentreX() - totalW * 0.5f;
            const float y  = getLocalBounds().toFloat().getCentreY();
            for (int i = 0; i < n; ++i)
            {
                // Brightness peaks when the travelling pulse is at this dot.
                float d = std::fmod (phase - static_cast<float> (i) / static_cast<float> (n) + 1.0f, 1.0f);
                float b = 1.0f - d;
                b = juce::jlimit (0.2f, 1.0f, b);
                g.setColour (juce::Colours::cyan.withAlpha (b));
                g.fillEllipse (x0 + static_cast<float> (i) * gap - r, y - r, r * 2.0f, r * 2.0f);
            }
        }
    private:
        float phase = 0.0f;
    };

    explicit OpenVoxKeyEditor (OpenVoxKeyProcessor& p)
        : AudioProcessorEditor (p), processor (p)
    {
        setSize (320, 200);

        addAndMakeVisible (titleLabel);
        titleLabel.setText ("OpenVoxKey", juce::dontSendNotification);
        titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
        titleLabel.setColour (juce::Label::textColourId, juce::Colours::cyan);

        addAndMakeVisible (hintLabel);
        hintLabel.setText ("Place on an accompaniment track.\nDetects the key and shares it with OpenVoxTuner (Key/Scale Detection = OpenVoxKey).",
                           juce::dontSendNotification);
        hintLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
        hintLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        hintLabel.setJustificationType (juce::Justification::topLeft);

        addAndMakeVisible (groupLabel);
        groupLabel.setText ("Group", juce::dontSendNotification);
        groupLabel.attachToComponent (&groupBox, true);

        addAndMakeVisible (groupBox);
        groupBox.addItem ("A", 1); groupBox.addItem ("B", 2);
        groupBox.addItem ("C", 3); groupBox.addItem ("D", 4);
        groupBox.setSelectedId (static_cast<int> (processor.getGroup()) + 1);
        groupBox.addListener (this);

        addAndMakeVisible (sendButton);
        sendButton.setTooltip ("Force-send the detected key/scale to OpenVoxTuner now (re-sync even if the detection did not change).");
        sendButton.addListener (this);
        sendButton.setEnabled (false); // enabled by timerCallback once a key is detected

        addAndMakeVisible (keyLabel);
        keyLabel.setText ("Detected key", juce::dontSendNotification);
        keyLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
        keyLabel.setColour (juce::Label::textColourId, juce::Colours::grey);

        addAndMakeVisible (keyValue);
        keyValue.setText ("-", juce::dontSendNotification);
        keyValue.setColour (juce::Label::textColourId, juce::Colours::grey);

        addAndMakeVisible (searchingDots);
        searchingDots.setVisible (false);
        keyValue.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
        keyValue.setColour (juce::Label::textColourId, juce::Colours::white);
        keyValue.setJustificationType (juce::Justification::centred);

        startTimerHz (20); // ~50 ms refresh of the detected key readout
    }

    ~OpenVoxKeyEditor() override
    {
        stopTimer();
        groupBox.removeListener (this);
        sendButton.removeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1d23));

        // Program logo (matches OpenVoxTuner): a stylized "O" with a pitch
        // curve passing through it, drawn to the left of the "OpenVoxKey" title.
        juce::Rectangle<float> logoArea (14.0f, 8.0f, 24.0f, 24.0f);
        g.setColour (juce::Colours::cyan);
        g.drawEllipse (logoArea.reduced (2.0f), 2.5f);

        juce::Path curve;
        curve.startNewSubPath (logoArea.getX() - 4.0f, logoArea.getCentreY() + 4.0f);
        curve.cubicTo (logoArea.getX() + 8.0f,  logoArea.getCentreY() + 4.0f,
                       logoArea.getCentreX(),     logoArea.getY() - 4.0f,
                       logoArea.getRight() + 4.0f, logoArea.getY() + 8.0f);
        g.setColour (juce::Colours::white);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered,
                                                    juce::PathStrokeType::rounded));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (14);
        // Leave room on the left for the logo (drawn in paint()).
        titleLabel.setBounds (area.removeFromTop (28).withTrimmedLeft (28));
        hintLabel.setBounds (area.removeFromTop (44));

        auto groupRow = area.removeFromTop (28);
        groupLabel.setBounds (groupRow.removeFromLeft (56));
        groupBox.setBounds (groupRow.removeFromLeft (80));
        sendButton.setBounds (groupRow.removeFromLeft (72));

        area.removeFromTop (16);
        keyLabel.setBounds (area.removeFromTop (20));
        auto valueRow = area.removeFromTop (40);
        keyValue.setBounds (valueRow);
        searchingDots.setBounds (valueRow);
    }

private:
    void timerCallback() override
    {
        const int key = processor.getCurrentDetectedKey();
        const bool minor = processor.isCurrentDetectedMinor();

        // Enable "Send" only once a key has been detected (forcePublish() is a
        // no-op before then). Done here so it applies in both the detected and
        // the "searching / no signal" branches below (the detected branch
        // returns early, so the old call at the bottom never ran for it).
        sendButton.setEnabled (key >= 0);

        // Audio presence (within the last 2 s) tells "searching" apart from
        // "no signal": a melodic-but-hard track is still being analysed, while a
        // silent/paused bus simply has nothing to analyse.
        const double now  = juce::Time::getCurrentTime().toMilliseconds() / 1000.0;
        const double last = processor.getLastAudioTime();
        const bool audioRecent = (last > 0.0) && ((now - last) < 2.0);

        if (key >= 0)
        {
            static const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                                 "F#", "G", "G#", "A", "A#", "B" };
            const juce::String text = juce::String (noteNames[key % 12])
                                    + (minor ? " Minor" : " Major");
            keyValue.setText (text, juce::dontSendNotification);
            keyValue.setColour (juce::Label::textColourId, juce::Colours::white);
            keyValue.setVisible (true);
            searchingDots.setVisible (false);
            return;
        }

        // No key detected yet: show the animated "searching" dots when audio is
        // present, otherwise a dim "No signal" label.
        if (audioRecent)
        {
            keyValue.setVisible (false);
            searchingDots.setVisible (true);
            // Drive the pulse from a small, high-resolution clock. The system
            // clock in seconds is far too large to fit in a float with
            // sub-second precision, which previously froze the animation. Cycle
            // the phase over ~1.33 s using the millisecond counter instead.
            const juce::uint32 ms = juce::Time::getMillisecondCounter();
            const float phase = static_cast<float> (ms % 1333) / 1333.0f;
            searchingDots.setPhase (phase);
        }
        else
        {
            keyValue.setText ("No signal", juce::dontSendNotification);
            keyValue.setColour (juce::Label::textColourId, juce::Colours::grey);
            keyValue.setVisible (true);
            searchingDots.setVisible (false);
        }

    }

    void comboBoxChanged (juce::ComboBox* combo) override
    {
        if (combo == &groupBox)
        {
            const int idx = juce::jlimit (0, 3, combo->getSelectedId() - 1);
            if (auto* gp = processor.getGroupParameter())
                // AudioParameterChoice stores a normalised 0..1 value, so map the
                // 0..3 index to idx/3 (setIndexNotifyingHost would do the same).
                gp->setValueNotifyingHost (static_cast<float> (idx) / 3.0f);
        }
    }

    void buttonClicked (juce::Button* btn) override
    {
        if (btn == &sendButton)
            processor.forcePublish();
    }

    OpenVoxKeyProcessor& processor;

    juce::Label      titleLabel, hintLabel, groupLabel, keyLabel, keyValue;
    juce::ComboBox    groupBox;
    juce::TextButton  sendButton { "Send" };
    SearchingDots    searchingDots;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenVoxKeyEditor)
};
