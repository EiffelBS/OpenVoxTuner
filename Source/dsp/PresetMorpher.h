// PresetMorpher.h
// Interpolation engine for morphing between two plugin states.
// Captures snapshots of all interpolable parameters and lerps/steps them when
// the morph slider moves. The pitch curve is NOT crossfaded: the displayed
// curve snaps to the nearest slot's curve, so the morph only blends parameters
// (speed, amount, formant, ...) and never resamples / adds curve points.

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
        float harmonyToneColor = 0.5f;
        float reverbMix = 0.0f;
        float flexTune = 0.0f;
        float humanize = 0.0f;

        // Discrete parameters (step at 50%)
        int key = 0;
        int scale = 0;
        int harmonyType = 3;
        int harmonyTone = 0;
        int harmonyShiftedVoices = 4;
        int latencyMode = 2;
        int editorMeasures = 4;

        // Boolean parameters (step at 50%)
        bool formantEnable = false;
        bool bypass = false;
        bool harmonyEnable = false;
        bool harmonyUseVoice = true;
        bool reverbEnable = false;
        bool noiseGateEnable = false;
        float noiseGateThreshold = 0.667f; // normalized: -40dB in -80..0 range
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
        if (auto* p = params.getParameter ("harmony_tone_color")) s.harmonyToneColor = p->getValue();
        if (auto* p = params.getParameter ("reverb_mix"))         s.reverbMix = p->getValue();
        if (auto* p = params.getParameter ("flex_tune"))          s.flexTune = p->getValue();
        if (auto* p = params.getParameter ("humanize"))           s.humanize = p->getValue();

        // Discrete (use getNormalisedRange + snap to int)
        if (auto* p = params.getParameter ("key"))                      s.key = (int) std::round (p->getValue() * 11.0f);
        if (auto* p = params.getParameter ("scale"))                    s.scale = (int) std::round (p->getValue() * 13.0f);
        if (auto* p = params.getParameter ("harmony_type"))             s.harmonyType = (int) std::round (p->getValue() * 21.0f);
        if (auto* p = params.getParameter ("harmony_tone"))             s.harmonyTone = (int) std::round (p->getValue() * 5.0f);
        if (auto* p = params.getParameter ("harmony_shifted_voices"))   s.harmonyShiftedVoices = (int) std::round (p->getValue() * 3.0f) + 1;
        if (auto* p = params.getParameter ("latency_mode"))             s.latencyMode = (int) std::round (p->getValue() * 3.0f);
        if (auto* p = params.getParameter ("editor_measures"))          s.editorMeasures = (int) std::round (p->getValue() * 31.0f) + 1;

        // Booleans
        if (auto* p = params.getParameter ("formant_enable"))      s.formantEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("bypass"))              s.bypass = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("harmony_enable"))      s.harmonyEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("harmony_use_voice"))   s.harmonyUseVoice = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("reverb_enable"))       s.reverbEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("noise_gate_enable"))   s.noiseGateEnable = p->getValue() > 0.5f;
        if (auto* p = params.getParameter ("noise_gate_threshold")) s.noiseGateThreshold = p->getValue();
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
                        "harmony_tone_color", "reverb_mix", "flex_tune", "humanize",
                        "noise_gate_threshold", "key", "scale", "harmony_type",
                        "harmony_tone", "harmony_shifted_voices", "latency_mode",
                        "editor_measures", "formant_enable", "bypass", "harmony_enable",
                        "harmony_use_voice", "reverb_enable", "noise_gate_enable",
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
        setParam ("harmony_tone_color", source.harmonyToneColor + (target.harmonyToneColor - source.harmonyToneColor) * t);
        setParam ("reverb_mix",         source.reverbMix + (target.reverbMix - source.reverbMix) * t);
        setParam ("flex_tune",          source.flexTune + (target.flexTune - source.flexTune) * t);
        setParam ("humanize",           source.humanize + (target.humanize - source.humanize) * t);
        setParam ("noise_gate_threshold", source.noiseGateThreshold + (target.noiseGateThreshold - source.noiseGateThreshold) * t);

        // Discrete parameters (step at 50%)
        setParam ("key",                    lerpOrStep ((float) source.key / 11.0f, (float) target.key / 11.0f, t));
        setParam ("scale",                  lerpOrStep ((float) source.scale / 13.0f, (float) target.scale / 13.0f, t));
        setParam ("harmony_type",           lerpOrStep ((float) source.harmonyType / 21.0f, (float) target.harmonyType / 21.0f, t));
        setParam ("harmony_tone",           lerpOrStep ((float) source.harmonyTone / 5.0f, (float) target.harmonyTone / 5.0f, t));
        setParam ("harmony_shifted_voices", lerpOrStep ((float) (source.harmonyShiftedVoices - 1) / 3.0f, (float) (target.harmonyShiftedVoices - 1) / 3.0f, t));
        setParam ("latency_mode",           lerpOrStep ((float) source.latencyMode / 3.0f, (float) target.latencyMode / 3.0f, t));
        setParam ("editor_measures",        lerpOrStep ((float) (source.editorMeasures - 1) / 31.0f, (float) (target.editorMeasures - 1) / 31.0f, t));

        // Boolean parameters (step at 50%)
        setParam ("formant_enable",    lerpOrStep (source.formantEnable ? 1.0f : 0.0f, target.formantEnable ? 1.0f : 0.0f, t));
        setParam ("bypass",            lerpOrStep (source.bypass ? 1.0f : 0.0f, target.bypass ? 1.0f : 0.0f, t));
        setParam ("harmony_enable",    lerpOrStep (source.harmonyEnable ? 1.0f : 0.0f, target.harmonyEnable ? 1.0f : 0.0f, t));
        setParam ("harmony_use_voice", lerpOrStep (source.harmonyUseVoice ? 1.0f : 0.0f, target.harmonyUseVoice ? 1.0f : 0.0f, t));
        setParam ("reverb_enable",     lerpOrStep (source.reverbEnable ? 1.0f : 0.0f, target.reverbEnable ? 1.0f : 0.0f, t));
        setParam ("noise_gate_enable", lerpOrStep ((float) source.noiseGateEnable, (float) target.noiseGateEnable, t) > 0.5f ? 1.0f : 0.0f);
        setParam ("correction_mode",   lerpOrStep (source.correctionMode ? 1.0f : 0.0f, target.correctionMode ? 1.0f : 0.0f, t));
    }
}
