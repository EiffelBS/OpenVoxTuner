// PresetGallery.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "../dsp/FactoryPresets.h"
#include "../dsp/PitchCurve.h"
#include "OVTTheme.h"
#include "OVTFonts.h"

class PresetGallery  : public juce::Component
{
public:
    using LoadFactoryFn   = std::function<void (const juce::String&)>;
    using LoadCustomFn    = std::function<void (const juce::File&)>;
    using DeleteCustomFn  = std::function<void (const juce::File&)>;
    using GetCustomFilesFn = std::function<juce::Array<juce::File>()>;

    PresetGallery (LoadFactoryFn   loadFactory,
                   LoadCustomFn    loadCustom,
                   DeleteCustomFn  deleteCustom,
                   GetCustomFilesFn getCustomFiles);

    // Rebuilds the card grid (factory + custom) and reflows the layout.
    void refresh();

    // Sets the callback for custom preset deletion (used by editor).
    void setOnDeleteCallback (DeleteCustomFn fn)
    {
        onDeleteCustom = std::move (fn);
    }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    // Renders a compact curve thumbnail (green line on the dark editor bg).
    static juce::Image renderCurveThumb (const ovtdsp::PitchCurve& curve, int w, int h);

    // A single preset tile (thumbnail + name + category/description).
    class PresetCard  : public juce::Component
    {
    public:
        PresetCard (const juce::String& name,
                 const juce::String& category,
                 const juce::String& description,
                 const juce::Image& thumb,
                 bool isCustom,
                 std::function<void()> onLoad,
                 std::function<void()> onDelete);

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent& e) override;

    private:
        juce::ImageComponent thumbComp;
        juce::Label nameLabel;
        juce::Label metaLabel;
        juce::TextButton deleteButton;
        bool custom = false;
        std::function<void()> loadCb;
        std::function<void()> deleteCb;
    };

    void layoutContent();

    juce::Viewport viewport;
    juce::Component content;            // holds the cards (+ headers)
    juce::Label factoryHeader;
    juce::Label customHeader;
    juce::Label emptyLabel;            // shown when there are no custom presets
    juce::OwnedArray<PresetCard> cards;

    LoadFactoryFn   onLoadFactory;
    LoadCustomFn    onLoadCustom;
    DeleteCustomFn  onDeleteCustom;
    GetCustomFilesFn getCustomFiles;
};


