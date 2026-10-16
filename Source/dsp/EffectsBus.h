// EffectsBus.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include "IEffect.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include <memory>
#include <vector>

namespace ovtdsp
{
    /**
     * One tap point of the vocal chain.
     *
     * The plugin owns three of them, in signal order:
     *   - Lead    : on the corrected lead voice, before any harmony is added.
     *   - Harmony : on the harmony bus, before it is mixed into the output.
     *   - Master  : on the final mixed output, after every bus has been blended.
     *
     * Each bus owns its OWN instances of the effects registered on it, so two
     * buses can run the same effect class with different fixed settings (a short
     * room on the lead, a long wide one on the harmony pad). The enable flag and
     * wet mix are computed by the caller through a wiring callback: the same bus
     * is driven from APVTS parameters inside the plugin, and from fixed values
     * inside a unit test. Nothing here reads parameters, so a bus can be tested
     * without a processor instance.
     */
    class EffectsBus
    {
    public:
        /**
         * Computes (enabled, wetMix) for one effect from the caller's state, and is
         * allowed to configure the effect instance itself (a knob drives a DSP field).
         * Return false to skip this effect for this block.
         */
        using Wiring = std::function<bool (IEffect& effect, bool& enabled, float& wetMix)>;

        void addEffect (std::unique_ptr<IEffect> effect)
        {
            if (effect != nullptr)
                effects.push_back (std::move (effect));
        }

        void prepare (double sampleRate, int maxBlockSize)
        {
            for (auto& e : effects)
                if (e != nullptr)
                    e->prepare (sampleRate, maxBlockSize);
        }

        void reset()
        {
            for (auto& e : effects)
                if (e != nullptr)
                    e->reset();
        }

        /** Runs every registered effect, in registration order, on `buffer` in place. */
        void apply (juce::AudioBuffer<float>& buffer, const Wiring& wiring)
        {
            for (auto& e : effects)
            {
                if (e == nullptr)
                    continue;

                bool enabled = false;
                float wetMix = 0.0f;
                if (wiring != nullptr && wiring (*e, enabled, wetMix))
                    e->process (buffer, enabled, wetMix);
            }
        }

        int size() const { return static_cast<int> (effects.size()); }

    private:
        std::vector<std::unique_ptr<IEffect>> effects;
    };
}
