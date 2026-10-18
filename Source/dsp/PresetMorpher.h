// PresetMorpher.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchCurve.h"

namespace ovtdsp
{
    /** Snapshot of all interpolable plugin parameters. */
    struct MorphState
    {
        // Continuous parameters (lerp)
        float speed = 50.0f;
        float amount = 1.0f;
        float formant = 0.0f;
        float harmonyGain = 1.0f;
        float harmonyBlend = 0.5f;
        float harmonyAttack = 0.1137f; // normalized value of "harmony_attack" (35 ms in 1..300 ms range)
        float harmonyFormant = 0.0f;
        float harmonyToneColor = 0.5f;
        float doubleAmount = 0.0f;   // normalized "double_amount" (0..100 %)
        bool doubleEnable = false;   // "double_enable": power switch, amount is kept
        float reverbMix = 0.0f;      // lead reverb wet mix ("reverb_mix")
        float padReverbMix = 0.30f;  // harmony-bus (pad) reverb wet mix ("pad_reverb_mix")
        float leadSend = 0.0f;       // amount of the corrected lead sent to the shared room ("lead_send")
        float padSend = 0.0f;        // amount of the blended harmony sent to the shared room ("pad_send")
        float duckDepth = 0.50f;     // how far the shared room follows the noise gate ("duck_depth")
        float flexTune = 0.0f;
        float humanize = 0.0f;
        float vibratoPreserve = 0.0f;
        float attackAware = 0.0f;
        float attackRelease = 60.0f;
        float keySource = 0.0f;
        float companionGroup = 0.0f;
        float keyDetect = 0.0f;

        // Discrete parameters (step at 50%)
        int key = 0;
        int scale = 0;
        int harmonyType = 3;
        int harmonyTone = 0;
        int harmonyShiftedVoices = 4;
        int latencyMode = 2;
        int editorMeasures = 4;
        int voiceType = 0; // 0=Universal, 1=Bass, 2=Baritone, 3=Tenor, 4=Alto, 5=Soprano

        // Boolean parameters (step at 50%)
        bool formantEnable = false;
        bool bypass = false;
        bool harmonyEnable = false;
        bool harmonyUseVoice = true;
        bool harmonyFollowLead = true;
        bool reverbEnable = false;   // lead reverb power switch
        bool padReverbEnable = false; // pad reverb power switch, mix is kept
        bool noiseGateEnable = false;
        float noiseGateThreshold = 0.375f; // normalized: -50dB in -80..0 range
        bool upwardCompEnable = false;
        float upwardCompAmount = 0.25f; // 0..1 upward compression amount
        bool deessEnable = false;       // "deess_enable": power switch, knob is kept
        float deessAmount = 0.65f;      // intensity 0..1: -38 x a dB threshold, 19 x a dB depth
        bool saturationEnable = false;  // "saturation_enable": master drive power switch
        float saturationDrive = 0.35f;  // "saturation_drive": soft clipper strength, 0 = transparent bypass
        float saturationTone = 0.40f;   // "saturation_tone": warmth, 8 kHz bright .. 5 kHz warm
        // The lead delay is morphed too: the power switch is stepped like its siblings, the
        // echo level and the feedback lerp, and the division stays a discrete grid so a morph
        // can never land between two note values (a ramp between grid points would smear the
        // ping-pong rhythm instead of moving it).
        bool delayEnable = false;       // "delay_enable": power switch, knobs are kept
        int delayDivision = 2;          // "delay_division": combo index, 2 = 1/4
        float delayMix = 0.30f;         // "delay_mix": echo level, 0 = no echo at all
        float delayFeedback = 0.35f;    // "delay_feedback": repeats, low-cut in the loop
        float outputTrim = 0.5f;        // normalized "output_trim" (-12..+12 dB): 0.5 = 0 dB
        bool limiterEnable = true;      // "limiter_enable": master peak limiter on/off
        bool correctionMode = false; // false = Modern

        // PitchCurve
        PitchCurve curve;

        // Preset name (for display)
        juce::String name;
    };

    /** Captures all parameters from an APVTS and a PitchCurve into a MorphState. */
    inline MorphState captureState (juce::AudioProcessorValueTreeState& params,
                                    const ovtdsp::PitchCurve& curve,
                                    const juce::String& presetName = "Current")
    {
        MorphState s;
        s.name = presetName;

        // Continuous
        if (auto* p = params.getParameter ("speed"))              s.speed = p->getValue();
        if (auto* p = params.getParameter ("amount"))             s.amount = p->getValue();
        if (auto* p = params.getParameter ("formant"))            s.formant = p->getValue();
        if (auto* p = params.getParameter ("harmony_gain"))       s.harmonyGain = p->getValue();
        if (auto* p = params.getParameter ("harmony_blend"))      s.harmonyBlend = p->getValue();
        if (auto* p = params.getParameter ("harmony_attack"))     s.harmonyAttack = p->getValue();
        if (auto* p = params.getParameter ("harmony_formant"))    s.harmonyFormant = p->getValue();
        if (auto* p = params.getParameter ("harmony_tone_color")) s.harmonyToneColor = p->getValue();
        if (auto* p = params.getParameter ("double_amount"))      s.doubleAmount = p->getValue();
        if (auto* p = params.getParameter ("double_enable"))      s.doubleEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("reverb_mix"))         s.reverbMix = p->getValue();
        if (auto* p = params.getParameter ("pad_reverb_mix"))     s.padReverbMix = p->getValue();
        if (auto* p = params.getParameter ("lead_send"))          s.leadSend = p->getValue();
        if (auto* p = params.getParameter ("pad_send"))           s.padSend = p->getValue();
        if (auto* p = params.getParameter ("duck_depth"))         s.duckDepth = p->getValue();
        if (auto* p = params.getParameter ("flex_tune"))          s.flexTune = p->getValue();
        if (auto* p = params.getParameter ("humanize"))           s.humanize = p->getValue();
        if (auto* p = params.getParameter ("vibrato_preserve"))   s.vibratoPreserve = p->getValue();
        if (auto* p = params.getParameter ("attack_aware"))       s.attackAware = p->getValue();
        if (auto* p = params.getParameter ("attack_release"))    s.attackRelease = p->getValue();
        if (auto* p = params.getParameter ("key_source"))         s.keySource = p->getValue();
        if (auto* p = params.getParameter ("companion_group"))    s.companionGroup = p->getValue();
        if (auto* p = params.getParameter ("key_detect"))         s.keyDetect = p->getValue();
        if (auto* p = params.getParameter ("harmony_follow_lead")) s.harmonyFollowLead = p->getValue() > 0.5f;

        // Discrete (use getNormalisedRange + snap to int)
        if (auto* p = params.getParameter ("key"))                      s.key = (int) std::round (p->getValue() * 11.0f);
        if (auto* p = params.getParameter ("scale"))                    s.scale = (int) std::round (p->getValue() * 13.0f);
        if (auto* p = params.getParameter ("harmony_type"))             s.harmonyType = (int) std::round (p->getValue() * 21.0f);
        if (auto* p = params.getParameter ("harmony_tone"))             s.harmonyTone = (int) std::round (p->getValue() * 1.0f);
        if (auto* p = params.getParameter ("harmony_shifted_voices"))   s.harmonyShiftedVoices = (int) std::round (p->getValue() * 3.0f) + 1;
        if (auto* p = params.getParameter ("latency_mode"))             s.latencyMode = (int) std::round (p->getValue() * 3.0f);
        if (auto* p = params.getParameter ("editor_measures"))          s.editorMeasures = (int) std::round (p->getValue() * 31.0f) + 1;
        if (auto* p = params.getParameter ("voice_type"))               s.voiceType = (int) std::round (p->getValue() * 5.0f);

        // Booleans
        if (auto* p = params.getParameter ("formant_enable"))      s.formantEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("bypass"))              s.bypass = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("harmony_enable"))      s.harmonyEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("harmony_use_voice"))   s.harmonyUseVoice = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("reverb_enable"))       s.reverbEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("pad_reverb_enable"))   s.padReverbEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("output_trim"))         s.outputTrim = p->getValue();
        if (auto* p = params.getParameter ("limiter_enable"))      s.limiterEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("noise_gate_enable"))   s.noiseGateEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("noise_gate_threshold")) s.noiseGateThreshold = p->getValue();
        if (auto* p = params.getParameter ("upward_comp_enable"))   s.upwardCompEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("upward_comp_amount"))   s.upwardCompAmount = p->getValue();
        if (auto* p = params.getParameter ("deess_enable"))         s.deessEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("deess_amount"))         s.deessAmount = p->getValue();
        if (auto* p = params.getParameter ("saturation_enable"))    s.saturationEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("saturation_drive"))     s.saturationDrive = p->getValue();
        if (auto* p = params.getParameter ("saturation_tone"))      s.saturationTone = p->getValue();
        if (auto* p = params.getParameter ("delay_enable"))         s.delayEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("delay_mix"))            s.delayMix = p->getValue();
        if (auto* p = params.getParameter ("delay_feedback"))       s.delayFeedback = p->getValue();
        // Discrete (snap to int): five divisions, so the normalised value scales by 4.
        if (auto* p = params.getParameter ("delay_division"))       s.delayDivision = (int) std::round (p->getValue() * 4.0f);
        if (auto* p = params.getParameter ("correction_mode"))     s.correctionMode = p->getValue() > 0.5f;

        // Curve (copy)
        s.curve = curve;

        return s;
    }

    /**
     * Returns the list of all parameter IDs that a morph can drive.
     * Used to detect which parameters are currently controlled externally
     * (e.g. DAW/UI automation) so the morph can avoid fighting them.
     */
    inline juce::StringArray getMorphParameterIds()
    {
        juce::StringArray ids;
        ids.addArray ({ "speed", "amount", "formant", "harmony_gain", "harmony_blend",
                        "harmony_formant", "harmony_tone_color", "double_amount", "double_enable", "reverb_mix", "flex_tune", "humanize",
                        "vibrato_preserve",
                        "attack_aware", "attack_release",
                        "key_source", "companion_group", "key_detect",
                        "noise_gate_threshold", "key", "scale", "harmony_type",
                        "harmony_tone", "harmony_shifted_voices", "latency_mode",
                        "editor_measures", "voice_type", "formant_enable", "bypass", "harmony_enable",
                        "harmony_use_voice", "harmony_follow_lead", "reverb_enable",
                        "pad_reverb_mix", "pad_reverb_enable",
                        "lead_send", "pad_send", "duck_depth",
                        "noise_gate_enable", "upward_comp_enable", "upward_comp_amount",
                        "deess_amount",
                        "deess_enable",
                        "saturation_drive", "saturation_tone",
                        "saturation_enable",
                        "delay_mix", "delay_feedback",
                        "delay_enable", "delay_division",
                        "output_trim", "limiter_enable",
                        "correction_mode" });
        return ids;
    }

    /**
     * Applies an interpolated state to the processor parameters.
     * Continuous parameters are lerped, discrete and boolean parameters
     * step at the 50% threshold.
     *
     * @param params      The APVTS to modify
     * @param source      Source morph state
     * @param target      Target morph state
     * @param morphAmount Interpolation amount (0 = source, 1 = target)
     * @param exclude     Optional list of parameter IDs to skip. Use this to
     *                    avoid overwriting parameters that are being driven by
     *                    external automation (e.g. DAW lanes for speed/amount
     *                    running concurrently with a morph automation).
     */
    inline void applyInterpolatedState (juce::AudioProcessorValueTreeState& params,
                                        const MorphState& source,
                                        const MorphState& target,
                                        float morphAmount,
                                        const juce::StringArray* exclude = nullptr)
    {
        const float t = morphAmount; // 0.0 = source, 1.0 = target

        // Helper lambda to set a parameter value (normalized 0..1).
        // Skips any parameter present in the exclusion list.
        auto setParam = [&params, exclude] (const juce::String& id, float normValue)
        {
            if (exclude != nullptr && exclude->contains (id))
                return;
            if (auto* p = params.getParameter (id))
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normValue));
        };

        // Helper for discrete params: step at t >= 0.5
        auto lerpOrStep = [] (float src, float tgt, float t) -> float
        {
            return t < 0.5f ? src : tgt;
        };

        // Continuous parameters (lerp)
        setParam ("speed",              source.speed + (target.speed - source.speed) * t);
        setParam ("amount",             source.amount + (target.amount - source.amount) * t);
        setParam ("formant",            source.formant + (target.formant - source.formant) * t);
        setParam ("harmony_gain",       source.harmonyGain + (target.harmonyGain - source.harmonyGain) * t);
        setParam ("harmony_blend",      source.harmonyBlend + (target.harmonyBlend - source.harmonyBlend) * t);
        setParam ("harmony_formant",    source.harmonyFormant + (target.harmonyFormant - source.harmonyFormant) * t);
        setParam ("harmony_tone_color", source.harmonyToneColor + (target.harmonyToneColor - source.harmonyToneColor) * t);
        setParam ("double_amount",      source.doubleAmount + (target.doubleAmount - source.doubleAmount) * t);
        setParam ("reverb_mix",         source.reverbMix + (target.reverbMix - source.reverbMix) * t);
        setParam ("pad_reverb_mix",     source.padReverbMix + (target.padReverbMix - source.padReverbMix) * t);
        // Send amounts are lerped like a wet mix: a morph moves the room from one bus
        // balance to another continuously, and duck depth lerps so a morph can take the
        // room from following the gate to standing alone.
        setParam ("lead_send",          source.leadSend + (target.leadSend - source.leadSend) * t);
        setParam ("pad_send",           source.padSend + (target.padSend - source.padSend) * t);
        setParam ("duck_depth",         source.duckDepth + (target.duckDepth - source.duckDepth) * t);
        setParam ("flex_tune",          source.flexTune + (target.flexTune - source.flexTune) * t);
        setParam ("humanize",           source.humanize + (target.humanize - source.humanize) * t);
        setParam ("vibrato_preserve",   source.vibratoPreserve + (target.vibratoPreserve - source.vibratoPreserve) * t);
        setParam ("attack_aware",       source.attackAware + (target.attackAware - source.attackAware) * t);
        setParam ("attack_release",     source.attackRelease + (target.attackRelease - source.attackRelease) * t);
        setParam ("key_source",         source.keySource + (target.keySource - source.keySource) * t);
        setParam ("companion_group",    source.companionGroup + (target.companionGroup - source.companionGroup) * t);
        setParam ("key_detect",         lerpOrStep (source.keyDetect, target.keyDetect, t));
        setParam ("noise_gate_threshold", source.noiseGateThreshold + (target.noiseGateThreshold - source.noiseGateThreshold) * t);
        setParam ("upward_comp_amount", source.upwardCompAmount + (target.upwardCompAmount - source.upwardCompAmount) * t);
        setParam ("deess_amount",        source.deessAmount + (target.deessAmount - source.deessAmount) * t);
        // Saturation drive and warmth are lerped like a wet mix: a morph takes the master
        // stage from one drive level to another continuously. Drive 0 is transparent, so a
        // morph ending at zero fades the stage out through its own bypass ramp.
        setParam ("saturation_drive",    source.saturationDrive + (target.saturationDrive - source.saturationDrive) * t);
        setParam ("saturation_tone",     source.saturationTone + (target.saturationTone - source.saturationTone) * t);
        // Echo level and feedback lerp like the other continuous effect controls.
        setParam ("delay_mix",           source.delayMix + (target.delayMix - source.delayMix) * t);
        setParam ("delay_feedback",      source.delayFeedback + (target.delayFeedback - source.delayFeedback) * t);
        setParam ("output_trim",         source.outputTrim + (target.outputTrim - source.outputTrim) * t);

        // Discrete parameters (step at 50%)
        setParam ("key",                    lerpOrStep ((float) source.key / 11.0f, (float) target.key / 11.0f, t));
        setParam ("scale",                  lerpOrStep ((float) source.scale / 13.0f, (float) target.scale / 13.0f, t));
        setParam ("harmony_type",           lerpOrStep ((float) source.harmonyType / 21.0f, (float) target.harmonyType / 21.0f, t));
        setParam ("harmony_tone",           lerpOrStep ((float) source.harmonyTone / 1.0f, (float) target.harmonyTone / 1.0f, t));
        setParam ("harmony_shifted_voices", lerpOrStep ((float) (source.harmonyShiftedVoices - 1) / 3.0f, (float) (target.harmonyShiftedVoices - 1) / 3.0f, t));
        setParam ("latency_mode",           lerpOrStep ((float) source.latencyMode / 3.0f, (float) target.latencyMode / 3.0f, t));
        setParam ("editor_measures",        lerpOrStep ((float) (source.editorMeasures - 1) / 31.0f, (float) (target.editorMeasures - 1) / 31.0f, t));
        setParam ("voice_type",             lerpOrStep ((float) source.voiceType / 5.0f, (float) target.voiceType / 5.0f, t));
        setParam ("delay_division",         lerpOrStep ((float) source.delayDivision / 4.0f, (float) target.delayDivision / 4.0f, t));

        // Boolean parameters (step at 50%)
        setParam ("formant_enable",    lerpOrStep (source.formantEnable ? 1.0f : 0.0f, target.formantEnable ? 1.0f : 0.0f, t));
        setParam ("bypass",            lerpOrStep (source.bypass ? 1.0f : 0.0f, target.bypass ? 1.0f : 0.0f, t));
        setParam ("harmony_enable",    lerpOrStep (source.harmonyEnable ? 1.0f : 0.0f, target.harmonyEnable ? 1.0f : 0.0f, t));
        setParam ("harmony_use_voice", lerpOrStep (source.harmonyUseVoice ? 1.0f : 0.0f, target.harmonyUseVoice ? 1.0f : 0.0f, t));
        setParam ("harmony_follow_lead", lerpOrStep (source.harmonyFollowLead ? 1.0f : 0.0f, target.harmonyFollowLead ? 1.0f : 0.0f, t));
        setParam ("reverb_enable",     lerpOrStep (source.reverbEnable ? 1.0f : 0.0f, target.reverbEnable ? 1.0f : 0.0f, t));
        setParam ("pad_reverb_enable", lerpOrStep (source.padReverbEnable ? 1.0f : 0.0f, target.padReverbEnable ? 1.0f : 0.0f, t));
        setParam ("double_enable",     lerpOrStep (source.doubleEnable ? 1.0f : 0.0f, target.doubleEnable ? 1.0f : 0.0f, t));
        setParam ("noise_gate_enable", lerpOrStep ((float) source.noiseGateEnable, (float) target.noiseGateEnable, t) > 0.5f ? 1.0f : 0.0f);
        setParam ("upward_comp_enable", lerpOrStep (source.upwardCompEnable ? 1.0f : 0.0f, target.upwardCompEnable ? 1.0f : 0.0f, t));
        setParam ("deess_enable",      lerpOrStep (source.deessEnable ? 1.0f : 0.0f, target.deessEnable ? 1.0f : 0.0f, t));
        setParam ("saturation_enable", lerpOrStep (source.saturationEnable ? 1.0f : 0.0f, target.saturationEnable ? 1.0f : 0.0f, t));
        setParam ("delay_enable",      lerpOrStep (source.delayEnable ? 1.0f : 0.0f, target.delayEnable ? 1.0f : 0.0f, t));
        setParam ("limiter_enable",    lerpOrStep (source.limiterEnable ? 1.0f : 0.0f, target.limiterEnable ? 1.0f : 0.0f, t));
        setParam ("correction_mode",   lerpOrStep (source.correctionMode ? 1.0f : 0.0f, target.correctionMode ? 1.0f : 0.0f, t));
    }
}



