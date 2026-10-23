// PluginEditorImport.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// File drag & drop and the two import paths (MIDI file -> curve, audio file
// -> curve), including the dialogs they launch. Split out of
// PluginEditor.cpp (step 2 of the source split) verbatim.
#include "PluginEditor.h"

#include "ui/AudioImportDialogComponent.h"
#include "ui/MidiChannelSelectorComponent.h"

#include "dsp/AudioCurveImporter.h"
#include "dsp/MidiImporter.h"

// =============================================================================
// MIDI and audio Import (drag-and-drop + menu)
// =============================================================================

// Audio files the importer can decode. MP3 is deliberately absent: JUCE's MP3
// decoder is disabled in this build, so offering it would only fail after the
// user picked the file. One helper keeps the drop target, the file chooser and
// the analysis agreeing on the same set.
static bool isAudioImportFile (const juce::String& path)
{
    const auto lower = path.toLowerCase();
    return lower.endsWith (".wav")  || lower.endsWith (".aiff")
        || lower.endsWith (".aif")  || lower.endsWith (".flac")
        || lower.endsWith (".ogg");
}

bool OpenVoxTunerAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
    {
        const auto lower = f.toLowerCase();
        if (lower.endsWith (".mid") || lower.endsWith (".midi")
            || isAudioImportFile (f))
            return true;
    }
    return false;
}

void OpenVoxTunerAudioProcessorEditor::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    juce::File midiFile;
    for (const auto& f : files)
    {
        juce::File candidate (f);
        if (candidate.existsAsFile()
            && (f.toLowerCase().endsWith (".mid") || f.toLowerCase().endsWith (".midi")))
        {
            midiFile = candidate;
            break;
        }
    }

    if (midiFile.existsAsFile())
    {
        handleMidiFileImport (midiFile);
        return;
    }

    // No MIDI in the drop: fall back to the FIRST audio file. Taking the first
    // (rather than looping over all of them) keeps a multi-file drop
    // deterministic instead of silently replacing the curve several times.
    for (const auto& f : files)
    {
        juce::File candidate (f);
        if (candidate.existsAsFile() && isAudioImportFile (f))
        {
            handleAudioFileImport (candidate);
            return;
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::handleMidiFileImport (const juce::File& midiFile)
{
    // Ensure we are on the Curve Editor tab
    if (tabbedComponent.getCurrentTabIndex() != 1)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            ovt::tr (ovt::Keys::kMidiErrorWrongTab));
        return;
    }

    // Analyze the file
    auto info = ovtdsp::MidiImporter::analyzeFile (midiFile);

    if (! info.isValid)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            info.errorMessage);
        return;
    }

    // Single non-percussion channel: import directly with "highest note" strategy
    if (info.channels.size() == 1)
    {
        auto curve = ovtdsp::MidiImporter::importFrom (
            midiFile,
            ovtdsp::MidiExtractStrategy::HighestNote,
            info.channels.getFirst().channel);

        applyMidiImport (curve, midiFile.getFileName());
        return;
    }

    // Multiple channels: show selection dialog
    showMidiChannelDialog (midiFile, info);
}

void OpenVoxTunerAudioProcessorEditor::applyMidiImport (
    const ovtdsp::PitchCurve& newCurve,
    const juce::String& sourceName)
{
    if (curveEditor == nullptr)
        return;

    if (newCurve.getNumPoints() < 2)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            ovt::tr (ovt::Keys::kMidiErrorTooFew));
        return;
    }

    curveEditor->importCurve (newCurve);
}

// =============================================================================
// MIDI Import: Multi-channel selection dialog
// =============================================================================

void OpenVoxTunerAudioProcessorEditor::showMidiChannelDialog (
    const juce::File& midiFile,
    const ovtdsp::MidiImportInfo& info)
{
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = ovt::tr (ovt::Keys::kMidiDialogTitle)
                     + ": " + midiFile.getFileName();

    auto* selectorComp = new MidiChannelSelectorComponent (
        info,
        [editorSafe = juce::Component::SafePointer<OpenVoxTunerAudioProcessorEditor> (this), midiFile]
        (ovtdsp::MidiExtractStrategy s, int ch)
        {
            if (editorSafe != nullptr)
            {
                auto curve = ovtdsp::MidiImporter::importFrom (midiFile, s, ch);
                editorSafe->applyMidiImport (curve, midiFile.getFileName());
            }
        });
    selectorComp->setSize (420, 260);
    opts.content.set (selectorComp, true);
    opts.useNativeTitleBar = true;
    opts.escapeKeyTriggersCloseButton = true;
    opts.resizable = false;
    opts.launchAsync();
}

// =============================================================================
// Audio Import (drag-and-drop + menu)
// =============================================================================

void OpenVoxTunerAudioProcessorEditor::handleAudioFileImport (const juce::File& audioFile)
{
    // Same gate as the MIDI path: the curve lands in the Curve Editor, so
    // importing from another tab would look like a no-op to the user.
    if (tabbedComponent.getCurrentTabIndex() != 1)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kAudioDialogTitle),
            ovt::tr (ovt::Keys::kAudioErrorWrongTab));
        return;
    }

    // Analysis decodes the whole file and runs the detector: the module documents
    // that this belongs on the message thread (never in processBlock), and here
    // it runs as one-shot work for an explicit user action.
    auto info = ovtdsp::AudioCurveImporter::analyzeFile (audioFile);

    if (! info.isValid)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kAudioDialogTitle),
            info.errorMessage);
        return;
    }

    // No voiced pitch at all: the dialog would offer an Import button that
    // produces an empty curve, so refuse before showing it.
    if (info.numNotes == 0)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kAudioDialogTitle),
            ovt::tr (ovt::Keys::kAudioErrorNoNotes));
        return;
    }

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = ovt::tr (ovt::Keys::kAudioDialogTitle)
                     + ": " + audioFile.getFileName();

    auto* dialogComp = new AudioImportDialogComponent (
        info,
        // Capture the editor weakly: the file chooser and the dialog both
        // outlive a single message-loop turn, and the editor can be destroyed
        // by the host while the modal is still open.
        [editorSafe = juce::Component::SafePointer<OpenVoxTunerAudioProcessorEditor> (this), audioFile]
        (bool snapToScale)
        {
            if (editorSafe == nullptr)
                return;

            // Read the scale at confirm time (not at dialog-open time) so the
            // snap target follows a key/scale change made in the meantime.
            juce::Array<int> noIntervals;
            const auto& intervals = (editorSafe->curveEditor != nullptr)
                                        ? editorSafe->curveEditor->getScaleIntervals()
                                        : noIntervals;

            auto curve = ovtdsp::AudioCurveImporter::importFrom (
                audioFile, snapToScale, intervals);
            editorSafe->applyAudioImport (curve, audioFile.getFileName());
        });
    dialogComp->setSize (460, 300);
    opts.content.set (dialogComp, true);
    opts.useNativeTitleBar = true;
    opts.escapeKeyTriggersCloseButton = true;
    opts.resizable = false;
    opts.launchAsync();
}

void OpenVoxTunerAudioProcessorEditor::applyAudioImport (
    const ovtdsp::PitchCurve& newCurve,
    const juce::String& sourceName)
{
    if (curveEditor == nullptr)
        return;

    // Same guard as the MIDI path: a single point cannot be edited or played,
    // so refuse instead of replacing the user's curve with a stub.
    if (newCurve.getNumPoints() < 2)
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::WarningIcon,
            ovt::tr (ovt::Keys::kAudioDialogTitle),
            ovt::tr (ovt::Keys::kAudioErrorTooFew));
        return;
    }

    juce::ignoreUnused (sourceName); // no per-source bookkeeping (same as MIDI)

    curveEditor->importCurve (newCurve);
}

