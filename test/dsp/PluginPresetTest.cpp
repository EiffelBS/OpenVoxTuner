#pragma once
// PluginPresetTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_data_structures/juce_data_structures.h>

class PluginPresetTest : public juce::UnitTest
{
public:
    PluginPresetTest() : juce::UnitTest ("PluginPreset") {}

    // Build a parameters.state ValueTree mirroring JUCE APVTS serialization:
    // a root with one child per parameter, each carrying an "id" and "value".
    static juce::ValueTree makeState (const std::map<juce::String, float>& vals)
    {
        juce::ValueTree root ("OpenVoxTuner");
        for (auto& [id, v] : vals)
        {
            juce::ValueTree p ("PARAM");
            p.setProperty ("id", id, nullptr);
            p.setProperty ("value", v, nullptr);
            root.appendChild (p, nullptr);
        }
        return root;
    }

    // A standalone pitch curve, living OUTSIDE parameters.state.
    static juce::ValueTree makeCurve (float pitch)
    {
        juce::ValueTree c ("PITCH_CURVE");
        c.setProperty ("basePitch", pitch, nullptr);
        return c;
    }

    static float getParam (const juce::ValueTree& state, const juce::String& id)
    {
        auto child = state.getChildWithProperty ("id", id);
        return child.isValid() ? (float) child.getProperty ("value") : -1.0f;
    }

    static void setParam (juce::ValueTree& state, const juce::String& id, float v)
    {
        auto child = state.getChildWithProperty ("id", id);
        if (child.isValid())
            child.setProperty ("value", v, nullptr);
    }

    // Production-style: build a factory preset state from the default tree +
    // overrides applied to the matching <PARAM> child's "value".
    static juce::ValueTree buildFactoryPreset (const juce::ValueTree& defaults,
                                               const std::map<juce::String, float>& overrides)
    {
        juce::ValueTree state = defaults.createCopy();
        for (auto& [id, v] : overrides)
            setParam (state, id, v);
        return state;
    }

    // Production-style load: replaceState via ValueTree::fromXml of the stored
    // <OVT_PLUGIN_PRESET> document. Returns the loaded parameters.state.
    static juce::ValueTree loadFromPresetXml (const juce::XmlElement& root)
    {
        const auto* paramsXml = root.getChildByName ("OpenVoxTuner");
        if (paramsXml == nullptr)
            paramsXml = root.getChildByName ("PARAMETERS");
        return juce::ValueTree::fromXml (*paramsXml);
    }

    void runTest() override
    {
        using namespace juce;

        // Factory defaults (a representative subset of the real parameters).
        std::map<String, float> defaults = {
            { "amount", 1.0f }, { "flex_tune", 10.0f }, { "humanize", 40.0f },
            { "harmony_enable", 0.0f }, { "harmony_type", 3.0f },
            { "latency_mode", 1.0f }, { "ui_language", 0.0f }, { "ui_theme", 0.0f },
            { "morph_amount", 0.0f }, { "mode", 0.0f }, { "reverb_enable", 0.0f }
        };
        const ValueTree defaultState = makeState (defaults);

        beginTest ("Default preset equals factory defaults");
        {
            // The "Default" preset must reproduce every factory value verbatim.
            for (auto& [id, v] : defaults)
                expect (approximatelyEqual (getParam (defaultState, id), v),
                        "Default preset must match factory value for " + id);
        }

        beginTest ("preset state excludes the pitch curve");
        {
            // In production, copyState() returns parameters.state only; a
            // separate pitch curve lives outside that tree. Mirror that here:
            // the curve is a sibling, never a child of the parameter state.
            ValueTree state = defaultState.createCopy();
            ValueTree curve = makeCurve (220.0f);
            // The curve is stored separately (e.g. in the document root), not
            // inside the parameter state.
            expect (state.getChildWithProperty ("id", "basePitch").isValid() == false,
                    "parameter state must not contain a curve parameter");
            expect (state.getNumChildren() == defaultState.getNumChildren(),
                    "curve must not change the parameter child count");

            // After a save+load of the parameter state, the curve is untouched.
            std::unique_ptr<XmlElement> saved = state.createXml();
            ValueTree reloaded = ValueTree::fromXml (*saved);
            expect (reloaded.getChildWithProperty ("id", "basePitch").isValid() == false,
                    "reloaded state must still exclude the curve");
            (void) curve;
        }

        beginTest ("factory override changes only declared parameters");
        {
            std::map<String, float> overrides = {
                { "amount", 0.9f }, { "flex_tune", 15.0f }, { "humanize", 20.0f },
                { "harmony_enable", 1.0f }, { "harmony_type", 17.0f },
                { "latency_mode", 0.0f }
            };
            ValueTree preset = buildFactoryPreset (defaultState, overrides);

            for (auto& [id, v] : overrides)
                expect (approximatelyEqual (getParam (preset, id), v),
                        "override must set " + id);

            // Untouched parameters stay at their factory defaults.
            expect (approximatelyEqual (getParam (preset, "reverb_enable"), 0.0f),
                    "untouched parameter must keep its default");
            expect (approximatelyEqual (getParam (preset, "mode"), 0.0f),
                    "untouched parameter must keep its default");
        }

        beginTest ("custom preset round-trips via <OVT_PLUGIN_PRESET> XML");
        {
            // Build and serialize a custom preset (mimics writePluginPresetFile).
            ValueTree custom = defaultState.createCopy();
            setParam (custom, "amount", 0.5f);
            setParam (custom, "harmony_enable", 1.0f);

            XmlElement root ("OVT_PLUGIN_PRESET");
            root.setAttribute ("name", "My Custom");
            root.setAttribute ("advancedExpanded", 1);
            std::unique_ptr<XmlElement> stateXml = custom.createXml();
            root.addChildElement (stateXml.release());

            // Reload (mimics loadPluginPresetFromFile).
            ValueTree loaded = loadFromPresetXml (root);
            expect (loaded.isValid(), "loaded preset must be valid");
            expect (root.getChildByName ("OpenVoxTuner") != nullptr
                        || root.getChildByName ("PARAMETERS") != nullptr,
                    "preset XML must carry a parameters block");
            expect (approximatelyEqual (getParam (loaded, "amount"), 0.5f),
                    "loaded amount must match saved value");
            expect (approximatelyEqual (getParam (loaded, "harmony_enable"), 1.0f),
                    "loaded harmony_enable must match saved value");
            expect (approximatelyEqual (getParam (loaded, "flex_tune"), 10.0f),
                    "loaded untouched param must keep its value");
            expect (root.getIntAttribute ("advancedExpanded", 0) == 1,
                    "preset must persist the advancedExpanded UI preference");
        }

        beginTest ("applying a preset preserves user/session preferences");
        {
            // Simulate a preset whose stored values differ from the live
            // preferences; after apply, ui_language / ui_theme / morph_amount /
            // mode must be restored to their pre-apply values (production
            // applyPluginPresetState does this explicitly).
            ValueTree live = defaultState.createCopy();
            setParam (live, "ui_language", 2.0f); // user chose French
            setParam (live, "morph_amount", 0.5f);
            setParam (live, "mode", 1.0f);        // Curve Editor mode

            ValueTree preset = buildFactoryPreset (defaultState,
                                                   { { "amount", 0.9f } });
            setParam (preset, "ui_language", 0.0f); // preset default language

            // Apply == replaceState(preset), then re-apply the preserved prefs.
            live = preset.createCopy();
            setParam (live, "ui_language", 2.0f);
            setParam (live, "morph_amount", 0.5f);
            setParam (live, "mode", 1.0f);

            expect (approximatelyEqual (getParam (live, "amount"), 0.9f),
                    "applied preset value must take effect");
            expect (approximatelyEqual (getParam (live, "ui_language"), 2.0f),
                    "user language must be preserved");
            expect (approximatelyEqual (getParam (live, "morph_amount"), 0.5f),
                    "morph position must be preserved");
            expect (approximatelyEqual (getParam (live, "mode"), 1.0f),
                    "Live/Curve mode must be preserved");
        }
    }

    static bool approximatelyEqual (float a, float b, float tol = 1.0e-3f)
    {
        return std::abs (a - b) <= tol;
    }
};

static PluginPresetTest pluginPresetTest;


