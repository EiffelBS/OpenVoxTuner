// AudioImportDialogComponent.h
// OpenVoxTuner UI components
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Audio import: the confirmation dialog shown before an audio file is turned
// into a curve. Extracted verbatim from PluginEditor.cpp (step 1 of the
// source split); its doc comment moved with it.

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "dsp/AudioCurveImporter.h"
#include "ui/OVTLanguages.h"

/**
 * Confirmation dialog shown before an audio file is turned into a curve.
 *
 * Audio counterpart of MidiChannelSelectorComponent: the analysis already ran,
 * so this dialog only reports what was found and collects the one option the
 * importer takes (snapping). It reuses that dialog's layout on purpose - same
 * dark card, same bottom-right Cancel/Import pair - so the two import paths do
 * not look like two different features.
 */
class AudioImportDialogComponent : public juce::Component
{
public:
    AudioImportDialogComponent (const ovtdsp::AudioImportInfo& audioInfo,
                                std::function<void (bool)> onConfirm)
        : info (audioInfo), confirmCallback (std::move (onConfirm))
    {
        setSize (460, 300);

        // Snapping is ON by default: a sung take is never perfectly in tune, so
        // the snapped curve is the useful starting point. The user can import
        // again unsnapped, or edit the points afterwards.
        snapButton.setButtonText (ovt::tr (ovt::Keys::kAudioSnapToScale));
        snapButton.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (snapButton);

        importButton.setButtonText (ovt::tr (ovt::Keys::kAudioBtnImport));
        importButton.setComponentID ("primary"); // accent fill for the action button
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

        // One row per fact: dim caption on the left, value aligned to the right
        // edge. Values are plain numbers with their unit, never a file path or an
        // internal name (user-facing text rule). Right alignment keeps the value
        // column clear of the captions, whose length varies a lot per language.
        auto drawRow = [&g] (const juce::String& caption, const juce::String& value, int y)
        {
            g.setColour (juce::Colours::white.withAlpha (0.8f));
            g.setFont (14.0f);
            g.drawText (caption, 16, y, 300, 24, juce::Justification::centredLeft);
            g.setColour (juce::Colours::white);
            g.drawText (value, 16, y, 428, 24, juce::Justification::centredRight);
        };

        drawRow (ovt::tr (ovt::Keys::kAudioDuration),
                 juce::String (info.durationSec, 2) + " s", 18);
        drawRow (ovt::tr (ovt::Keys::kAudioSampleRate),
                 juce::String (info.sampleRate) + " Hz", 46);
        drawRow (ovt::tr (ovt::Keys::kAudioChannels),
                 juce::String (info.numChannels), 74);
        drawRow (ovt::tr (ovt::Keys::kAudioNotesDetected),
                 juce::String (info.numNotes), 102);

        // Detected pitch span, useful context for the user to judge the result.
        // Hidden when there is nothing to show (the caller never opens the
        // dialog for a file with no detected note).
        if (info.numNotes > 0 && info.minDetectedHz > 0.0f && info.maxDetectedHz > 0.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.setFont (12.0f);
            g.drawText (juce::String (info.minDetectedHz, 1) + " - "
                            + juce::String (info.maxDetectedHz, 1) + " Hz",
                        16, 128, 428, 20, juce::Justification::centredRight);
        }

        // Formats hint: the MP3 limitation has to be visible BEFORE the import,
        // otherwise the only feedback for an MP3 is an unexplained failure.
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (12.0f);
        g.drawFittedText (ovt::tr (ovt::Keys::kAudioFormatsHint),
                          16, 190, 428, 40, juce::Justification::topLeft, 2);
    }

    void resized() override
    {
        snapButton.setBounds (16, 156, 300, 24);
        cancelButton.setBounds (250, 252, 90, 30);
        importButton.setBounds (352, 252, 90, 30);
    }

private:
    void onImportClicked()
    {
        confirmCallback (snapButton.getToggleState());

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }

    ovtdsp::AudioImportInfo info;
    std::function<void (bool)> confirmCallback;
    juce::ToggleButton snapButton;
    juce::TextButton importButton, cancelButton;
};
