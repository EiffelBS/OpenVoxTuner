// FactoryPresets.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace ovtdsp
{
    /** One factory preset descriptor. */
    struct FactoryPresetInfo
    {
        juce::String id;           // PitchCurve::loadPreset() key
        juce::String displayName; // gallery / menu label
        juce::String category;    // grouping (e.g. "Robotic")
        juce::String description; // short metadata line
    };

    /** Returns the ordered list of factory presets (never empty). */
    const std::vector<FactoryPresetInfo>& getFactoryPresets();
}



