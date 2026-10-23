// PluginProcessorState.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Plugin state serialisation (parameters, pitch curve, A/B slots) and the VST3
// bundle dump used to diagnose install problems. Moved out of
// PluginProcessor.cpp (step 4 of the source split) verbatim.
#include "PluginProcessor.h"
#include "PluginProcessorInternal.h"

#include <juce_data_structures/juce_data_structures.h>

void OpenVoxTunerAudioProcessor::dumpVST3BundleInfo()
{
    juce::StringArray commonPaths;
#if JUCE_WINDOWS
    commonPaths.add ("C:/Program Files/Common Files/VST3");
    commonPaths.add ("C:/Program Files (x86)/Common Files/VST3");
#elif JUCE_MAC
    commonPaths.add ("/Library/Audio/Plug-Ins/VST3");
    commonPaths.add ("~/Library/Audio/Plug-Ins/VST3");
#else
    commonPaths.add ("/usr/lib/vst3");
    commonPaths.add ("/usr/local/lib/vst3");
#endif

    OVT_LOG ("Dumping VST3 search paths...");
    juce::File found;
    for (auto& p : commonPaths)
    {
        juce::File dir (p);
        if (! dir.exists()) continue;
        auto children = dir.findChildFiles (juce::File::findDirectories, false);
        for (auto& c : children)
        {
            if (c.getFileName().containsIgnoreCase ("OpenVoxTuner"))
            {
                OVT_LOG ("Found candidate bundle: " + c.getFullPathName());
                auto files = c.findChildFiles (juce::File::findFiles, true);
                for (auto& f : files)
                {
                    OVT_LOG ("  " + f.getRelativePathFrom (c));
                }
                found = c;
            }
        }
    }

    if (! found.exists())
    {
        OVT_LOG ("OpenVoxTuner.vst3 not found in common VST3 folders.");
    }
    else
    {
        OVT_LOG ("Bundle dump complete.");

        // Try to read the moduleinfo.json for VST3 metadata (contains event/bus info)
        juce::File moduleInfo = found.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("moduleinfo.json");
        if (moduleInfo.existsAsFile())
        {
            OVT_LOG ("Reading moduleinfo.json: " + moduleInfo.getFullPathName());
            juce::String content = moduleInfo.loadFileAsString();
            const int maxLog = 8192;
            if (content.length() > maxLog)
                OVT_LOG (content.substring (0, maxLog) + "... (truncated)");
            else
                OVT_LOG (content);
        }
        else
        {
            OVT_LOG ("moduleinfo.json not found inside bundle.");
        }
    }
}

// === Plugin state: XML serialization of parameters + pitch curve ===
void OpenVoxTunerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Before saving, make the ACTIVE slot reflect the current live state so any
    // edits made just before exit are never lost (the editor only recaptures a
    // slot when the user switches away from it). This keeps the whole-state
    // snapshot consistent on quit/restart regardless of slot-switch timing.
    if (pitchCurve != nullptr)
    {
        const int active = juce::jlimit (0, 1, abActiveSlot);
        auto live = ovtdsp::captureState (parameters, *pitchCurve,
                                          active == 0 ? "Slot A" : "Slot B");
        if (active == 0) abSlotAMorph = live;
        else             abSlotBMorph = live;
        abSlotHasData[active] = true;
    }

    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (pitchCurve != nullptr)
    {
        auto curveXml = pitchCurve->toXml();
        if (xml != nullptr && curveXml != nullptr)
            xml->addChildElement (curveXml.release());
    }
    // Persist UI-only preferences that are not AudioParameters.
    xml->setAttribute ("advancedExpanded", advancedExpandedState ? 1 : 0);
    xml->setAttribute ("harmonyExpanded", harmonyExpandedState ? 1 : 0);
    xml->setAttribute ("reverbExpanded", reverbExpandedState ? 1 : 0);
    xml->setAttribute ("effectsExpanded", effectsExpandedState ? 1 : 0);
    xml->setAttribute ("abActiveSlot", abActiveSlot);
    // The standalone tempo: the timeline advances at it, the Curve Editor draws its grid on it and
    // the tempo-synced Delay reads it (a DAW replaces it with the host tempo, an ARA source with its
    // tempo map). It used to be forgotten on every reload, which put a 137 BPM session back at 120.
    xml->setAttribute ("bpm", (double) bpm.load());

    // Persist A/B slot MorphStates as compact flat attributes (no nested XML).
    for (int slot = 0; slot < 2; ++slot)
    {
        if (! hasAbSlotData (slot)) continue;
        const auto& ms = (slot == 0) ? abSlotAMorph : abSlotBMorph;
        auto* slotXml = xml->createNewChildElement (slot == 0 ? "AB_A" : "AB_B");
        slotXml->setAttribute ("speed",            (double) ms.speed);
        slotXml->setAttribute ("amount",           (double) ms.amount);
        slotXml->setAttribute ("formant",          (double) ms.formant);
        slotXml->setAttribute ("harmonyGain",      (double) ms.harmonyGain);
        slotXml->setAttribute ("harmonyBlend",     (double) ms.harmonyBlend);
        slotXml->setAttribute ("harmonyToneColor", (double) ms.harmonyToneColor);
        slotXml->setAttribute ("doubleAmount",     (double) ms.doubleAmount);
        slotXml->setAttribute ("doubleEnable",     ms.doubleEnable);
        // Reverb amounts are written with the state, so a slot carries the room balance it
        // was built with (and its two on/off switches, written with the booleans below).
        slotXml->setAttribute ("leadRoomSend",     (double) ms.leadRoomSend);
        slotXml->setAttribute ("leadHallSend",     (double) ms.leadHallSend);
        slotXml->setAttribute ("harmonyRoomSend",  (double) ms.harmonyRoomSend);
        slotXml->setAttribute ("harmonyHallSend",  (double) ms.harmonyHallSend);
        slotXml->setAttribute ("reverbDuckDepth",  (double) ms.reverbDuckDepth);
        slotXml->setAttribute ("flexTune",         (double) ms.flexTune);
        slotXml->setAttribute ("humanize",         (double) ms.humanize);
        slotXml->setAttribute ("vibratoPreserve",  (double) ms.vibratoPreserve);
        slotXml->setAttribute ("attackAware",      (double) ms.attackAware);
        slotXml->setAttribute ("attackRelease",    (double) ms.attackRelease);
        slotXml->setAttribute ("keySource",        (double) ms.keySource);
        slotXml->setAttribute ("companionGroup",   (double) ms.companionGroup);
        slotXml->setAttribute ("keyDetect",        (double) ms.keyDetect);
        slotXml->setAttribute ("harmonyFollowLead", (double) (ms.harmonyFollowLead ? 1 : 0));
        slotXml->setAttribute ("key",              ms.key);
        slotXml->setAttribute ("scale",            ms.scale);
        slotXml->setAttribute ("harmonyType",      ms.harmonyType);
        slotXml->setAttribute ("harmonyTone",      ms.harmonyTone);
        slotXml->setAttribute ("shiftedVoices",    ms.harmonyShiftedVoices);
        slotXml->setAttribute ("harmonyFormant",   (double) ms.harmonyFormant);
        slotXml->setAttribute ("harmonyAttack",    (double) ms.harmonyAttack);
        slotXml->setAttribute ("harmonyGainMatch", ms.harmonyGainMatch);
        // The twelve custom notes travel as ONE 12-char attribute ("1" in the scale, "0" out):
        // twelve separate attributes would be twelve chances to forget one.
        {
            juce::String notes;
            for (const bool on : ms.customNote)
                notes += on ? "1" : "0";
            slotXml->setAttribute ("customNotes", notes);
        }
        slotXml->setAttribute ("voiceType",        ms.voiceType);
        slotXml->setAttribute ("latencyMode",      ms.latencyMode);
        slotXml->setAttribute ("editorMeasures",   ms.editorMeasures);
        slotXml->setAttribute ("formantEnable",    ms.formantEnable);
        slotXml->setAttribute ("bypass",           ms.bypass);
        slotXml->setAttribute ("harmonyEnable",    ms.harmonyEnable);
        slotXml->setAttribute ("useVoice",         ms.harmonyUseVoice);
        slotXml->setAttribute ("leadReverbEnable",    ms.leadReverbEnable);
        slotXml->setAttribute ("harmonyReverbEnable", ms.harmonyReverbEnable);
        slotXml->setAttribute ("outputTrim",       (double) ms.outputTrim);
        slotXml->setAttribute ("limiterEnable",    ms.limiterEnable);
        slotXml->setAttribute ("correctionMode",   ms.correctionMode);
        slotXml->setAttribute ("hardTune",         ms.hardTune);
        slotXml->setAttribute ("noiseGateEnable",  ms.noiseGateEnable);
        slotXml->setAttribute ("noiseGateThreshold", (double) ms.noiseGateThreshold);
        slotXml->setAttribute ("upwardCompEnable", ms.upwardCompEnable);
        slotXml->setAttribute ("upwardCompAmount", (double) ms.upwardCompAmount);

        // The de-esser is morphed and captured, so it has to be stored too: a slot that
        // switches the de-esser off must restore it off. Attributes absent in an old slot
        // file read back as off at the parameter default (intensity 0.65).
        slotXml->setAttribute ("deessEnable",      ms.deessEnable);
        slotXml->setAttribute ("deessAmount",      (double) ms.deessAmount);

        // Saturation belongs to a slot's sound: drive 0 is transparent, so switching slots
        // fades the stage through its own bypass ramp. A slot saved before these attributes
        // existed reads back as off with the parameter defaults.
        slotXml->setAttribute ("saturationEnable", ms.saturationEnable);
        slotXml->setAttribute ("saturationDrive",  (double) ms.saturationDrive);
        slotXml->setAttribute ("saturationTone",   (double) ms.saturationTone);

        // The lead delay belongs to a slot's sound as well: a slot that switches the
        // tempo-synced echo off must restore it off. The division is stored as its combo
        // index (0..4), not as a normalised value, so an old slot cannot shift the grid.
        // Attributes absent in a slot saved before this stage read back as off at the
        // parameter defaults (mix 0.30, feedback 0.35, division 1/4).
        slotXml->setAttribute ("delayEnable",      ms.delayEnable);
        slotXml->setAttribute ("delayDivision",    ms.delayDivision);
        slotXml->setAttribute ("delayMix",         (double) ms.delayMix);
        slotXml->setAttribute ("delayFeedback",    (double) ms.delayFeedback);

        // Serialize the pitch curve so the slot restores its exact line (not an
        // empty PitchCurve) after a project reload / standalone restart.
        auto curveXml = ms.curve.toXml();
        if (curveXml != nullptr)
            slotXml->addChildElement (curveXml.release());

#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
        if (slot == 0)
            OVT_LOG ("[DIAG] getState AB_A: has=" + juce::String (hasAbSlotData (0) ? 1 : 0)
                     + " type=" + juce::String (abSlotAMorph.harmonyType)
                     + " vibrato=" + juce::String (abSlotAMorph.vibratoPreserve, 4)
                     + " humanize=" + juce::String (abSlotAMorph.humanize, 4)
                     + " formant=" + juce::String (abSlotAMorph.harmonyFormant, 4)
                     + " activeSlot=" + juce::String (abActiveSlot));
#endif
    }
    copyXmlToBinary (*xml, destData);
}

void OpenVoxTunerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (parameters.state.getType()))
    {
        // The A/B slots and the pitch curve are plugin-level data, NOT parameter
        // state. getStateInformation() appends them to the serialized XML, so if
        // they flow into parameters.state via fromXml() here, every save/load
        // round-trip re-appends another AB_A/AB_B/PITCH_CURVE and the tree
        // accumulates dozens of stale copies. getChildByName("AB_A") then returns
        // the OLDEST copy (often the defaults captured on the first ever save),
        // which is exactly the "slot A restores to defaults" bug. So we read the
        // plugin-level data first, then strip it before replacing the param tree.
        auto getLastChild = [&xmlState] (const juce::String& tag) -> juce::XmlElement*
        {
            juce::XmlElement* found = nullptr;
            if (xmlState != nullptr)
                for (auto* e = xmlState->getFirstChildElement(); e != nullptr; e = e->getNextElement())
                    if (e->hasTagName (tag)) found = e;
            return found;
        };

        // Restore A/B slot MorphStates from compact flat attributes. Reading the
        // LAST occurrence recovers the most recent state even from files written
        // by older builds that accumulated duplicate slot children.
        for (int slot = 0; slot < 2; ++slot)
        {
            auto* slotXml = getLastChild (slot == 0 ? "AB_A" : "AB_B");
            if (slotXml == nullptr) continue;
            ovtdsp::MorphState ms;
            ms.speed              = (float) slotXml->getDoubleAttribute ("speed", 0.25);
            ms.amount             = (float) slotXml->getDoubleAttribute ("amount", 0.5);
            ms.formant            = (float) slotXml->getDoubleAttribute ("formant", 0.5);
            ms.harmonyGain        = (float) slotXml->getDoubleAttribute ("harmonyGain", 0.5);
            ms.harmonyBlend       = (float) slotXml->getDoubleAttribute ("harmonyBlend", 0.5);
            ms.harmonyToneColor   = (float) slotXml->getDoubleAttribute ("harmonyToneColor", 0.5);
            ms.doubleAmount       = (float) slotXml->getDoubleAttribute ("doubleAmount", 0.0);
            ms.doubleEnable       = slotXml->getBoolAttribute ("doubleEnable", false);
            // Slots saved before the two shared buses existed carry the old insert/gate
            // reverb attributes. Map them, so a slot keeps (approximately) the room it was
            // built with instead of loading dry: the old crossfade Mix becomes the HALL send
            // (1.43 is the measured crossfade -> send calibration: the crossfade also
            // attenuated the dry voice, a send does not), the old shared send folds into the
            // same hall amount, and the old gate-linked duck depth becomes the ducker depth.
            if (slotXml->hasAttribute ("leadHallSend") || slotXml->hasAttribute ("harmonyHallSend"))
            {
                ms.leadRoomSend    = (float) slotXml->getDoubleAttribute ("leadRoomSend", 0.0);
                ms.leadHallSend    = (float) slotXml->getDoubleAttribute ("leadHallSend", 0.0);
                ms.harmonyRoomSend = (float) slotXml->getDoubleAttribute ("harmonyRoomSend", 0.0);
                ms.harmonyHallSend = (float) slotXml->getDoubleAttribute ("harmonyHallSend", 0.0);
                ms.reverbDuckDepth = (float) slotXml->getDoubleAttribute ("reverbDuckDepth", 0.0);
                // The switches only exist in a new-style slot, so they are read here and never
                // from the legacy branch below (which derives them from the old flags).
                ms.leadReverbEnable    = slotXml->getBoolAttribute ("leadReverbEnable", false);
                ms.harmonyReverbEnable = slotXml->getBoolAttribute ("harmonyReverbEnable", false);
            }
            else
            {
                bool hasReverb = false;
                const float legacyMix     = (float) slotXml->getDoubleAttribute ("reverbMix", 0.3);
                const float legacyPadMix  = (float) slotXml->getDoubleAttribute ("padReverbMix", 0.3);
                const float legacyLeadSend = (float) slotXml->getDoubleAttribute ("leadSend", 0.0);
                const float legacyPadSend  = (float) slotXml->getDoubleAttribute ("padSend", 0.0);
                const float legacyDuck     = (float) slotXml->getDoubleAttribute ("duckDepth", 0.5);
                const bool legacyLeadEnable = slotXml->getBoolAttribute ("reverbEnable", false);
                const bool legacyPadEnable  = slotXml->getBoolAttribute ("padReverbEnable", false);
                hasReverb = slotXml->hasAttribute ("reverbMix") || slotXml->hasAttribute ("padReverbMix")
                            || slotXml->hasAttribute ("reverbEnable") || slotXml->hasAttribute ("padReverbEnable");

                ms.leadRoomSend     = 0.0f;
                ms.harmonyRoomSend  = 0.0f;
                ms.leadHallSend     = juce::jlimit (0.0f, 1.0f, 1.43f * legacyMix + legacyLeadSend);
                ms.harmonyHallSend  = juce::jlimit (0.0f, 1.0f, 1.43f * legacyPadMix + legacyPadSend);
                ms.reverbDuckDepth  = juce::jlimit (0.0f, 1.0f, legacyDuck);
                ms.leadReverbEnable    = legacyLeadEnable || legacyLeadSend > 0.0f;
                ms.harmonyReverbEnable = legacyPadEnable || legacyPadSend > 0.0f;
                if (! hasReverb)
                {
                    ms.leadHallSend = 0.0f;
                    ms.harmonyHallSend = 0.0f;
                    ms.reverbDuckDepth = 0.0f;
                }
            }
            ms.flexTune           = (float) slotXml->getDoubleAttribute ("flexTune", 0.25);
            ms.humanize           = (float) slotXml->getDoubleAttribute ("humanize", 0.0);
            ms.vibratoPreserve    = (float) slotXml->getDoubleAttribute ("vibratoPreserve", 0.0);
            ms.attackAware        = (float) slotXml->getDoubleAttribute ("attackAware", 0.0);
            ms.attackRelease      = (float) slotXml->getDoubleAttribute ("attackRelease", 60.0);
            ms.keySource          = (float) slotXml->getDoubleAttribute ("keySource", 0.0);
            ms.companionGroup     = (float) slotXml->getDoubleAttribute ("companionGroup", 0.0);
            ms.keyDetect          = (float) slotXml->getDoubleAttribute ("keyDetect", 0.0);
            ms.harmonyFollowLead  = slotXml->getDoubleAttribute ("harmonyFollowLead", 1.0) > 0.5;
            ms.key                = slotXml->getIntAttribute ("key", 0);
            ms.scale              = slotXml->getIntAttribute ("scale", 0);
            ms.harmonyType        = slotXml->getIntAttribute ("harmonyType", 0);
            ms.harmonyTone        = slotXml->getIntAttribute ("harmonyTone", 0);
            ms.harmonyShiftedVoices = slotXml->getIntAttribute ("shiftedVoices", 1);
            ms.harmonyFormant     = (float) slotXml->getDoubleAttribute ("harmonyFormant", 0.5);
            ms.harmonyAttack      = (float) slotXml->getDoubleAttribute ("harmonyAttack", 0.1137);
        ms.harmonyGainMatch   = slotXml->getBoolAttribute ("harmonyGainMatch", false);
        {
            // Default "101011010101" is C major, the APVTS default for custom0..custom11.
            const juce::String notes = slotXml->getStringAttribute ("customNotes", "101011010101");
            for (int i = 0; i < 12; ++i)
                ms.customNote[i] = notes[i] == '1';
        }
            ms.voiceType          = slotXml->getIntAttribute ("voiceType", 0);
            ms.latencyMode        = slotXml->getIntAttribute ("latencyMode", 1);
            // Legacy 12 ms "Low Latency" slot -> Fast: the mode is no longer in
            // the menu (see setStateInformation for the same migration).
            if (ms.latencyMode == 1)
                ms.latencyMode = 0;
            ms.editorMeasures     = slotXml->getIntAttribute ("editorMeasures", 8);
            ms.formantEnable      = slotXml->getBoolAttribute ("formantEnable", false);
            ms.bypass             = slotXml->getBoolAttribute ("bypass", false);
            ms.harmonyEnable      = slotXml->getBoolAttribute ("harmonyEnable", false);
            ms.harmonyUseVoice    = slotXml->getBoolAttribute ("useVoice", false);
            ms.outputTrim         = (float) slotXml->getDoubleAttribute ("outputTrim", 0.5);
            ms.limiterEnable      = slotXml->getBoolAttribute ("limiterEnable", true);
            ms.correctionMode     = slotXml->getBoolAttribute ("correctionMode", false);
            ms.hardTune           = slotXml->getBoolAttribute ("hardTune", false);
            ms.noiseGateEnable    = slotXml->getBoolAttribute ("noiseGateEnable", false);
            ms.noiseGateThreshold = (float) slotXml->getDoubleAttribute ("noiseGateThreshold", 0.375);
            ms.upwardCompEnable   = slotXml->getBoolAttribute ("upwardCompEnable", false);
            ms.upwardCompAmount   = (float) slotXml->getDoubleAttribute ("upwardCompAmount", 0.25);
            // De-esser attributes: absent in a state saved before #130 stored them, which reads
            // back as off at the parameter default (intensity 0.65).
            ms.deessEnable        = slotXml->getBoolAttribute ("deessEnable", false);
            ms.deessAmount        = (float) slotXml->getDoubleAttribute ("deessAmount", 0.65);
            // Saturation attributes: absent in a state saved before this stage existed, which
            // reads back as off at the parameter defaults (drive 0.35, warmth 0.40).
            ms.saturationEnable   = slotXml->getBoolAttribute ("saturationEnable", false);
            ms.saturationDrive    = (float) slotXml->getDoubleAttribute ("saturationDrive", 0.35);
            ms.saturationTone     = (float) slotXml->getDoubleAttribute ("saturationTone", 0.40);
            // Delay attributes: absent in a state saved before the lead delay existed, which
            // reads back as off at the parameter defaults (mix 0.30, feedback 0.35, 1/4).
            ms.delayEnable        = slotXml->getBoolAttribute ("delayEnable", false);
            ms.delayDivision      = slotXml->getIntAttribute ("delayDivision", 2);
            ms.delayMix           = (float) slotXml->getDoubleAttribute ("delayMix", 0.30);
            ms.delayFeedback      = (float) slotXml->getDoubleAttribute ("delayFeedback", 0.35);
            // Restore the pitch curve (absent in pre-curve states -> empty curve).
            auto* curveXml = slotXml->getChildByName ("PITCH_CURVE");
            if (curveXml != nullptr)
                ms.curve.fromXml (*curveXml);
            setAbSlotMorphState (slot, std::move (ms));
        }
        // Restore the pitch curve if present in the XML.
        if (pitchCurve != nullptr)
        {
            auto* curveXml = getLastChild ("PITCH_CURVE");
            if (curveXml != nullptr)
            {
                pitchCurve->fromXml (*curveXml);
                pendingCurveRestore.store (true);
            }
        }

        // Strip the plugin-level children so they do not accumulate inside
        // parameters.state on successive save/load cycles.
        for (auto* e = xmlState->getFirstChildElement(); e != nullptr;)
        {
            auto* next = e->getNextElement();
            if (e->hasTagName ("AB_A") || e->hasTagName ("AB_B") || e->hasTagName ("PITCH_CURVE"))
                xmlState->removeChildElement (e, true);
            e = next;
        }

        advancedExpandedState = xmlState->getBoolAttribute ("advancedExpanded", false);
        // Every expansible frame starts FOLDED, including a state file written before the fold existed: a missing
        // attribute is not evidence of an opening the user chose, and the row at minimum width fits folded. The
        // knobs are one chevron click away, so nothing is lost - only the initial width of the bottom band.
        harmonyExpandedState = xmlState->getBoolAttribute ("harmonyExpanded", false);
        reverbExpandedState = xmlState->getBoolAttribute ("reverbExpanded", false);
        // Effects: absent reads back folded, i.e. the two-column rack, so a state saved before the fold was
        // remembered opens exactly as it used to. Once the user opens it, the Delay column comes back open.
        effectsExpandedState = xmlState->getBoolAttribute ("effectsExpanded", false);
        abActiveSlot = juce::jlimit (0, 1, xmlState->getIntAttribute ("abActiveSlot", 0));
        // Absent reads back 120, the default this register has always had.
        setBpm ((float) xmlState->getDoubleAttribute ("bpm", 120.0));

        // Migration: a state saved before the shared ROOM/HALL buses existed carries the
        // seven insert/gate reverb parameters and none of the new ones. Map them onto the
        // two buses' sends, so an existing project keeps (approximately) the room it was
        // mixed with instead of loading dry. Values in the state tree are DENORMALISED and
        // every reverb parameter is a 0..1 amount, so the conversion is direct; 1.43 is the
        // measured crossfade -> send calibration (the legacy Mix also attenuated the dry
        // voice, a send never does). A state that already has a new id is left untouched.
        {
            auto stateParamValue = [&xmlState] (const juce::String& id, bool& found) -> float
            {
                for (auto* e = xmlState->getFirstChildElement(); e != nullptr; e = e->getNextElement())
                    if (e->hasTagName ("PARAM") && e->getStringAttribute ("id") == id)
                    {
                        found = true;
                        return (float) e->getDoubleAttribute ("value", 0.0);
                    }
                found = false;
                return 0.0f;
            };

            bool hasLegacyMix = false, hasNewHall = false;
            const float legacyMix = stateParamValue ("reverb_mix", hasLegacyMix);
            stateParamValue ("lead_hall_send", hasNewHall);

            if (hasLegacyMix && ! hasNewHall)
            {
                bool found = false;
                const float legacyPadMix   = stateParamValue ("pad_reverb_mix", found);
                const float legacyLeadSend = stateParamValue ("lead_send", found);
                const float legacyPadSend  = stateParamValue ("pad_send", found);
                const float legacyDuck     = stateParamValue ("duck_depth", found);
                bool hasLeadEnable = false, hasPadEnable = false;
                const float legacyLeadEnable = stateParamValue ("reverb_enable", hasLeadEnable);
                const float legacyPadEnable  = stateParamValue ("pad_reverb_enable", hasPadEnable);
                (void) hasLeadEnable; (void) hasPadEnable;

                auto addMigrated = [&xmlState] (const char* id, float value)
                {
                    for (auto* e = xmlState->getFirstChildElement(); e != nullptr; e = e->getNextElement())
                        if (e->hasTagName ("PARAM") && e->getStringAttribute ("id") == id)
                            return;
                    auto* param = xmlState->createNewChildElement ("PARAM");
                    param->setAttribute ("id", id);
                    param->setAttribute ("value", (double) value);
                };

                addMigrated ("lead_reverb_enable",
                             (legacyLeadEnable > 0.5f || legacyLeadSend > 0.0f) ? 1.0f : 0.0f);
                addMigrated ("harmony_reverb_enable",
                             (legacyPadEnable > 0.5f || legacyPadSend > 0.0f) ? 1.0f : 0.0f);
                addMigrated ("lead_room_send", 0.0f);
                addMigrated ("lead_hall_send",
                             juce::jlimit (0.0f, 1.0f, 1.43f * legacyMix + legacyLeadSend));
                addMigrated ("harmony_room_send", 0.0f);
                addMigrated ("harmony_hall_send",
                             juce::jlimit (0.0f, 1.0f, 1.43f * legacyPadMix + legacyPadSend));
                addMigrated ("reverb_duck_depth", juce::jlimit (0.0f, 1.0f, legacyDuck));

                OVT_LOG ("State migration: legacy reverb parameters mapped onto the ROOM/HALL buses");
            }
        }

        // Legacy latency_mode migration (2026-10-23): the UI now exposes three
        // modes (Fast = internal 10 ms, Standard = 20 ms, High quality = 30 ms);
        // the old 12 ms "Low Latency" entry is gone. A state saved with it carries
        // the normalised value 1/3, which no menu entry can display any more, so it
        // is mapped to Fast (index 0) - the two were within 1-2 ms of effective
        // delay anyway. New states can only hold 0, 2/3 or 1.
        {
            juce::ValueTree state = juce::ValueTree::fromXml (*xmlState);
            if (auto child = state.getChildWithProperty ("id", "latency_mode"); child.isValid())
            {
                const float v = (float) child.getProperty ("value", 0.0);
                if (std::abs (v - (1.0f / 3.0f)) < 1.0e-3f)
                    child.setProperty ("value", 0.0, nullptr);
            }
            parameters.replaceState (state);
        }
    }
}
