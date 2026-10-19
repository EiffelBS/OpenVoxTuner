// ReverbTuning.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

/**
 * The two shared reverb rooms, in ONE place.
 *
 * The plugin has ONE room instance per tuning, each fed by a lead send and a
 * harmony send (the DAW bus model: a send adds, it never steals the dry voice).
 * The tunings below ARE the sound of those two buses, so they are shared by the
 * processor (which configures the instances), the factory presets and the unit
 * tests (which measure the tail each tuning produces). Changing a tuning here
 * changes every one of those at once, on purpose.
 *
 * HALL is the legacy Main room: roomSize 0.70 / damping 0.30 / width 1.0,
 * measured at RT60 ~2.43 s on a voice-like signal. It is the sound the single
 * legacy Mix knob delivered, kept bit for bit.
 *
 * ROOM is the short room that keeps the lead present: measured at RT60 ~1.18 s,
 * just short enough to stop ringing inside a phrase. The pair (short ROOM +
 * long HALL) is the recommended vocal setup the UI offers out of the box.
 */
namespace ovtdsp
{
    struct ReverbRoomTuning
    {
        float roomSize;
        float damping;
        float width;
    };

    inline constexpr ReverbRoomTuning kReverbHallTuning { 0.70f, 0.30f, 1.0f };
    inline constexpr ReverbRoomTuning kReverbRoomTuning { 0.35f, 0.50f, 1.0f };

    /**
     * Default amount sent by each voice to each room ("ready to sing").
     *
     * The Lead and Harmony toggles start OFF, so a fresh session is dry; the
     * knobs still carry these values, which means switching a toggle ON applies a
     * balanced room immediately instead of the silence a default of 0 would give.
     * Calibration: at the 0.7 return trim, 0.45 on HALL adds about +1.9 dB of
     * room behind an untouched dry voice (the old 30 % Mix crossfade left the dry
     * at 70 %, so it is the same reverb-to-voice balance, with the voice intact).
     */
    inline constexpr float kDefaultLeadRoomSend      = 0.15f;
    inline constexpr float kDefaultLeadHallSend      = 0.45f;
    inline constexpr float kDefaultHarmonyRoomSend   = 0.10f;
    inline constexpr float kDefaultHarmonyHallSend   = 0.30f;

    /** Default ducker depth: 0.35 of the 12 dB range, i.e. about -4.2 dB when the
        lead is singing at its reference level. 0 would be no ducking at all. */
    inline constexpr float kDefaultReverbDuckDepth   = 0.35f;
}
