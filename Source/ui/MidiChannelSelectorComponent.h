// MidiChannelSelectorComponent.h
// OpenVoxTuner UI components
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// MIDI import: the multi-channel selection dialog. The analysis has already
// run when this opens, so the dialog only reports the channels found and
// collects the strategy / channel the importer should use. Extracted
// verbatim from PluginEditor.cpp (step 1 of the source split).

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "dsp/MidiImporter.h"
#include "dsp/NoteUtils.h"
#include "ui/OVTLanguages.h"

class MidiChannelSelectorComponent : public juce::Component
{
public:
    MidiChannelSelectorComponent (
        const ovtdsp::MidiImportInfo& info,
        std::function<void (ovtdsp::MidiExtractStrategy, int)> onConfirm)
        : importInfo (info), confirmCallback (std::move (onConfirm))
    {
        setSize (420, 260);

        // Strategy combo
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyHighest), 1);
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyLowest),  2);
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyLoudest), 3);
        strategyCombo.addItem (ovt::tr (ovt::Keys::kMidiStrategyChannel), 4);
        strategyCombo.setSelectedId (1, juce::dontSendNotification);
        strategyCombo.onChange = [this] { updateChannelVisibility(); };
        addAndMakeVisible (strategyCombo);

        // Channel combo (populated from info)
        for (const auto& ch : info.channels)
        {
            channelCombo.addItem (
                "Channel " + juce::String (ch.channel)
                + " (" + juce::String (ch.numNotes) + " notes, "
                + ovtdsp::noteInOctaveName (ovtdsp::midiToNoteInOctave (ch.minNote))
                + juce::String (ovtdsp::midiToOctave (ch.minNote)) + " - "
                + ovtdsp::noteInOctaveName (ovtdsp::midiToNoteInOctave (ch.maxNote))
                + juce::String (ovtdsp::midiToOctave (ch.maxNote)) + ")",
                ch.channel);
        }
        if (info.channels.size() > 0)
            channelCombo.setSelectedId (info.channels.getFirst().channel,
                                        juce::dontSendNotification);
        addAndMakeVisible (channelCombo);

        updateChannelVisibility();

        // Buttons
        importButton.setButtonText (ovt::tr (ovt::Keys::kMidiBtnImport));
        importButton.onClick = [this] { onImportClicked(); };
        addAndMakeVisible (importButton);

        cancelButton.setButtonText (ovt::tr (ovt::Keys::kMenuCancel));
        cancelButton.onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };
        addAndMakeVisible (cancelButton);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff2d2d2d));
        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (14.0f);
        g.drawText (ovt::tr (ovt::Keys::kMidiStrategy),
                    16, 24, 80, 24, juce::Justification::centredLeft);
        g.drawText (ovt::tr (ovt::Keys::kMidiChannel),
                    16, 64, 80, 24, juce::Justification::centredLeft);
    }

    void resized() override
    {
        strategyCombo.setBounds (100, 24, 300, 24);
        channelCombo.setBounds (100, 64, 300, 24);
        cancelButton.setBounds (230, 210, 80, 30);
        importButton.setBounds (320, 210, 80, 30);
    }

private:
    void updateChannelVisibility()
    {
        bool showChannel = (strategyCombo.getSelectedId() == 4);
        channelCombo.setVisible (showChannel);
    }

    void onImportClicked()
    {
        auto strategy = static_cast<ovtdsp::MidiExtractStrategy> (
            strategyCombo.getSelectedId() - 1);
        int channel = channelCombo.getSelectedId();
        confirmCallback (strategy, channel);

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }

    ovtdsp::MidiImportInfo importInfo;
    std::function<void (ovtdsp::MidiExtractStrategy, int)> confirmCallback;
    juce::ComboBox strategyCombo, channelCombo;
    juce::TextButton importButton, cancelButton;
};
