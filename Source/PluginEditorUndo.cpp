// PluginEditorUndo.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The editor-global undo/redo stack: the keyboard shortcuts, the gesture
// snapshots and the listeners that open and close them. Split out of
// PluginEditor.cpp (step 2 of the source split) verbatim.
#include "PluginProcessor.h"
#include "PluginEditor.h"

bool OpenVoxTunerAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Global plugin Undo/Redo (Option 1): Ctrl/Cmd+Z = undo,
    // Ctrl/Cmd+Shift+Z (or Ctrl/Cmd+Y) = redo. These are editor-global and
    // distinct from the Curve Editor's own undo (which only covers curve
    // points and only fires when the Curve Editor has focus).
    if (key.getModifiers().isCommandDown())
    {
        if (key.getKeyCode() == 'Z')
        {
            if (key.getModifiers().isShiftDown())
                performGlobalRedo();
            else
                performGlobalUndo();
            return true;
        }
        if (key.getKeyCode() == 'Y')
        {
            performGlobalRedo();
            return true;
        }
    }
    return false;
}

// === Global plugin Undo/Redo (Option 1) ===

void OpenVoxTunerAudioProcessorEditor::performGlobalUndo()
{
    auto& um = processorRef.getUndoManager();
    if (! um.canUndo())
        return;
    applyingUndo_ = true;
    um.undo();
    applyingUndo_ = false;
    // Re-sync the live baseline and every dependent control to the restored
    // state. refreshLabels() also re-disables Harmony sub-toggles etc.
    undoLiveSnapshot_ = processorRef.getParameters().copyState();
    refreshLabels();
}

void OpenVoxTunerAudioProcessorEditor::performGlobalRedo()
{
    auto& um = processorRef.getUndoManager();
    if (! um.canRedo())
        return;
    applyingUndo_ = true;
    um.redo();
    applyingUndo_ = false;
    undoLiveSnapshot_ = processorRef.getParameters().copyState();
    refreshLabels();
}

void OpenVoxTunerAudioProcessorEditor::commitUndoGesture (const juce::ValueTree& before)
{
    // `before` is the pre-change snapshot; `after` is the current live state.
    // Skip programmatic refreshes that round-trip identical values.
    const juce::ValueTree after = processorRef.getParameters().copyState();
    if (before.isValid() && after.isValid() && ! before.isEquivalentTo (after))
        processorRef.pushUndoAction (before, after);
    undoLiveSnapshot_ = after;
}

void OpenVoxTunerAudioProcessorEditor::registerUndoGestureListeners()
{
    // Register the editor as listener on every control that mutates the
    // plugin state, so a single set of handlers records ALL gestures.
    // Sliders: snapshot at drag-start, commit at drag-end.
    speedSlider.addListener (this);
    amountSlider.addListener (this);
    formantSlider.addListener (this);
    morphSlider.addListener (this);
    leadRoomSlider.addListener (this);
    leadHallSlider.addListener (this);
    harmonyRoomSlider.addListener (this);
    harmonyHallSlider.addListener (this);
    duckDepthSlider.addListener (this);
    outputTrimSlider.addListener (this);
    // 2026-07-24 (Deprecation): FlexTune listener disabled (UI hidden).
    // flexTuneSlider.addListener (this);
    humanizeSlider.addListener (this);
    vibratoPreserveSlider.addListener (this);
    // 2026-07-24 (Deprecation): Attack-Aware listeners disabled.
    // attackReleaseSlider.addListener (this);
    harmonyGainSlider.addListener (this);
    harmonyBlendSlider.addListener (this);
    harmonyAttackSlider.addListener (this);
    harmonyToneColorSlider.addListener (this);
    doubleAmountSlider.addListener (this);
    noiseGateThresholdSlider.addListener (this);

    // Toggles + combo boxes: commit on change using the last pre-change
    // snapshot (the live baseline captured by the timer).
    formantEnableButton.addListener (this);
    leadReverbEnableButton.addListener (this);
    harmonyReverbEnableButton.addListener (this);
    limiterEnableButton.addListener (this);
    doubleEnableButton.addListener (this);
    noiseGateEnableButton.addListener (this);
    // 2026-07-24 (Deprecation): Attack-Aware button listener disabled.
    // attackAwareButton.addListener (this);
    // correctionModeButton is NOT registered here: the mode switch wraps the
    // correction_mode + hard_tune pair in one pushUndoAroundCall transaction.
    harmonyEnableButton.addListener (this);
    harmonyFollowLeadButton.addListener (this);
    harmonyGainMatchButton.addListener (this);
    useVoiceButton.addListener (this);
    keyDetectPowerButton.addListener (this);

    keyBox.addListener (this);
    scaleBox.addListener (this);
    latencyModeBox.addListener (this);
    detectorBox.addListener (this);
    keySourceBox.addListener (this);
    companionGroupBox.addListener (this);
    harmonyTypeBox.addListener (this);
    shiftedVoicesBox.addListener (this);
    harmonyToneBox.addListener (this);
    measuresComboBox.addListener (this);

    // The 12 custom-scale toggle buttons (part of the scale keyboard).
    for (int i = 0; i < 12; ++i)
        scaleKeyboard.getButton (i).addListener (this);
}

// juce::Slider::Listener
void OpenVoxTunerAudioProcessorEditor::sliderValueChanged (juce::Slider*)
{
    // Deliberately empty: the live snapshot is refreshed by the 30 fps timer
    // and committed on sliderDragEnded, so per-tick value changes need no
    // action here.
}

void OpenVoxTunerAudioProcessorEditor::sliderDragStarted (juce::Slider*)
{
    undoGestureActive_ = true;
    undoBeforeDrag_ = processorRef.getParameters().copyState();
}

void OpenVoxTunerAudioProcessorEditor::sliderDragEnded (juce::Slider*)
{
    if (! undoGestureActive_)
        return;
    undoGestureActive_ = false;
    commitUndoGesture (undoBeforeDrag_);
}

// juce::Button::Listener
void OpenVoxTunerAudioProcessorEditor::buttonClicked (juce::Button*)
{
    commitUndoGesture (undoLiveSnapshot_);
}

// juce::ComboBox::Listener
void OpenVoxTunerAudioProcessorEditor::comboBoxChanged (juce::ComboBox*)
{
    commitUndoGesture (undoLiveSnapshot_);
}

// Wrap a non-gesture state change (preset load, programmatic apply) in a
// single undo transaction: snapshot before, run the change, snapshot after.
void OpenVoxTunerAudioProcessorEditor::pushUndoAroundCall (std::function<void()> fn)
{
    if (applyingUndo_)
        return; // never record while replaying undo/redo
    const juce::ValueTree before = processorRef.getParameters().copyState();
    fn();
    commitUndoGesture (before);
}

