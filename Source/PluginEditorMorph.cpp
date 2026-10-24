// PluginEditorMorph.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Preset morphing: the morph slider, its undo state and its context menu.
// Split out of PluginEditor.cpp (step 2 of the source split) verbatim.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/PluginEditorInternal.h"

// === Preset Morphing ===

void OpenVoxTunerAudioProcessorEditor::resetMorph()
{
    if (morphSource != nullptr)
    {
        ovtdsp::applyInterpolatedState (processorRef.getParameters(),
                                        *morphSource, *morphSource, 0.0f);
        auto resetCurve = morphSource->curve;
        if (curveEditor != nullptr)
        {
            curveEditor->setCurve (resetCurve);
            curveEditor->setGhostCurve (nullptr);
        }
    }
    processorRef.setMorphAmount (0.0f);
    lastMorphValue = 0.0f;
    morphSource.reset();
    morphTarget.reset();
    morphUndoState.reset();
    lastMorphIntendedValues.clear();
}

void OpenVoxTunerAudioProcessorEditor::onMorphSliderChanged (float value)
{
    // Ignore morph application during slot switching - the caller manages state.
    if (switchingSlot)
    {
        lastMorphValue = value;
        return;
    }

    // Auto-capture source on first movement if not set
    if (morphSource == nullptr)
    {
        // Fresh morph: forget any previous external-automation exclusions so
        // the new crossfade starts with a clean baseline.
        lastMorphIntendedValues.clear();
        morphSource = std::make_unique<ovtdsp::MorphState> (
            ovtdsp::captureState (processorRef.getParameters(), processorRef.getPitchCurve(), "Current"));
        morphSourceName = "Current";
    }
    // Auto-capture target from Slot B (or Slot A if B is empty) on first movement
    if (morphTarget == nullptr)
    {
        if (slotB.hasData && slotB.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            morphTargetName = "Slot B";
        }
        else if (slotA.hasData && slotA.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
            morphTargetName = "Slot A";
        }
        else
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (
                ovtdsp::captureState (processorRef.getParameters(), processorRef.getPitchCurve(), "Target"));
            morphTargetName = "Target";
        }
    }

    // Capture pre-morph state on first movement (for undo)
    if (lastMorphValue < 0.01f && value > 0.01f && morphUndoState == nullptr)
    {
        morphUndoState = std::make_unique<ovtdsp::MorphState> (
            ovtdsp::captureState (processorRef.getParameters(), processorRef.getPitchCurve(), "Pre-morph"));
    }

    // Detect parameters currently driven by external automation (DAW lanes or
    // UI) so the morph crossfade does not overwrite them. A parameter is
    // considered externally driven when its live value differs from the value
    // the morph last applied to it.
    juce::AudioProcessorValueTreeState& params = processorRef.getParameters();
    juce::StringArray excluded;
    {
        const juce::StringArray ids = ovtdsp::getMorphParameterIds();
        for (const auto& id : ids)
        {
            auto* p = params.getParameter (id);
            if (p == nullptr)
                continue;
            auto it = lastMorphIntendedValues.find (id);
            if (it != lastMorphIntendedValues.end()
                && std::abs (p->getValue() - it->second) > 1.0e-4f)
                excluded.add (id);
        }
    }

    // Apply interpolated state (skipping externally-driven parameters)
    ovtdsp::applyInterpolatedState (params,
                                    *morphSource, *morphTarget, value, &excluded);

    // Record the values the morph just applied so the next frame can detect
    // whether a parameter was changed externally and must remain excluded.
    {
        const juce::StringArray ids = ovtdsp::getMorphParameterIds();
        for (const auto& id : ids)
        {
            if (excluded.contains (id))
                continue;
            auto* p = params.getParameter (id);
            if (p != nullptr)
                lastMorphIntendedValues[id] = p->getValue();
        }
    }

    // Morph the parameters only: the pitch curve is NOT crossfaded (a curve
    // blend would resample and add spurious intermediate points). The displayed
    // curve snaps to the nearest slot's curve as the slider moves, so no extra
    // points are ever introduced by the morph.
    if (curveEditor != nullptr)
    {
        const ovtdsp::PitchCurve& nearestCurve = (value < 0.5f) ? morphSource->curve : morphTarget->curve;
        curveEditor->setCurve (nearestCurve);
        // Show ghost curve (target) when morphing, clear when at source
        curveEditor->setGhostCurve (value > 0.01f ? &morphTarget->curve : nullptr);
    }

    lastMorphValue = value;
}

void OpenVoxTunerAudioProcessorEditor::undoMorph()
{
    if (morphUndoState == nullptr) return;

    // Restore the pre-morph state
    ovtdsp::applyInterpolatedState (processorRef.getParameters(),
                                    *morphUndoState, *morphUndoState, 0.0f);
    if (curveEditor != nullptr)
    {
        curveEditor->setCurve (morphUndoState->curve);
        curveEditor->setGhostCurve (nullptr);
    }
    processorRef.setMorphAmount (0.0f);
    lastMorphValue = 0.0f;
    morphUndoState.reset();
}

void OpenVoxTunerAudioProcessorEditor::showMorphContextMenu()
{
    juce::PopupMenu menu;

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphSetSource), [this] {
        morphSource = std::make_unique<ovtdsp::MorphState> (
            ovtdsp::captureState (processorRef.getParameters(),
                                 processorRef.getPitchCurve(), "Current"));
        morphSourceName = "Current";
        processorRef.setMorphAmount (0.0f);
        lastMorphValue = 0.0f;
    });

    menu.addSeparator();

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphSetTargetA), [this] {
        if (slotA.hasData && slotA.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
            morphTargetName = "Slot A";
            processorRef.setMorphAmount (0.0f);
            lastMorphValue = 0.0f;
        }
    });

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphSetTargetB), [this] {
        if (slotB.hasData && slotB.morphState != nullptr)
        {
            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            morphTargetName = "Slot B";
            processorRef.setMorphAmount (0.0f);
            lastMorphValue = 0.0f;
        }
    });

    menu.addSeparator();

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphAtoB), [this] {
        if (slotA.hasData && slotA.morphState != nullptr && slotB.hasData && slotB.morphState != nullptr)
        {
            morphSource = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
            morphSourceName = "Slot A";

            morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            morphTargetName = "Slot B";

            // Fresh morph baseline: drop the external-automation exclusion map so
            // the new crossfade is not poisoned by stale intended values.
            lastMorphIntendedValues.clear();
            processorRef.setMorphAmount (0.0f);
            lastMorphValue = 0.0f;
        }
    });

    menu.addSeparator();

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphUndo), morphUndoState != nullptr, false, [this] {
        undoMorph();
    });

    menu.addItem (ovt::tr(ovt::Keys::kMenuMorphReset), [this] {
        if (morphSource != nullptr)
        {
            ovtdsp::applyInterpolatedState (processorRef.getParameters(),
                                            *morphSource, *morphSource, 0.0f);
            auto resetCurve = morphSource->curve;
            if (curveEditor != nullptr)
            {
                curveEditor->setCurve (resetCurve);
                curveEditor->setGhostCurve (nullptr);
            }
        }
        processorRef.setMorphAmount (0.0f);
        lastMorphValue = 0.0f;
    });

    applyMenuLookAndFeel (menu, customLookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options());
}

