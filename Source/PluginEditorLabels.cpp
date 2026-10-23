// PluginEditorLabels.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// refreshLabels(): pushes every translated string at the widgets when the
// interface language changes. Split out of PluginEditor.cpp (step 2 of the
// source split) verbatim.
#include "PluginEditor.h"

#include "ui/CorrectionModeSwitch.h"
#include "ui/OVTLanguages.h"

void OpenVoxTunerAudioProcessorEditor::refreshLabels()
{
    // Update registered labels
    for (auto& tl : translatableLabels)
    {
        if (tl.label != nullptr)
            tl.label->setText (ovt::tr (tl.key), juce::dontSendNotification);
    }

    // The left title spines obey the same rule as the labels: a longer translated name pays in length along the
    // spine, never in width, so switching language re-states every band text here.
    for (auto& tb : translatableBands)
    {
        if (tb.band != nullptr)
            tb.band->setTitle (ovt::tr (tb.key));
    }

    // Update scale combo box items
    {
        const char* scaleKeys[] = {
            ovt::Keys::kScaleChromatic, ovt::Keys::kScaleMajor,
            ovt::Keys::kScaleMelodicMinor, ovt::Keys::kScaleHarmonicMinor,
            ovt::Keys::kScaleNaturalMinor, ovt::Keys::kScaleMajorPentatonic,
            ovt::Keys::kScaleMinorPentatonic, ovt::Keys::kScaleBlues,
            ovt::Keys::kScaleDorian, ovt::Keys::kScalePhrygian,
            ovt::Keys::kScaleLydian, ovt::Keys::kScaleMixolydian,
            ovt::Keys::kScaleLocrian, ovt::Keys::kScaleCustom
        };
        const int currentScale = scaleBox.getSelectedItemIndex();
        scaleBox.clear (juce::dontSendNotification);
        for (int i = 0; i < 14; ++i)
            scaleBox.addItem (ovt::tr (scaleKeys[i]), i + 1);
        if (currentScale >= 0)
            scaleBox.setSelectedItemIndex (currentScale, juce::dontSendNotification);
    }

    // Update harmony combo box items (all 22)
    {
        const char* const* harmonyKeys = ovt::harmonyTypeKeys();
        const int currentHarmony = harmonyTypeBox.getSelectedItemIndex();
        harmonyTypeBox.clear (juce::dontSendNotification);
        for (int i = 0; i < ovt::kHarmonyTypeCount; ++i)
            harmonyTypeBox.addItem (ovt::tr (harmonyKeys[i]), i + 1);
        if (currentHarmony >= 0)
            harmonyTypeBox.setSelectedItemIndex (juce::jmin (currentHarmony, ovt::kHarmonyTypeCount - 1), juce::dontSendNotification);
    }

    // Update Voice Type combo box items (6). Labels already include the note
    // range suffix (e.g. "Bass (E2-E4)") so the user sees the range directly
    // in the menu without needing a tooltip.
    {
        const char* voiceTypeKeys[] = {
            ovt::Keys::kVoiceTypeUniversal, ovt::Keys::kVoiceTypeBass,
            ovt::Keys::kVoiceTypeBaritone, ovt::Keys::kVoiceTypeTenor,
            ovt::Keys::kVoiceTypeAlto, ovt::Keys::kVoiceTypeSoprano
        };
        const int currentVoiceType = voiceTypeBox.getSelectedItemIndex();
        voiceTypeBox.clear (juce::dontSendNotification);
        for (int i = 0; i < 6; ++i)
            voiceTypeBox.addItem (ovt::tr (voiceTypeKeys[i]), i + 1);
        if (currentVoiceType >= 0)
            voiceTypeBox.setSelectedItemIndex (juce::jmin (currentVoiceType, 5), juce::dontSendNotification);
    }

    // Update "Use Voice" switch caption
    useVoiceLabel.setText (ovt::tr (ovt::Keys::kLabelUseVoice), juce::dontSendNotification);

    // Update button text
    updateButton.setButtonText (ovt::tr(ovt::Keys::kLabelUpdates));
    harmonyEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyBtn));
    harmonyFollowLeadButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyFollow));
    harmonyGainMatchButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyGainMatch));
    formantEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelFormantBtn));
    leadReverbEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelReverbBtn));
    harmonyReverbEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelPadReverbBtn));
    limiterEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelLimiterBtn));
    correctionModeButton.setButtonText (correctionModeButton.getToggleState() ? ovt::tr(ovt::Keys::kLabelTransparentBtn) : ovt::tr(ovt::Keys::kLabelModernBtn));
    harmonyToneColorLabel.setText (ovt::tr(ovt::Keys::kLabelTone), juce::dontSendNotification);
    noiseGateEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelNoiseGate));
    doubleEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelDouble));
    deessEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelDeesser));
    saturationEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelSaturation));
    delayEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelDelay));
    noiseGateThresholdLabel.setText (ovt::tr(ovt::Keys::kLabelThreshold), juce::dontSendNotification);

    // Refresh all translatable tooltips
    correctionFrame.disclosure().setTooltip (ovt::tr (ovt::Keys::kTooltipAdvanced));
    voiceTypeBox.setTooltip (ovt::tr (ovt::Keys::kTooltipVoiceType));
    voiceTypeLabel.setTooltip (ovt::tr (ovt::Keys::kTooltipVoiceType));
    formantStrategyBox.setTooltip (ovt::tr (ovt::Keys::kTooltipFormantStrategy));
    formantStrategyLabel.setTooltip (ovt::tr (ovt::Keys::kLabelFormantStrategy));
    harmonyFollowLeadButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyFollow));
    harmonyGainMatchButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyGainMatch));
    updateButton.setTooltip (ovt::tr(ovt::Keys::kTooltipCheckUpdates));
    menuButton.setTooltip (ovt::tr(ovt::Keys::kTooltipMenuOptions));
    bypassButton.setTooltip (ovt::tr (ovt::Keys::kTooltipBypassIcon));
    midiOutButton.setTooltip (ovt::tr(ovt::Keys::kTooltipMidiOutIcon));
    correctionModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    harmonyEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyEn));
    leadReverbEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipLeadReverbEn));
    harmonyReverbEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyReverbEn));
    formantEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipFormant));
    formantSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipFormant));
    leadRoomSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipLeadRoom));
    leadHallSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipLeadHall));
    harmonyRoomSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyRoom));
    harmonyHallSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyHall));
    limiterEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipLimiterEn));
    outputTrimSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipOutputTrim));
    duckDepthSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipReverbDucking));

    outputMeter.setCaptions (ovt::tr (ovt::Keys::kLabelMeterGr), ovt::tr (ovt::Keys::kLabelMeterTrim));
    outputMeter.setTooltip (ovt::tr (ovt::Keys::kTooltipOutputMeter));

    noiseGateEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipNoiseGate));
    noiseGateThresholdSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipThreshold));
    flexTuneSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipFlexTune));
    humanizeSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHumanize));
    vibratoPreserveSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipVibrato));
    attackReleaseSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipAttackRelease));
    attackAwareButton.setTooltip (ovt::tr (ovt::Keys::kTooltipAttack));
    attackAwareButton.setButtonText (ovt::tr (ovt::Keys::kLabelAttackBtn));
    keySourceBox.setTooltip (ovt::tr (ovt::Keys::kTooltipKeySource));
    companionGroupBox.setTooltip (ovt::tr (ovt::Keys::kTooltipCompanionGroup));
    speedSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSpeed));
    amountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipAmount));
    harmonyGainSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipVolume));
    harmonyBlendSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipBlend));
    harmonyAttackSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyAttack));
    harmonyFormantSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyFormant));
    doubleAmountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDoubleAmount));
    doubleEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipDoubleEn));
    deessEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipDeesserEnable));
    deessAmountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDeesserAmount));
    saturationEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSaturationEnable));
    saturationDriveSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSaturationDrive));
    saturationToneSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSaturationWarmth));
    delayEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayEn));
    delayMixSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayMix));
    delayDivisionKnob.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayDivision));
    delayFeedbackSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayFeedback));
    harmonyToneColorSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipToneColor));
    buttonA.setTooltip (ovt::tr(ovt::Keys::kTooltipAbSlotA));
    buttonB.setTooltip (ovt::tr(ovt::Keys::kTooltipAbSlotB));
    morphSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphDrag));
    morphSliderLabel.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphLabel));
    morphSliderLabel.setText (ovt::tr(ovt::Keys::kLabelMorph), juce::dontSendNotification);

    // Refresh drawable button tooltips (already use ovt::tr but need refresh)
    optionsButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCurveOptions));
    snapButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSnapToScale));
    snapGridButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSnapToGrid));
    stepModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipStepMode));
    zoomInButton.setTooltip (ovt::tr (ovt::Keys::kTooltipZoomIn));
    zoomOutButton.setTooltip (ovt::tr (ovt::Keys::kTooltipZoomOut));
    scrollUpButton.setTooltip (ovt::tr (ovt::Keys::kTooltipScrollUp));
    scrollDownButton.setTooltip (ovt::tr (ovt::Keys::kTooltipScrollDown));
    resetViewButton.setTooltip (ovt::tr (ovt::Keys::kTooltipResetView));
    rewindButton.setTooltip (ovt::tr (ovt::Keys::kTooltipRewind));

    // Refresh tooltips for components that were created in the constructor
    // and were NOT being updated on language change. Without these calls
    // the user would see the tooltip in the language that was active at
    // first launch, even after switching to a different one via
    // wrench/Interface/Language. Affects: Upward Compress (enable + amount
    // knob), Modern/Transparent pill switch, and the plugin preset
    // prev/next/save + gallery buttons.
    upwardCompEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipUpwardCompEn));
    upwardCompAmountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipUpwardCompAmount));
    correctionModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    if (modeSwitch != nullptr)
        modeSwitch->setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    presetPrevButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPluginPresetPrev));
    presetNextButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPluginPresetNext));
    presetSaveButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPluginPresetSave));
    presetGalleryButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPresetGallery));
    undoButton.setTooltip (ovt::tr (ovt::Keys::kTooltipUndo));
    redoButton.setTooltip (ovt::tr (ovt::Keys::kTooltipRedo));

    // No activity-dot sentence here any more: the dot says by itself what it means (it lights
    // with the signal the component works on), so a tooltip only has to explain the CONTROL.

    // "Measures" control label follows the active language.
    measuresLabel.setText (ovt::tr (ovt::Keys::kLabelMeasures), juce::dontSendNotification);

    // Update tab names
    tabbedComponent.setTabName (0, ovt::tr(ovt::Keys::kTabLive));
    tabbedComponent.setTabName (1, ovt::tr(ovt::Keys::kTabCurveEditor));

    // Refresh curve editor embedded labels
    if (curveEditor != nullptr)
        curveEditor->refreshTranslations();

    // Re-run the layout so width-dependent labels (e.g. the "Measures" control)
    // are re-sized to the new language text instead of staying clipped at the
    // previous (shorter/longer) width.
    resized();

    repaint();
}

