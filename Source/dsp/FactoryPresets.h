// FactoryPresets.h
// Single source of truth for the built-in (factory) pitch-curve presets.
// Replaces the hardcoded menu items in OpenVoxTunerAudioProcessorEditor so
// both the curve preset menu and the browsable Preset Gallery iterate
// the same list. Each entry carries display metadata (category, description)
// used by the gallery thumbnails.

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
