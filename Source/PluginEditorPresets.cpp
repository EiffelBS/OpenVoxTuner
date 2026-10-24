// PluginEditorPresets.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Presets, A/B slots and the factory preset menu: curve presets on disk,
// plugin presets on disk, the A/B morph slots, the preset gallery and the
// MIDI-learn hook that the Advanced menu arms. Split out of
// PluginEditor.cpp (step 2 of the source split) verbatim.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/PluginEditorInternal.h"

#include "ui/OVTLanguages.h"
#include "ui/PresetGallery.h"

#include <map>

namespace
{
    juce::File getUserPresetsDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("OpenVoxTuner")
                       .getChildFile ("Presets");
        dir.createDirectory();
        return dir;
    }

    // Plugin Presets live in a separate "Presets/Plugin" folder so their files
    // never collide with Curve Preset files (which live directly under
    // "Presets"). The two preset kinds are fully orthogonal.
    juce::File getUserPluginPresetsDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("OpenVoxTuner")
                       .getChildFile ("Presets")
                       .getChildFile ("Plugin");
        dir.createDirectory();
        return dir;
    }

    juce::String sanitizePresetFileStem (juce::String name)
    {
        name = name.trim();
        if (name.isEmpty())
            return "Preset";

        juce::String out;
        out.preallocateBytes (name.getNumBytesAsUTF8());

        for (auto c : name)
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                out << juce::String::charToString (c);
            else if (c == ' ' || c == '-' || c == '_')
                out << "_";
        }

        out = out.trimCharactersAtEnd ("_").trimCharactersAtStart ("_");
        if (out.isEmpty())
            out = "Preset";

        return out;
    }
}

void OpenVoxTunerAudioProcessorEditor::applyPresetUiStateFromXml (const juce::XmlElement& xml)
{
    const int keyIdx = xml.getIntAttribute ("key", keyBox.getSelectedItemIndex());
    const int scaleIdx = xml.getIntAttribute ("scale", scaleBox.getSelectedItemIndex());
    const bool snapScale = xml.getBoolAttribute ("snapScale", snapButton.getToggleState());
    const bool snapGrid = xml.getBoolAttribute ("snapGrid", snapGridButton.getToggleState());
    const bool stepMode = xml.getBoolAttribute ("stepMode", stepModeButton.getToggleState());

    keyBox.setSelectedItemIndex (juce::jlimit (0, 11, keyIdx), juce::sendNotificationSync);
    scaleBox.setSelectedItemIndex (juce::jlimit (0, 13, scaleIdx), juce::sendNotificationSync);

    snapButton.setToggleState (snapScale, juce::dontSendNotification);
    snapGridButton.setToggleState (snapGrid, juce::dontSendNotification);
    stepModeButton.setToggleState (stepMode, juce::dontSendNotification);

    if (curveEditor != nullptr)
    {
        curveEditor->setSnapEnabled (snapScale);
        curveEditor->setSnapToGridEnabled (snapGrid);
        curveEditor->setStepModeEnabled (stepMode);
    }
}

void OpenVoxTunerAudioProcessorEditor::loadCustomPresetFromFile (const juce::File& file)
{
    if (curveEditor == nullptr) return;
    if (! file.existsAsFile()) return;

    pushUndoAroundCall ([this, &file] {
        resetMorph(); // cancel any active morph when loading a preset

        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument (file).getDocumentElement());
        if (xml == nullptr) return;

        const juce::XmlElement* root = xml.get();
        const juce::XmlElement* curveXml = nullptr;

        if (xml->hasTagName ("OVT_PRESET"))
            curveXml = xml->getChildByName ("PITCH_CURVE");
        else if (xml->hasTagName ("PITCH_CURVE"))
            curveXml = xml.get();

        if (curveXml == nullptr) return;

        ovtdsp::PitchCurve newCurve;
        newCurve.fromXml (*curveXml);
        curveEditor->setCurve (newCurve);
        syncEditButtons();

        if (root->hasTagName ("OVT_PRESET"))
            applyPresetUiStateFromXml (*root);
        else
        {
            // Reflect the effective Step Mode (forced ON in Piano Roll mode) rather
            // than the raw preset value.
            stepModeButton.setToggleState (curveEditor->isStepModeEnabled(), juce::dontSendNotification);
            applyPresetUiStateFromXml (*root);
        }
    });
}

void OpenVoxTunerAudioProcessorEditor::promptSaveCustomPreset()
{
    if (curveEditor == nullptr) return;

    auto* w = new juce::AlertWindow (ovt::tr(ovt::Keys::kDlgSavePreset),
                                     ovt::tr(ovt::Keys::kDlgSavePresetDesc),
                                     juce::AlertWindow::NoIcon);
    w->addTextEditor ("name", "", "Name:");
    w->addButton (ovt::tr(ovt::Keys::kDlgSave), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ovt::tr(ovt::Keys::kMenuCancel), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        std::unique_ptr<juce::AlertWindow> cleanup (w);
        if (result == 0 || curveEditor == nullptr)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         ovt::tr(ovt::Keys::kDlgInvalidName),
                                                         ovt::tr(ovt::Keys::kDlgEmptyName),
                                                         this);
            return;
        }

        const auto dir = getUserPresetsDirectory();
        const auto fileStem = sanitizePresetFileStem (name);
        const auto file = dir.getChildFile (fileStem + ".xml");

        if (file.existsAsFile())
        {
            auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                                   ovt::tr(ovt::Keys::kDlgOverwrite),
                                                                   ovt::tr(ovt::Keys::kDlgOverwriteDesc),
                                                                   ovt::tr(ovt::Keys::kMenuCancel), ovt::tr(ovt::Keys::kDlgOverwriteBtn),
                                                                   this);

            juce::NativeMessageBox::showAsync (opts, [this, file, name] (int result)
            {
                if (result != 2)
                    return;
                writeCustomPresetFile (name, file);
    });
            return;
        }

        writeCustomPresetFile (name, file);
    }), true);
}

void OpenVoxTunerAudioProcessorEditor::writeCustomPresetFile (const juce::String& name, const juce::File& file)
{
    if (curveEditor == nullptr || name.isEmpty() || file == juce::File())
        return;

    auto root = std::make_unique<juce::XmlElement> ("OVT_PRESET");
    root->setAttribute ("name", name);
    root->setAttribute ("key", keyBox.getSelectedItemIndex());
    root->setAttribute ("scale", scaleBox.getSelectedItemIndex());
    root->setAttribute ("snapScale", snapButton.getToggleState() ? 1 : 0);
    root->setAttribute ("snapGrid", snapGridButton.getToggleState() ? 1 : 0);
    root->setAttribute ("stepMode", stepModeButton.getToggleState() ? 1 : 0);

    auto curveXml = curveEditor->getCurve().toXml();
    if (curveXml != nullptr)
        root->addChildElement (curveXml.release());

    const bool ok = file.replaceWithText (root->toString());
    if (ok)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                     ovt::tr(ovt::Keys::kDlgPresetSaved),
                                                     ovt::tr(ovt::Keys::kDlgPresetSavedDesc) + name,
                                                     this);
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     ovt::tr(ovt::Keys::kDlgSaveFailed),
                                                     ovt::tr(ovt::Keys::kDlgSaveFailedDesc),
                                                     this);
    }
}

void OpenVoxTunerAudioProcessorEditor::deleteCustomPresetFile (const juce::File& file, std::function<void (bool)> onDone,
                                                              juce::Component* parentComp)
{
    if (! file.existsAsFile())
    {
        if (onDone) onDone (false);
        return;
    }

    // When called from the preset gallery (a separate DocumentWindow above the
    // editor), parent the dialog to the gallery so it appears in front of it
    // instead of behind. Defaults to the editor when no parent is supplied.
    juce::Component* dialogParent = (parentComp != nullptr) ? parentComp : this;

    auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                           ovt::tr(ovt::Keys::kDlgDeletePreset),
                                                           ovt::tr(ovt::Keys::kDlgDeletePresetDesc) + file.getFileNameWithoutExtension(),
                                                           ovt::tr(ovt::Keys::kMenuCancel), ovt::tr(ovt::Keys::kDlgDelete),
                                                           dialogParent);

    juce::NativeMessageBox::showAsync (opts, [this, file, onDone, dialogParent] (int result)
    {
        // Debug: display the result value
        // Debug: show the actual result value
        if (result != 1) // Delete button id is 1 in makeOptionsYesNo
        {
            if (onDone) onDone (false);
            return;
        }

        // Ensure file is writable before attempting deletion
        file.setReadOnly(false);
        const bool deleted = file.deleteFile();
        if (!deleted || file.existsAsFile())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                             ovt::tr(ovt::Keys::kDlgDeleteFailed),
                                                             ovt::tr(ovt::Keys::kDlgDeleteFailedDesc),
                                                             dialogParent);
            if (onDone) onDone (false);
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                             ovt::tr(ovt::Keys::kDlgPresetDeleted),
                                                             ovt::tr(ovt::Keys::kDlgPresetDeletedDesc) + file.getFileNameWithoutExtension(),
                                                             dialogParent);
            // Refresh the gallery (if open) or fall back to the preset menu.
            if (onDone)
                onDone (true);
            else
                showPresetsMenu();
        }
    });
}

// === A/B Comparison ===

/**
 * Saves the current plugin state into an A/B slot using a direct MorphState
 * snapshot. Persists the MorphState to the processor for project reload.
 */
void OpenVoxTunerAudioProcessorEditor::saveSlot (ABState& slot, int slotIndex)
{
    // Use a safe curve reference - pitchCurve may be null during init.
    const ovtdsp::PitchCurve emptyCurve;
    const auto& curve = processorRef.hasPitchCurve() ? processorRef.getPitchCurve() : emptyCurve;

    slot.morphState = std::make_unique<ovtdsp::MorphState> (
        ovtdsp::captureState (processorRef.getParameters(), curve,
                             slotIndex == 0 ? "Slot A" : "Slot B"));
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
    if (slotIndex == 0)
        juce::Logger::writeToLog (
            "[DIAG] saveSlot(A): type=" + juce::String (slot.morphState->harmonyType)
            + " vibrato=" + juce::String (slot.morphState->vibratoPreserve, 4)
            + " humanize=" + juce::String (slot.morphState->humanize, 4)
            + " formant=" + juce::String (slot.morphState->harmonyFormant, 4));
#endif
    slot.hasData = true;
    slot.name = "filled";

    processorRef.setAbSlotMorphState (slotIndex, *slot.morphState);
}

/**
 * Loads a saved A/B slot by setting UI sliders directly, which triggers
 * the SliderAttachment to update the processor parameter.
 * This "slider-first" approach works reliably because SliderAttachment
 * listens to slider changes (Slider::Listener) and syncs them to params.
 * For discrete/boolean params without sliders, we use setValueNotifyingHost.
 */
void OpenVoxTunerAudioProcessorEditor::loadSlot (const ABState& slot)
{
    if (! slot.hasData || slot.morphState == nullptr) return;

    switchingSlot = true;

    auto& params = processorRef.getParameters();
    const auto& saved = *slot.morphState;

    // Set continuous parameter sliders directly. The SliderAttachment will
    // see the value change and call param->setValueNotifyingHost to sync.
    auto setSlider = [&params] (const juce::String& id, float normValue, juce::Slider& slider)
    {
        if (auto* p = params.getParameter (id))
            slider.setValue (p->convertFrom0to1 (normValue), juce::sendNotificationSync);
    };
    setSlider ("speed",              saved.speed,              speedSlider);
    setSlider ("amount",             saved.amount,             amountSlider);
    setSlider ("formant",            saved.formant,            formantSlider);
    setSlider ("harmony_gain",       saved.harmonyGain,        harmonyGainSlider);
    setSlider ("harmony_blend",      saved.harmonyBlend,       harmonyBlendSlider);
    setSlider ("harmony_attack",     saved.harmonyAttack,      harmonyAttackSlider);
    setSlider ("harmony_formant",    saved.harmonyFormant,     harmonyFormantSlider);
    setSlider ("harmony_tone_color", saved.harmonyToneColor,   harmonyToneColorSlider);
    setSlider ("double_amount",      saved.doubleAmount,       doubleAmountSlider);
    setSlider ("lead_room_send",     saved.leadRoomSend,       leadRoomSlider);
    setSlider ("lead_hall_send",     saved.leadHallSend,       leadHallSlider);
    setSlider ("harmony_room_send",  saved.harmonyRoomSend,    harmonyRoomSlider);
    setSlider ("harmony_hall_send",  saved.harmonyHallSend,    harmonyHallSlider);
    setSlider ("reverb_duck_depth",  saved.reverbDuckDepth,    duckDepthSlider);
    setSlider ("output_trim",        saved.outputTrim,         outputTrimSlider);
    setSlider ("noise_gate_threshold", saved.noiseGateThreshold, noiseGateThresholdSlider);
    setSlider ("upward_comp_amount",   saved.upwardCompAmount,   upwardCompAmountSlider);
    setSlider ("deess_amount",       saved.deessAmount,      deessAmountSlider);
    setSlider ("saturation_drive",   saved.saturationDrive,  saturationDriveSlider);
    setSlider ("saturation_tone",    saved.saturationTone,   saturationToneSlider);
    setSlider ("delay_mix",          saved.delayMix,         delayMixSlider);
    setSlider ("delay_feedback",     saved.delayFeedback,    delayFeedbackSlider);
    setSlider ("flex_tune",          saved.flexTune,           flexTuneSlider);
    setSlider ("humanize",           saved.humanize,           humanizeSlider);
    setSlider ("vibrato_preserve",    saved.vibratoPreserve,    vibratoPreserveSlider);
    setSlider ("attack_release",      saved.attackRelease,      attackReleaseSlider);
    attackAwareButton.setToggleState (saved.attackAware > 0.5f, juce::dontSendNotification);

    // Backward-compatibility: old Vocoder-like (index 5) -> Organ (index 1)
    int curTone = saved.harmonyTone;
    if (curTone == 5) curTone = 1; // Vocoder-like was the last item (index 5) in the old 6-item list
    // Note: old indices 1..4 were removed (Bright, SynthLead, Strings, Guitar) -
    // any project using those will silently map to Choir (0), which is the default.

    // Discrete parameters: use setValueNotifyingHost (ComboBox attachments sync)
    auto setChoice = [&params] (const juce::String& id, float normValue)
    {
        if (auto* p = params.getParameter (id))
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normValue));
    };
    setChoice ("key",                      (float) saved.key / 11.0f);
    setChoice ("scale",                    (float) saved.scale / 13.0f);
    setChoice ("harmony_type",             (float) saved.harmonyType / 21.0f);
    setChoice ("harmony_tone",             (float) curTone / 1.0f);
    setChoice ("harmony_shifted_voices",   (float) (saved.harmonyShiftedVoices - 1) / 3.0f);
    // Legacy 12 ms slot -> Fast: that mode is no longer in the menu.
    const int latencyModeChoice = (saved.latencyMode == 1) ? 0 : saved.latencyMode;
    setChoice ("latency_mode",             (float) latencyModeChoice / 3.0f);
    setChoice ("editor_measures",          (float) (saved.editorMeasures - 1) / 31.0f);
    setChoice ("voice_type",               (float) saved.voiceType / 5.0f);
    // Nine divisions, so the choice index scales by 8 on the normalised axis.
    setChoice ("delay_division",           (float) saved.delayDivision / 8.0f);
    // Continuous-style morph params (key_source / companion_group are
    // normalised 0..1; key_detect steps). They were captured by captureState
    // but never re-applied here, so a slot switch left them at the previous
    // slot's values.
    setChoice ("key_source",               saved.keySource);
    setChoice ("companion_group",          saved.companionGroup);
    setChoice ("key_detect",               saved.keyDetect);

    // Boolean parameters: use setValueNotifyingHost (Button attachments sync)
    setChoice ("formant_enable",    saved.formantEnable ? 1.0f : 0.0f);
    setChoice ("bypass",            saved.bypass ? 1.0f : 0.0f);
    setChoice ("harmony_enable",    saved.harmonyEnable ? 1.0f : 0.0f);
    setChoice ("harmony_use_voice", saved.harmonyUseVoice ? 1.0f : 0.0f);
    setChoice ("harmony_follow_lead", saved.harmonyFollowLead ? 1.0f : 0.0f);
    setChoice ("lead_reverb_enable",    saved.leadReverbEnable ? 1.0f : 0.0f);
    setChoice ("harmony_reverb_enable", saved.harmonyReverbEnable ? 1.0f : 0.0f);
    setChoice ("limiter_enable",    saved.limiterEnable ? 1.0f : 0.0f);
    setChoice ("double_enable",     saved.doubleEnable ? 1.0f : 0.0f);
    setChoice ("noise_gate_enable", saved.noiseGateEnable ? 1.0f : 0.0f);
    setChoice ("upward_comp_enable", saved.upwardCompEnable ? 1.0f : 0.0f);
    setChoice ("deess_enable",       saved.deessEnable ? 1.0f : 0.0f);
    setChoice ("saturation_enable",  saved.saturationEnable ? 1.0f : 0.0f);
    setChoice ("delay_enable",       saved.delayEnable ? 1.0f : 0.0f);
    setChoice ("correction_mode",   saved.correctionMode ? 1.0f : 0.0f);
    setChoice ("hard_tune",         saved.hardTune ? 1.0f : 0.0f);

    // Restore the pitch curve
    if (curveEditor != nullptr)
        curveEditor->setCurve (saved.curve);

    switchingSlot = false;
}

/** Update A/B button visual states and morph slider visibility. */
void OpenVoxTunerAudioProcessorEditor::updateABButtonStates()
{
    buttonA.hasValidData = true; // A is always valid (default state)
    buttonB.hasValidData = slotB.hasData;
    buttonA.isActive = isSlotAActive;
    buttonB.isActive = ! isSlotAActive;
    buttonA.repaint();
    buttonB.repaint();

    const bool bothFilled = slotA.hasData && slotB.hasData;
    morphSlider.setVisible (bothFilled);
    morphSliderLabel.setVisible (bothFilled);
    if (! bothFilled && morphSource != nullptr)
    {
        processorRef.setMorphAmount (0.0f);
        lastMorphValue = 0.0f;
        morphSource.reset();
        morphTarget.reset();
        morphUndoState.reset();
    }
}

// === Plugin Presets (separate from Curve Presets) ===

namespace
{
    // Ordered factory plugin-preset names. "Default" is first and contains
    // the plugin's factory-default parameter values.
    const juce::StringArray& getFactoryPluginPresetNames()
    {
        static const juce::StringArray names {
            "Default",
            "Pure Correction",
            "Transparent / Doubling",
            "Studio Stack",
            "Live Scene",
            "Spoken Voice / Podcast",
            "Creative Robot",
            "Hard Tune (Cher / T-Pain)",
            "Broadcast Voice",
            "Breathy Vocals",
            "Lead Vocal"
        };
        return names;
    }

    // Returns the parameter overrides (id -> value) for a factory preset, or
    // nullptr for "Default" (which uses the factory defaults verbatim).
    const std::map<juce::String, float>* getFactoryPluginPresetOverrides (const juce::String& name)
    {
        static const std::map<juce::String, float> correctionPure = {
            { "amount", 1.0f }, { "flex_tune", 0.0f }, { "humanize", 0.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", false },
            { "vibrato_preserve", 0.0f }, { "attack_aware", 0.0f },
            { "lead_reverb_enable", 0.0f }, { "harmony_reverb_enable", 0.0f }, { "noise_gate_enable", 0.0f },
            // Nothing dynamic on the lead: pitch correction only, so the de-esser
            // stays off and the sibilance keeps its natural level. The saturation stage
            // is named in the same spirit: drive 0 is a transparent bypass, and this
            // preset promises nothing added to the signal at all. The tempo delay rides
            // the same rule: pitch correction only, no copies on the master.
            { "deess_enable", 0.0f }, { "saturation_enable", 0.0f }, { "delay_enable", 0.0f }
        };
        static const std::map<juce::String, float> transparentDub = {
            { "amount", 0.85f }, { "flex_tune", 25.0f }, { "humanize", 15.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", false },
            { "lead_reverb_enable", 0.0f }, { "correction_mode", 0.0f },
            // The preset's name promises a double track: the stereo doubler
            // delivers it on the lead alone, with Harmony off (the doubler is
            // independent of harmony_enable). 35 % = ~3.5 cents detune, ~9 ms
            // Haas offset and a light right-side spread; the power switch must be
            // on for that amount to be heard.
            { "double_enable", 1.0f }, { "double_amount", 35.0f },
            // Transparency means no dynamics: the doubler widens the lead, the
            // sibilance is left exactly as recorded, and the master stage stays off.
            // No tempo copies either - "transparent" promises an echo-free dry signal.
            { "deess_enable", 0.0f }, { "saturation_enable", 0.0f }, { "delay_enable", 0.0f }
        };
        static const std::map<juce::String, float> studioStack = {
            { "amount", 0.9f }, { "flex_tune", 15.0f }, { "humanize", 20.0f },
            { "harmony_enable", 1.0f }, { "harmony_type", 17.0f }, // Unison + Octaves (4 voices)
            { "harmony_gain_match", 1.0f }, { "harmony_follow_lead", 1.0f },
            { "harmony_gain", 0.75f }, { "harmony_blend", 0.5f },
            { "formant_enable", 1.0f }, { "formant_mode", 1.0f },
            // A 4-voice stack multiplies the sibilants: N voices singing the same
            // "s" add up to +6 dB of top end, so a light de-esser keeps the stack
            // bright without the hiss the sum produces. Low intensity, and the band
            // centre follows the Voice Type the user picked, not a preset override.
            { "deess_enable", 1.0f }, { "deess_amount", 0.55f },
            // The harmony voices get the long HALL (and a little ROOM): a 4-voice stack is
            // where a long, wide room pays, because it fattens the voices without washing
            // the lead, which stays dry here (its switch is off, as in every fresh session).
            { "harmony_reverb_enable", 1.0f }, { "harmony_room_send", 0.10f }, { "harmony_hall_send", 0.35f },
            // Light drive on the master: a stack of four voices is where a little soft
            // clipping pays, because it glues the voices together ahead of the room. Low
            // drive (k = 1 + 9 x 0.25) and warmth at 0.6 keep the added harmonics low in
            // the band, so the stack gains body without gaining hiss on top of the de-esser.
            { "saturation_enable", 1.0f }, { "saturation_drive", 0.25f }, { "saturation_tone", 0.6f },
            // The stack is a tight unison: no tempo copies, the room is the harmony reverb alone.
            { "delay_enable", 0.0f }
        };
        static const std::map<juce::String, float> liveScene = {
            { "amount", 0.9f }, { "flex_tune", 10.0f }, { "humanize", 30.0f },
            { "harmony_enable", 0.0f }, { "attack_aware", 1.0f },
            { "latency_mode", 0.0f }, // Direct Monitoring
            { "lead_reverb_enable", 0.0f }, { "noise_gate_enable", 0.0f },
            // On stage the plugin sits in a live monitor path: nothing here adds loudness,
            // so the master drive stays off and the level matches the house mix directly.
            // A live monitor path is also echo-free by definition - no tempo copies.
            { "saturation_enable", 0.0f }, { "delay_enable", 0.0f }
        };
        static const std::map<juce::String, float> spokenPodcast = {
            { "amount", 0.7f }, { "speed", 40.0f }, { "flex_tune", 35.0f },
            { "humanize", 25.0f }, { "vibrato_preserve", 0.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", 1.0f },
            { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "lead_reverb_enable", 0.0f }, { "noise_gate_enable", 1.0f },
            { "noise_gate_threshold", -45.0f },
            // Spoken/quiet material: lift weak passages before tuning so the
            // pitch detector and correction see a more consistent level.
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.6f },
            // Speech is where sibilance hurts most: the upward compressor lifts
            // every quiet "s" and a close mic on a podcast rig makes it worse.
            // Medium intensity; the band centre follows the Voice Type, which for
            // spoken material is usually Universal (7 kHz).
            { "deess_enable", 1.0f }, { "deess_amount", 0.65f },
            // Spoken voice is where a little drive pays: at low drive the stage adds the
            // presence harmonics a close mic loses, and warmth at 0.7 keeps them under the
            // de-esser band so the lift reads as body, not as hiss.
            { "saturation_enable", 1.0f }, { "saturation_drive", 0.25f }, { "saturation_tone", 0.7f },
            // Speech is linear: a podcast rig wants no tempo echoes on the voice.
            { "delay_enable", 0.0f }
        };
        static const std::map<juce::String, float> creativeRobot = {
            { "amount", 1.0f }, { "flex_tune", 0.0f }, { "humanize", 0.0f },
            // The name promises a hard tune, and the two parameters that make it
            // hard are NOT sound-flavoured ones: the factory correction mode is
            // Transparent (which scales the amount by 0.8 and floors the retune
            // speed at 30 ms) and the factory speed is 20 ms. A preset that leaves
            // both alone loads as a soft robot, so pin them explicitly.
            { "speed", 0.0f }, { "correction_mode", 0.0f },
            { "harmony_enable", 0.0f }, { "formant_enable", 1.0f },
            { "formant_mode", 1.0f }, { "formant", 3.0f }, // strong formant shift
            { "vibrato_preserve", 0.0f },
            // The hard-tuned robot voice is the one preset that wants added harmonics:
            // drive at 0.45 (k = 1 + 9 x 0.45) pushes the locked pitch through the soft
            // clipper, and low warmth keeps the edge bright, which is the effect's point.
            { "saturation_enable", 1.0f }, { "saturation_drive", 0.45f }, { "saturation_tone", 0.2f },
            // The robot voice stays hard-tuned and dry: a ping-pong tail would smear the
            // locked pitch, so the delay stage is switched off here explicitly.
            { "delay_enable", 0.0f }
        };
        // Hard Tune (Cher / T-Pain): the "Auto-Tune as an instrument" sound. The
        // recipe is deliberately small -- three settings carry the effect and two
        // of them are easy to miss:
        //   - correction_mode 0 is mandatory. The factory default is Transparent,
        //     which multiplies the amount by 0.8 and floors the retune speed at
        //     30 ms (PluginProcessor::syncParameters / processBlock), so a hard
        //     tune that forgets it always sounds half-corrected;
        //   - speed 0 is the fastest retune the engine offers;
        //   - vibrato_preserve 0 + humanize 0 leave no modulation alive, so the
        //     voice snaps to the scale grid instead of gliding like a singer.
        // `hard_tune` 1 switches on the Hard Tune engine path: it bypasses the
        // 80 ms anti-jitter speed floor, the pitch shifter's attack duck and the
        // formant preserver, which is what turns the very fast glide into a snap
        // (see the mode's comment in PluginProcessor.cpp). Without it the preset
        // still sounds hard-tuned, but with the engine's ~80 ms glide.
        // Formant +2 st is the Cher "chipmunk" colour. The T-Pain flavour is the
        // same sound with the Formant switch off (or +1 st) and less drive; it
        // is documented in docs/reference/presets.md rather than shipped as a
        // second preset, because the two differ by one parameter and the factory
        // list is the user's first screen.
        // The key/scale is NOT overridden: applyPluginPresetState() preserves the
        // user's musical choice (chromatic is the rawest target set, the song's
        // key/scale is the production one). Same for the latency mode: the preset
        // leaves it alone so loading it never changes the session's monitored
        // latency. The limiter stays on, as in every factory preset.
        static const std::map<juce::String, float> hardTune = {
            { "speed", 0.0f }, { "amount", 1.0f }, { "correction_mode", 0.0f },
            { "hard_tune", 1.0f },
            { "flex_tune", 0.0f }, { "humanize", 0.0f }, { "vibrato_preserve", 0.0f },
            { "harmony_enable", 0.0f }, { "double_enable", 0.0f },
            { "formant_enable", 1.0f }, { "formant_mode", 1.0f }, { "formant", 2.0f },
            { "formant_strategy", 4.0f }, // Precise: the allpass path, no tone colouring
            { "lead_reverb_enable", 0.0f }, { "harmony_reverb_enable", 0.0f },
            { "noise_gate_enable", 0.0f }, { "upward_comp_enable", 0.0f },
            { "deess_enable", 0.0f },
            { "saturation_enable", 1.0f }, { "saturation_drive", 0.4f }, { "saturation_tone", 0.25f },
            { "delay_enable", 0.0f }, { "limiter_enable", 1.0f }
        };
        // Broadcast Voice: strong, consistent level for streaming/radio. A firm
        // gate cuts room tone between phrases and the upward compressor lifts
        // any dip so the loudness stays even and present on air.
        static const std::map<juce::String, float> broadcastVoice = {
            { "amount", 0.9f }, { "speed", 30.0f }, { "flex_tune", 20.0f },
            { "humanize", 20.0f }, { "harmony_enable", 0.0f },
            { "formant_enable", 1.0f }, { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "lead_reverb_enable", 0.0f }, { "noise_gate_enable", 1.0f },
            { "noise_gate_threshold", -50.0f },
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.8f },
            // On air the 0.8 upward lift pushes every sibilant up to the same loud
            // level, which is what makes a broadcast voice sound harsh on small
            // speakers. High intensity (15 dB of depth at full pull) keeps the level
            // even and clean; the band centre follows the Voice Type.
            { "deess_enable", 1.0f }, { "deess_amount", 0.8f },
            // On air the voice must stay present without getting loud: a low drive adds
            // the density a radio voice needs, and warmth at 0.6 keeps that added energy
            // out of the band the de-esser is already working on.
            { "saturation_enable", 1.0f }, { "saturation_drive", 0.3f }, { "saturation_tone", 0.6f },
            // A broadcast voice stays dry: copies would blur the diction the preset promises.
            { "delay_enable", 0.0f }
        };
        // Breathy Vocals: airy, weak indie/lo-fi voice. No gate (keep the natural
        // breath), just a gentle upward lift so the quiet, breathy parts stay
        // intelligible and sit in tune without losing their character.
        static const std::map<juce::String, float> breathyVocals = {
            { "amount", 0.8f }, { "speed", 50.0f }, { "flex_tune", 30.0f },
            { "humanize", 35.0f }, { "vibrato_preserve", 0.3f },
            { "harmony_enable", 0.0f }, { "formant_enable", 1.0f },
            { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "lead_reverb_enable", 1.0f }, { "lead_room_send", 0.15f }, { "lead_hall_send", 0.40f },
            { "reverb_duck_depth", 0.30f }, { "noise_gate_enable", 0.0f },
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.6f },
            // The lo-fi character is the point of this preset: a whisper of drive gives
            // the breathy voice a little body, and warmth at 0.8 keeps it soft, so the
            // stage never adds the top end an airy recording already has too much of.
            { "saturation_enable", 1.0f }, { "saturation_drive", 0.15f }, { "saturation_tone", 0.8f },
            // The lo-fi character stays dry: a ping-pong tail is not part of this sound.
            { "delay_enable", 0.0f }
        };
        // Lead Vocal: the flagship "thickened lead" sound. A light gate tames
        // room tone, "-3 / +3" harmony (3rd below + above) fattens the lead
        // without smearing it, a touch of reverb adds space, and a moderate
        // upward compression keeps the lead consistent and present.
        static const std::map<juce::String, float> leadVocal = {
            { "amount", 0.9f }, { "speed", 40.0f }, { "flex_tune", 15.0f },
            { "humanize", 20.0f }, { "vibrato_preserve", 0.2f },
            { "harmony_enable", 1.0f }, { "harmony_type", 3.0f }, // 3rd below + above (-3 / +3)
            { "harmony_gain_match", 1.0f }, { "harmony_follow_lead", 1.0f },
            { "harmony_gain", 0.6f }, { "harmony_blend", 0.5f },
            { "harmony_tone_color", 0.5f },
            { "formant_enable", 1.0f }, { "formant_mode", 1.0f }, { "formant", 0.0f },
            { "lead_reverb_enable", 1.0f }, { "lead_room_send", 0.15f }, { "lead_hall_send", 0.45f },
            { "reverb_duck_depth", 0.35f },
            { "noise_gate_enable", 1.0f }, { "noise_gate_threshold", -40.0f },
            { "upward_comp_enable", 1.0f }, { "upward_comp_amount", 0.5f },
            // The thickened lead is also the most sibilant preset: upward
            // compression, harmony voices and reverb all add top end on top of the
            // doubled "s". De-essing before the reverb keeps the space without the
            // hiss, at the standard intensity; the band centre follows Voice Type.
            { "deess_enable", 1.0f }, { "deess_amount", 0.65f },
            // The "-3 / +3" voices get their own long room: the harmony HALL fattens the
            // stack without smearing the lead, which keeps its balanced room + hall pair.
            { "harmony_reverb_enable", 1.0f }, { "harmony_room_send", 0.10f }, { "harmony_hall_send", 0.30f },
            // The thickened lead is also where the master stage belongs: drive at 0.35 glues
            // the lead and its "-3 / +3" voices ahead of the room, and warmth at 0.5 keeps the
            // added harmonics in the presence region, below the de-esser's band centre.
            { "saturation_enable", 1.0f }, { "saturation_drive", 0.35f }, { "saturation_tone", 0.5f },
            // The flagship lead stays dry: its space comes from the two rooms, not from copies.
            { "delay_enable", 0.0f }
        };

        if (name == "Pure Correction")       return &correctionPure;
        if (name == "Transparent / Doubling") return &transparentDub;
        if (name == "Studio Stack")          return &studioStack;
        if (name == "Live Scene")            return &liveScene;
        if (name == "Spoken Voice / Podcast") return &spokenPodcast;
        if (name == "Creative Robot")        return &creativeRobot;
        if (name == "Hard Tune (Cher / T-Pain)") return &hardTune;
        if (name == "Broadcast Voice")       return &broadcastVoice;
        if (name == "Breathy Vocals")        return &breathyVocals;
        if (name == "Lead Vocal")            return &leadVocal;
        return nullptr; // "Default" -> factory defaults
    }
}

void OpenVoxTunerAudioProcessorEditor::refreshPluginPresetList()
{
    pluginPresetNames.clear();
    pluginPresetNames.addArray (getFactoryPluginPresetNames());

    const auto dir = getUserPluginPresetsDirectory();
    const auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
        pluginPresetNames.add (f.getFileNameWithoutExtension());

    // "User" marks a non-preset (modified) state, shown when no named preset matches.
    if (! pluginPresetNames.contains ("User"))
        pluginPresetNames.add ("User");

    // The combo shows user file names, which are an untrusted edge: harden them
    // for the text shaper. The lookup key stays the RAW name, so preset matching
    // is never affected by the display form.
    juce::StringArray displayNames;
    for (const auto& n : pluginPresetNames)
        displayNames.add (ebs::sanitizeForShaping (n));

    presetComboBox.clear();
    presetComboBox.addItemList (displayNames, 1);
}

void OpenVoxTunerAudioProcessorEditor::applyPluginPresetByName (const juce::String& name)
{
    if (name == "User" || name.isEmpty())
        return;

    const juce::ValueTree state = buildPluginPresetStateByName (name);
    if (! state.isValid())
        return;

    processorRef.applyPluginPresetState (state);
    currentPluginPresetName = name;
    presetComboBox.setText (ebs::sanitizeForShaping (name), juce::dontSendNotification);

    // Record the assigned preset on the active A/B slot so that switching
    // slots later can show the preset name (or "User" if it was edited).
    (isSlotAActive ? slotA : slotB).presetName = name;
}

juce::ValueTree OpenVoxTunerAudioProcessorEditor::buildPluginPresetStateByName (const juce::String& name)
{
    if (name == "User" || name.isEmpty())
        return {};

    // Factory preset: build the state from the default tree + overrides.
    const auto& factoryNames = getFactoryPluginPresetNames();
    if (factoryNames.contains (name))
    {
        juce::ValueTree state = processorRef.getDefaultPluginState().createCopy();
        if (const auto* ov = getFactoryPluginPresetOverrides (name))
        {
            for (const auto& [id, val] : *ov)
            {
                juce::ValueTree paramChild = state.getChildWithProperty ("id", id);
                if (paramChild.isValid())
                    paramChild.setProperty ("value", val, nullptr);
            }
        }
        return state;
    }

    // Custom preset: parse the XML file from the Plugin presets folder.
    const auto file = getUserPluginPresetsDirectory().getChildFile (name + ".xml");
    if (! file.existsAsFile())
        return {};

    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument (file).getDocumentElement());
    if (xml == nullptr)
        return {};

    const juce::XmlElement* paramsXml = nullptr;
    if (xml->hasTagName ("OVT_PLUGIN_PRESET"))
    {
        paramsXml = xml->getChildByName (processorRef.getParameters().state.getType());
        if (paramsXml == nullptr)
            paramsXml = xml->getChildByName ("PARAMETERS");
    }
    else if (xml->hasTagName (processorRef.getParameters().state.getType()))
    {
        paramsXml = xml.get();
    }

    if (paramsXml == nullptr)
        return {};

    return juce::ValueTree::fromXml (*paramsXml);
}

bool OpenVoxTunerAudioProcessorEditor::slotMatchesPreset (const ABState& slot)
{
    if (slot.presetName.isEmpty())
        return false;

    const juce::ValueTree expected = buildPluginPresetStateByName (slot.presetName);
    if (! expected.isValid())
        return false;

    // Compare only the parameters the preset defines (the preset's children).
    // The stored "value" is the parameter's normalized 0..1 value, which is
    // exactly what AudioProcessorParameter::getValue() returns, so a direct
    // tolerance comparison is correct. UI language / theme / morph / mode are
    // intentionally excluded (presets never touch them).
    const auto& params = processorRef.getParameters();
    for (int i = 0; i < expected.getNumChildren(); ++i)
    {
        const auto child = expected.getChild (i);
        const auto idVar = child.getProperty ("id", juce::var());
        if (idVar.isVoid())
            continue;
        const juce::String id = idVar;
        if (auto* p = params.getParameter (id))
        {
            const float liveVal   = p->getValue();
            const float presetVal = static_cast<float> (child.getProperty ("value", 0.0));
            if (std::abs (liveVal - presetVal) > 1.0e-3f)
                return false;
        }
    }
    return true;
}

void OpenVoxTunerAudioProcessorEditor::promptSavePluginPreset()
{
    auto* w = new juce::AlertWindow (ovt::tr(ovt::Keys::kDlgSavePluginPreset),
                                      ovt::tr(ovt::Keys::kDlgSavePluginPresetDesc),
                                      juce::AlertWindow::NoIcon);
    w->addTextEditor ("name", "", "Name:");
    w->addButton (ovt::tr(ovt::Keys::kDlgSave), 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton (ovt::tr(ovt::Keys::kMenuCancel), 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result) {
        if (result == 0)
        {
            delete w;
            return;
        }

        const juce::String name = w->getTextEditorContents ("name").trim();
        delete w;

        if (name.isEmpty())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         ovt::tr(ovt::Keys::kDlgInvalidName),
                                                         ovt::tr(ovt::Keys::kDlgEmptyName),
                                                         this);
            return;
        }

        const auto file = getUserPluginPresetsDirectory()
                             .getChildFile (sanitizePresetFileStem (name) + ".xml");

        if (file.existsAsFile())
        {
            auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                                   ovt::tr(ovt::Keys::kDlgOverwrite),
                                                                   ovt::tr(ovt::Keys::kDlgOverwriteDesc),
                                                                   ovt::tr(ovt::Keys::kMenuCancel),
                                                                   ovt::tr(ovt::Keys::kDlgOverwriteBtn),
                                                                   this);
            juce::NativeMessageBox::showAsync (opts, [this, file, name] (int result) {
                if (result != 2)
                    return;
                writePluginPresetFile (name, file);
                refreshPluginPresetList();
                currentPluginPresetName = name;
                presetComboBox.setText (ebs::sanitizeForShaping (name), juce::dontSendNotification);
            });
            return;
        }

        writePluginPresetFile (name, file);
        refreshPluginPresetList();
        currentPluginPresetName = name;
        presetComboBox.setText (ebs::sanitizeForShaping (name), juce::dontSendNotification);
    }), true);
}

void OpenVoxTunerAudioProcessorEditor::writePluginPresetFile (const juce::String& name, const juce::File& file)
{
    // A Plugin Preset captures parameters.state (no pitch curve) plus a few
    // UI-only preferences. Serialised as <OVT_PLUGIN_PRESET> with a
    // <PARAMETERS> child so it never collides with Curve Preset XML shape.
    auto stateXml = processorRef.getPluginPresetState().createXml();
    if (stateXml == nullptr)
        return;

    juce::XmlElement root ("OVT_PLUGIN_PRESET");
    root.setAttribute ("name", name);
    root.setAttribute ("advancedExpanded", correctionFrame.expanded() ? 1 : 0);
    root.setAttribute ("harmonyExpanded", harmonyFrame.expanded() ? 1 : 0);
    // The Reverb frame's fold is persisted exactly like Harmony's: the attribute rides on the preset
    // root and is read back by restoreState(), so a preset opens with the send room in the state you
    // saved it in. (The Effects frame deliberately does NOT persist its fold.)
    root.setAttribute ("reverbExpanded", reverbFrame.expanded() ? 1 : 0);
    // stateXml already carries the real parameters.state root type (e.g.
    // "OpenVoxTuner") with <PARAM id=.. value=..> children, so restoreState()
    // via replaceState() keeps the tree type consistent.
    root.addChildElement (stateXml.release());

    const bool ok = file.replaceWithText (root.toString());
    if (ok)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                     ovt::tr(ovt::Keys::kDlgPresetSaved),
                                                     ovt::tr(ovt::Keys::kDlgPresetSavedDesc) + name,
                                                     this);
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     ovt::tr(ovt::Keys::kDlgSaveFailed),
                                                     ovt::tr(ovt::Keys::kDlgSaveFailedDesc),
                                                     this);
    }
}

void OpenVoxTunerAudioProcessorEditor::deletePluginPresetFile (const juce::File& file)
{
    file.deleteFile();
    refreshPluginPresetList();
    if (currentPluginPresetName == file.getFileNameWithoutExtension())
    {
        currentPluginPresetName = "User";
        presetComboBox.setText ("User", juce::dontSendNotification);
    }
}

// === MIDI Learn ===
void OpenVoxTunerAudioProcessorEditor::startMidiLearn (const juce::String& parameterId)
{
    learnState.isLearning = true;
    learnState.parameterId = parameterId;
    learnState.assignedCc = -1;

    // Show a brief dialog prompting the user to move a MIDI controller.
    juce::AlertWindow::showMessageBoxAsync (
        juce::AlertWindow::InfoIcon,
        ovt::tr(ovt::Keys::kDlgMidiLearn),
        ovt::tr(ovt::Keys::kDlgMidiLearnDesc),
        ovt::tr(ovt::Keys::kDlgOk));
}

void OpenVoxTunerAudioProcessorEditor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (! learnState.isLearning || ! message.isController()) return;

    const int cc = message.getControllerNumber();
    auto* param = processorRef.getParameters().getParameter (learnState.parameterId);
    if (param != nullptr)
    {
        // Store the CC assignment (could be persisted to state).
        learnState.assignedCc = cc;
        learnState.isLearning = false;
    }
}

void OpenVoxTunerAudioProcessorEditor::applyFactoryPreset (const juce::String& name)
{
    if (curveEditor == nullptr)
        return;
    pushUndoAroundCall ([this, &name] {
        resetMorph(); // cancel any active morph when loading a preset
        ovtdsp::PitchCurve newCurve;
        newCurve.loadPreset (name);
        curveEditor->setCurve (newCurve);

        // A preset replaces the current editor state, so commit it to the active
        // A/B slot. Otherwise switching slots would discard the preset and reload
        // the slot's stale stored curve (the preset only touches the editor curve,
        // not the slot's MorphState).
        if (isSlotAActive)
            saveSlot (slotA, 0);
        else
            saveSlot (slotB, 1);

        // Keep the morph slider aligned with the active slot so the displayed
        // curve matches the slot the preset was applied to (and the auto-save on
        // the next slot switch captures it correctly).
        processorRef.setMorphAmount (isSlotAActive ? 0.0f : 1.0f);

        syncEditButtons();
    });
}

juce::PopupMenu OpenVoxTunerAudioProcessorEditor::buildPresetsMenu()
{
    auto loadFactory = [this] (const juce::String& name) {
        applyFactoryPreset (name);
    };

    juce::PopupMenu factory;
    factory.addItem ("Default",   [loadFactory] { loadFactory ("default");    });
    factory.addSeparator();
    factory.addItem ("Robot (C3)", [loadFactory] { loadFactory ("robot_c3");    });
    factory.addItem ("Robot (C4)", [loadFactory] { loadFactory ("robot_c4");    });
    factory.addSeparator();
    factory.addItem ("Spoken Voice (Male)",   [loadFactory] { loadFactory ("spoken_male");    });
    factory.addItem ("Spoken Voice (Female)", [loadFactory] { loadFactory ("spoken_female");    });
    factory.addSeparator();
    factory.addItem ("Bass",     [loadFactory] { loadFactory ("bass");    });
    factory.addItem ("Baritone", [loadFactory] { loadFactory ("baritone");    });
    factory.addItem ("Tenor",    [loadFactory] { loadFactory ("tenor");    });
    factory.addItem ("Alto",     [loadFactory] { loadFactory ("alto");    });
    factory.addItem ("Mezzo",    [loadFactory] { loadFactory ("mezzo");    });
    factory.addItem ("Soprano",  [loadFactory] { loadFactory ("soprano");    });

    juce::PopupMenu custom;
    const auto dir = getUserPresetsDirectory();
    const auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
        custom.addItem (f.getFileNameWithoutExtension(), [this, f] { loadCustomPresetFromFile (f);    });

    custom.addSeparator();
    custom.addItem (ovt::tr (ovt::Keys::kMenuSavePresetAs), [this] { promptSaveCustomPreset();    });

    juce::PopupMenu deleteMenu;
    for (const auto& f : files)
        deleteMenu.addItem (f.getFileNameWithoutExtension(), [this, f] { deleteCustomPresetFile (f);    });

    custom.addSubMenu (ovt::tr (ovt::Keys::kMenuDeletePreset), deleteMenu, ! files.isEmpty());

    juce::PopupMenu menu;
    // Preset Gallery opens the browsable grid (factory + custom, with thumbnails).
    menu.addItem (ovt::tr (ovt::Keys::kMenuPresetGallery), [this] { showPresetGallery();    });
    menu.addSubMenu (ovt::tr (ovt::Keys::kMenuFactory), factory);
    menu.addSubMenu (ovt::tr (ovt::Keys::kMenuCustom), custom);
    menu.addSeparator();
    menu.addItem (ovt::tr (ovt::Keys::kMenuCleanCurves), [this] {
        if (curveEditor != nullptr) curveEditor->clearCurve();
        processorRef.resetTransportTime();
    });
    return menu;
}

void OpenVoxTunerAudioProcessorEditor::showPresetsMenu (const juce::MouseEvent* mouseEvent)
{
    if (curveEditor == nullptr)
        return;

    juce::PopupMenu menu = buildPresetsMenu();
    applyMenuLookAndFeel (menu, customLookAndFeel);

    auto opts = juce::PopupMenu::Options();
    if (mouseEvent != nullptr)
    {
        opts = opts.withTargetComponent (mouseEvent->eventComponent)
                   .withTargetScreenArea (juce::Rectangle<int> (mouseEvent->getScreenX(), mouseEvent->getScreenY(), 1, 1));
    }
    else
    {
        opts = opts.withTargetComponent (&optionsButton)
                   .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards);
    }

    menu.showMenuAsync (opts, [] (int) {    });
}

void OpenVoxTunerAudioProcessorEditor::showCurveOptionsMenu()
{
    if (curveEditor == nullptr)
        return;

    const bool isStandalone = processorRef.isStandaloneWrapper();
    juce::PopupMenu menu;

    // Auto-Scroll (ticked): follows the playhead during playback. In Standalone
    // the playhead loops on the Measures window, so the option is disabled.
    const bool autoScroll = curveEditor->getAutoScroll();
    menu.addItem (ovt::tr (ovt::Keys::kMenuAutoScroll), ! isStandalone, autoScroll, [this] {
        if (curveEditor == nullptr)
            return;
        const bool next = ! curveEditor->getAutoScroll();
        curveEditor->setAutoScroll (next);
        if (auto* p = processorRef.getParameters().getRawParameterValue ("auto_scroll"))
            const_cast<std::atomic<float>*>(p)->store (next ? 1.0f : 0.0f);
    });

    // Loop Playhead (Measures): in ARA the host owns the timeline (disabled,
    // follows host); in Standalone the playhead always loops on the Measures
    // window (disabled, ticked); in plugin mode the user chooses (enabled).
    const bool isARA = processorRef.isBoundToARA_custom();
    const bool loopItemEnabled = ! isARA && ! isStandalone;
    const bool loopItemTicked = isARA ? false : (isStandalone ? true : processorRef.getPlayheadLoop());
    menu.addItem (ovt::tr (ovt::Keys::kMenuLoopPlayhead), loopItemEnabled, loopItemTicked, [this] {
        if (curveEditor == nullptr)
            return;
        processorRef.setPlayheadLoop (! processorRef.getPlayheadLoop());
    });

    menu.addSeparator();

    // === Display options (Curve Editor only) ===
    // Both "Show Input Trace" (red line) and "Show Harmonies Trace" (blue
    // lines) are kept together here because they belong to the same logical
    // group: visual overlays of the curve editor. The PitchVisualizer is
    // unaffected and manages its own overlays via the harmony engine.
    {
        const bool showInputTrace = curveEditor->getShowInputTrace();
        menu.addItem (ovt::tr (ovt::Keys::kMenuShowInputTrace), true, showInputTrace, [this] {
            if (curveEditor == nullptr)
                return;
            const bool next = ! curveEditor->getShowInputTrace();
            curveEditor->setShowInputTrace (next);
        });

        // Show Harmonies Trace: toggles the blue harmony voice lines
        // in the Curve Editor only.
        menu.addItem (ovt::tr (ovt::Keys::kMenuShowHarmoniesTrace), true, showHarmoniesTrace, [this] {
            showHarmoniesTrace = ! showHarmoniesTrace;
            if (curveEditor != nullptr)
                curveEditor->setShowHarmoniesTrace (showHarmoniesTrace);
        });
    }

    menu.addSeparator();

    // Reset Playhead is hidden (there is a dedicated button for this now).

    // Standalone-only controls.
    if (isStandalone)
    {
        // Tempo (BPM): the standalone timeline advances at this rate.
        juce::PopupMenu tempo;
        const float currentBpm = processorRef.getBpm();
        static constexpr float tempoChoices[] = { 60.0f, 70.0f, 80.0f, 90.0f, 100.0f,
                                                  110.0f, 120.0f, 130.0f, 140.0f, 150.0f,
                                                  160.0f, 180.0f };
        for (float b : tempoChoices)
            tempo.addItem (juce::String (static_cast<int> (b)) + " BPM",
                           true, std::abs (b - currentBpm) < 0.5f,
                           [this, b] { processorRef.setBpm (b);    });
        menu.addSubMenu (ovt::tr (ovt::Keys::kMenuTempo), tempo, true);
    }

    menu.addSeparator();

    // Curve Presets: opens the preset manager as a submenu.
    menu.addSubMenu (ovt::tr (ovt::Keys::kMenuCurvePresets), buildPresetsMenu(), true);

    menu.addSeparator();

    // Import MIDI: open a file chooser for .mid files and generate a pitch curve.
    menu.addItem (ovt::tr (ovt::Keys::kMenuImportMidi), true, false, [this] {
        midiImportFileChooser = std::make_unique<juce::FileChooser> (
            ovt::tr (ovt::Keys::kMidiDialogTitle),
            juce::File(),
            "*.mid;*.midi");

        juce::Component::SafePointer<OpenVoxTunerAudioProcessorEditor> editorSafe (this);
        midiImportFileChooser->launchAsync (
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [editorSafe] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (editorSafe != nullptr && file.existsAsFile())
                    editorSafe->handleMidiFileImport (file);
            });
    });

    // Import Audio: same delivery as Import MIDI, but the file is decoded and
    // its sung notes become the curve. The filter lists exactly the formats the
    // importer accepts (MP3 is excluded because its decoder is disabled).
    menu.addItem (ovt::tr (ovt::Keys::kMenuImportAudio), true, false, [this] {
        audioImportFileChooser = std::make_unique<juce::FileChooser> (
            ovt::tr (ovt::Keys::kAudioDialogTitle),
            juce::File(),
            "*.wav;*.aiff;*.aif;*.flac;*.ogg");

        juce::Component::SafePointer<OpenVoxTunerAudioProcessorEditor> editorSafe (this);
        audioImportFileChooser->launchAsync (
            juce::FileBrowserComponent::openMode
                | juce::FileBrowserComponent::canSelectFiles,
            [editorSafe] (const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (editorSafe != nullptr && file.existsAsFile())
                    editorSafe->handleAudioFileImport (file);
            });
    });

    applyMenuLookAndFeel (menu, customLookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&optionsButton)
                            .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards),
                        [] (int) {    });
}

void OpenVoxTunerAudioProcessorEditor::showPresetGallery()
{
    if (curveEditor == nullptr)
        return;

    if (galleryWindow != nullptr)
    {
        galleryWindow->setVisible (true);
        galleryWindow->toFront (true);
        return;
    }

    class ClosableGalleryWindow : public juce::DocumentWindow
    {
    public:
        ClosableGalleryWindow (const juce::String& name, juce::Colour backgroundColour, int requiredButtons)
            : juce::DocumentWindow (name, backgroundColour, requiredButtons) {}

        void closeButtonPressed() override
        {
            setVisible (false);
        }
    };

    auto* w = new ClosableGalleryWindow ("OpenVoxTuner - Preset Gallery",
                                           ebs::bgDark(),
                                           juce::DocumentWindow::closeButton);

    auto* gallery = new PresetGallery (
        [this] (const juce::String& id) { applyFactoryPreset (id); },
        [this] (const juce::File& f)      { loadCustomPresetFromFile (f); },
        [this] (const juce::File& f)      { /* delete handled via gallery refresh */ },
        [this] () -> juce::Array<juce::File> { return getUserPresetsDirectory().findChildFiles (juce::File::findFiles, false, "*.xml");    });

    // Keep a safe pointer so the delete callback can refresh the grid
    // after a custom preset is removed (the deletion is async).
    juce::Component::SafePointer<PresetGallery> gallerySp (gallery);
    gallery->setOnDeleteCallback ([this, gallerySp] (const juce::File& f) {
        // Parent the confirmation dialog to the gallery window so it shows
        // in front of it (the gallery is a separate DocumentWindow above the editor).
        deleteCustomPresetFile (f, [gallerySp] (bool ok) {
            if (ok && gallerySp != nullptr)
                gallerySp->refresh();
            }, gallerySp.getComponent());
    });

    w->setContentOwned (gallery, true);
    // System title bar, the house norm: the OS chrome owns the caption.
    w->setUsingNativeTitleBar (true);
    w->setSize (760, 540);
    w->setResizeLimits (480, 360, 2200, 1400);
    w->centreWithSize (760, 540);
    w->setAlwaysOnTop (true);
    w->setVisible (true);
    w->toFront (true);

    galleryWindow.reset (w);
}

