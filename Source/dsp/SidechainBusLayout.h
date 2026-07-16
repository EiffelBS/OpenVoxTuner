// SidechainBusLayout.h
// Shared helper for validating the optional Sidechain input bus layout.
// Kept header-only and dependency-light so it can be unit-tested without
// linking the full audio plug-in.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    // Validates the Sidechain input bus (input bus index 1) inside a BusesLayout.
    // The sidechain is optional: it must be either disabled (empty channel set)
    // or mono. Any other configuration (e.g. stereo sidechain) is rejected.
    // The main input/output matching rules are handled by the caller.
    inline bool isSidechainLayoutValid (const juce::AudioProcessor::BusesLayout& layouts)
    {
        if (layouts.inputBuses.size() >= 2)
        {
            const auto sc = layouts.getChannelSet (true, 1);
            if (sc.size() != 0 && sc != juce::AudioChannelSet::mono())
                return false;
        }
        return true;
    }
}
