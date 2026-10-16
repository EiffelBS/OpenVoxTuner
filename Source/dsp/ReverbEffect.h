// ReverbEffect.h
// ReverbEffect.h
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include "IEffect.h"
#include "OnePoleLowCut.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace ovtdsp
{
    /**
     * A reverb room, used in two ways.
     *
     * 1. As a normal insert on an effects bus (process()): the tapped signal is
     *    copied into a scratch buffer, run through the room, and ADDED to the
     *    tapped signal scaled by wetMix. The dry path is never attenuated -- a
     *    mix of 0 leaves the input bit-identical, a mix of 1 adds the full room.
     * 2. As the SHARED send room (processLeadSend / processPadSend /
     *    processReturn): one instance fed by two parallel sends, so the lead and
     *    the harmony voices hear the SAME room. Each send has its own amount, its
     *    own smoothed ramp, and the lead send passes a one-pole low-cut before
     *    entering the room. The return is summed additively into the master,
     *    multiplied by a ducking envelope driven by the noise gate.
     *
     * Every buffer this class touches is sized in prepare(); process() and the
     * send/return path allocate nothing at block time.
     */
    class ReverbEffect : public IEffect
    {
    public:
        ReverbEffect();
        ~ReverbEffect() override = default;

        // IEffect interface
        juce::String getId() const override   { return "reverb"; }
        juce::String getName() const override { return "Reverb"; }
        void prepare (double sampleRate, int maximumBlockSize) override;
        void reset() override;
        void process (juce::AudioBuffer<float>& buffer, bool enabled, float wetMix) override;

        /**
         * Interface hook: feeding the shared room. Forwards to the lead send slot,
         * which is the one carrying the pre-send low-cut.
         */
        void processSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount) override;

        /**
         * Add the room's return into the master buffer, scaled by the ducking
         * envelope. The send accumulator is consumed (cleared) by this call, so
         * every block's sends are used exactly once.
         */
        void processReturn (juce::AudioBuffer<float>& masterBuffer, float duckGain) override;

        /** Feed the room from the lead voice (through the pre-send low-cut). */
        void processLeadSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount);

        /** Feed the room from the harmony voices (no pre-send filter). */
        void processPadSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount);

        /** Adjust reverb room size (0.0 .. 1.0). */
        void setRoomSize (float size);

        /** Adjust reverb damping (0.0 .. 1.0). */
        void setDamping (float damping);

        /** Adjust stereo width (0.0 = centred, 1.0 = fully wide). */
        void setStereoWidth (float width);

        /** Scratch capacity prepared for this instance (channels, samples). */
        int getScratchSamples() const { return sendAccum.getNumSamples(); }

    private:
        /** Pre-send low-cut on the lead send: 150 Hz, 12 dB/octave. Below the
            fundamental range of the voice, so it removes breath and mic rumble
            without touching the body of the singing. */
        static constexpr float kLeadSendLowCutHz = 150.0f;

        juce::Reverb reverb;
        juce::Reverb::Parameters params;
        double sampleRate = 44100.0;
        bool prepared = false;
        // Smoothed master enable gain. Toggling the reverb on/off ramps this
        // (instead of hard-cutting the wet path), so enabling/disabling the
        // effect produces no click -- behaviour matches moving the mix slider.
        float masterGain = 0.0f;
        float masterTarget = 0.0f;
        float masterCoeff = 0.0f;

        // Wet path scratch, sized in prepare() and reused every block.
        juce::AudioBuffer<float> wetScratch;
        // The shared send line: two channels (the room is stereo), sized in prepare().
        juce::AudioBuffer<float> sendAccum;
        // Per-sample send ramps, one per tap so the two amounts stay independent.
        juce::LinearSmoothedValue<float> leadSendRamp { 0.0f };
        juce::LinearSmoothedValue<float> padSendRamp { 0.0f };
        // Ducking envelope for the return: 1.0 = room untouched, 0.0 = fully ducked.
        juce::LinearSmoothedValue<float> duckRamp { 1.0f };
        // Ramp length in samples shared by the three ramps (25 ms), set in prepare().
        int rampSamples = 1;
        // Peak of the last return, so a silent send line with a decayed tail skips the room.
        float tailPeak = 0.0f;
        // Pre-send low-cut on the lead send, one pole per channel.
        OnePoleLowCut leadLowCut[2];
    };
}



