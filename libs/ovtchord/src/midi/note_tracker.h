// note_tracker.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: tracks active (held) MIDI notes. Not part of the public API.

#pragma once

#include "ovtchord/types.h"
#include <map>
#include <vector>

namespace ovtchord
{
    class NoteTracker
    {
    public:
        struct ActiveNote
        {
            uint8_t note = 0;
            uint8_t velocity = 0;
            double timestamp = 0.0;
        };

        // Process one parsed MIDI event (Note On/Off, All Notes Off, ...).
        void process (const MidiEvent& e);

        // Currently held notes, keyed by note number (0..127).
        const std::map<uint8_t, ActiveNote>& activeNotes() const { return notes; }

        // Pitch-class set (0..11) of active notes, optionally filtered by a
        // minimum velocity (0..127).
        std::vector<int> pitchClassSet (uint8_t minVelocity = 0) const;

        // Lowest active note (for inversion detection), or -1 if none.
        int lowestNote() const;

        void reset();

    private:
        std::map<uint8_t, ActiveNote> notes;
    };
}
