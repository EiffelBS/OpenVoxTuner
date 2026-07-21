// SidechainBusLayout.h
// Shared helper for validating the optional Sidechain input bus layout.
// Kept header-only and dependency-light so it can be unit-tested without
// linking the full audio plug-in.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    // Validates the Sidechain input bus (input bus index 1) inside a BusesLayout.
    // The sidechain is optional: it must be either disabled (empty channel set),
    // mono, or stereo. Only the first channel is used for pitch detection, so
    // stereo is accepted for compatibility with Logic AU negotiation.
    inline bool isSidechainLayoutValid (const juce::AudioProcessor::BusesLayout& layouts)
    {
        if (layouts.inputBuses.size() >= 2)
        {
            const auto sc = layouts.getChannelSet (true, 1);
            if (sc != juce::AudioChannelSet::disabled()
             && sc != juce::AudioChannelSet::mono()
             && sc != juce::AudioChannelSet::stereo())
                return false;
        }
        return true;
    }
}
