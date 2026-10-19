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
     * ONE reverb room. The plugin builds TWO instances of this class, one per
     * SHARED BUS, each with its own tuning (see ReverbTuning.h): the short ROOM
     * and the long HALL. Every voice sends to the SAME room instances, exactly
     * like a reverb bus in a DAW: a send ADDS, it never steals the dry path.
     *
     * The bus path is processLeadSend / processHarmonySend / processReturn. Each
     * send has its own amount and its own smoothed ramp, so the lead and the
     * harmony can dose their own voice independently; the lead send passes a
     * one-pole low-cut before entering the room. The return is summed additively
     * into the master before the output stage, multiplied by the sidechain
     * ducker's gain (ReverbDucker) and by kReturnTrim.
     *
     * process() keeps the IEffect insert contract (a dry/wet crossfade on a bus
     * tap). The plugin no longer registers a room as an insert -- both rooms are
     * buses -- but the contract is part of the interface and stays covered by the
     * unit tests.
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
         * Interface hook: feeding the room from a bus tap. Forwards to the lead
         * send slot, which is the one carrying the pre-send low-cut.
         */
        void processSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount) override;

        /**
         * Add the room's return into the master buffer, scaled by the sidechain
         * ducker's gain. The send accumulator is consumed (cleared) by this call,
         * so every block's sends are used exactly once.
         */
        void processReturn (juce::AudioBuffer<float>& masterBuffer, float duckGain) override;

        /** Feed the room from the lead voice (through the pre-send low-cut). */
        void processLeadSend (juce::AudioBuffer<float>& sendBuffer, float sendAmount);

        /** Feed the room from the harmony voices (no pre-send filter). */
        void processHarmonySend (juce::AudioBuffer<float>& sendBuffer, float sendAmount);

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

        /** The room returns BEHIND the voice: a unity send is not a copy of the
            level. Measured on a voice-like signal, the wet of a unity send is ~85 %
            of the source RMS, so summing it raw put +5 dB on top of the dry voice
            and drove the output stage into constant limiting. This trim keeps the
            tail audible while leaving headroom for the trim + limiter to stay open. */
        static constexpr float kReturnTrim = 0.7f;

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
        juce::LinearSmoothedValue<float> harmonySendRamp { 0.0f };
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



