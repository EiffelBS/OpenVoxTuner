// IEffect.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    /**
     * Abstract interface for vocal effects.
     *
     * An effect is registered on one of the three tap points of the chain
     * (see ovtdsp::EffectsBus): the corrected lead voice before any harmony is
     * added, the harmony bus before it is mixed into the output, or the master
     * output after every bus has been blended. Each bus owns its OWN instances,
     * so the same class can run twice with different fixed settings (a short room
     * on the lead, a long wide one on the harmony pad). Processing is in-place on
     * the tapped buffer; the effect never reads parameters, the caller's wiring
     * computes `enabled` and `wetMix` from them.
     *
     * Two extra hooks exist for the SHARED send room (see processSend /
     * processReturn below). They are defaulted to no-ops, so an effect that does
     * not participate in the shared room - Noise Gate, Upward Compressor,
     * De-Esser - keeps compiling and behaving exactly as before. Only the reverb
     * overrides them.
     */
    class IEffect
    {
    public:
        virtual ~IEffect() = default;

        /** Unique identifier for this effect type (e.g. "reverb"). */
        virtual juce::String getId() const = 0;

        /** Human-readable name (e.g. "Reverb"). */
        virtual juce::String getName() const = 0;

        /** Prepare with sample rate and max block size. */
        virtual void prepare (double sampleRate, int maximumBlockSize) = 0;

        /** Reset internal state (flush delays, reverb tails, etc.). */
        virtual void reset() = 0;

        /**
         * Apply the effect to the audio buffer.
         * @param buffer      The audio buffer to process (in-place).
         * @param enabled     Whether the effect is currently enabled.
         * @param wetMix      Wet/dry mix ratio (0.0 = dry only, 1.0 = full wet).
         */
        virtual void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) = 0;

        /**
         * Feed the shared send bus from a tapped signal.
         *
         * The caller hands over the buffer it tapped (the processed lead, or the
         * blended harmony voices) and the amount to send. The effect ADDS the
         * scaled contribution to its own send accumulator; it never touches the
         * source buffer, so a send is always parallel: the dry path stays at
         * unity and no latency is declared anywhere.
         *
         * @param sendBuffer  The tapped signal, read-only.
         * @param sendAmount  Send amount (0.0 = nothing sent, 1.0 = full signal).
         *
         * Defaulted to a no-op: only an effect that owns the shared room overrides it.
         */
        virtual void processSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount)
        {
            (void) sendBuffer; (void) sendAmount;
        }

        /**
         * Add the shared room's return into the master output.
         *
         * The wet is summed ADDITIVELY into the master buffer, never replacing it,
         * and multiplied by the ducking envelope the caller computed from the noise
         * gate: a closed gate pulls the room down with the voice, which is the
         * classic vocal ducking. The sum happens before the output trim + limiter,
         * so the additive wet is guarded by the same ceiling as everything else.
         *
         * @param masterBuffer  The fully mixed master buffer (in-place).
         * @param duckGain      Ducking multiplier (1.0 = no ducking, 0.0 = room fully ducked).
         *
         * Defaulted to a no-op, for the same reason as processSend.
         */
        virtual void processReturn (juce::AudioBuffer<float>& masterBuffer, float duckGain)
        {
            (void) masterBuffer; (void) duckGain;
        }
    };
}


