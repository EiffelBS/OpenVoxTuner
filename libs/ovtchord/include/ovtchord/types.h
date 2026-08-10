// types.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Shared data types for the ovtchord library. This header is part of the
// public API and must stay free of any plugin / JUCE / UI dependency.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ovtchord
{
    // ------------------------------------------------------------------
    // MIDI
    // ------------------------------------------------------------------

    // A single parsed MIDI 1.0 event.
    struct MidiEvent
    {
        double timestamp = 0.0; // caller-defined unit (ms by convention)
        uint8_t status  = 0;    // raw status byte (0x80..0xEF)
        uint8_t data1   = 0;
        uint8_t data2   = 0;
    };

    // ------------------------------------------------------------------
    // Chord result (common to MIDI and audio)
    // ------------------------------------------------------------------

    // A detected chord.
    struct ChordResult
    {
        int rootPitchClass = -1;        // 0=C .. 11=B, -1 = none
        std::vector<int> pitchClasses;  // full pitch-class set (0..11)
        std::string symbol;             // e.g. "Cmaj7", "Csus4", "G7", "C/E"
        float confidence = 0.0f;        // 0..1
        double timestamp = 0.0;
        bool valid = false;             // true only when stable & confident
    };

    // ------------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------------

    // Short note name for a pitch class (0=C .. 11=B).
    inline const char* pitchClassName (int pc)
    {
        static const char* names[12] = { "C", "C#", "D", "D#", "E", "F",
                                         "F#", "G", "G#", "A", "A#", "B" };
        const int m = ((pc % 12) + 12) % 12;
        return names[m];
    }
}
