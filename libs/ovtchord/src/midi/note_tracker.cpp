// note_tracker.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "midi/note_tracker.h"
#include <algorithm>

namespace ovtchord
{
    void NoteTracker::process (const MidiEvent& e)
    {
        const uint8_t type = e.status & 0xF0;

        if (type == 0x90) // Note On
        {
            const uint8_t note = e.data1;
            const uint8_t vel  = e.data2;
            if (vel == 0)
            {
                notes.erase (note); // velocity 0 == Note Off
            }
            else
            {
                ActiveNote n;
                n.note = note;
                n.velocity = vel;
                n.timestamp = e.timestamp;
                notes[note] = n;
            }
        }
        else if (type == 0x80) // Note Off
        {
            notes.erase (e.data1);
        }
        else if (type == 0xB0) // Control Change
        {
            // All Notes Off (123), All Sound Off (120), Reset All Controllers (121).
            if (e.data1 == 123 || e.data1 == 120 || e.data1 == 121)
                notes.clear();
        }
    }

    std::vector<int> NoteTracker::pitchClassSet (uint8_t minVelocity) const
    {
        std::vector<int> pcs;
        for (const auto& kv : notes)
            if (kv.second.velocity >= minVelocity)
                pcs.push_back (kv.first % 12);
        std::sort (pcs.begin(), pcs.end());
        pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
        return pcs;
    }

    int NoteTracker::lowestNote() const
    {
        if (notes.empty()) return -1;
        return notes.begin()->first;
    }

    void NoteTracker::reset() { notes.clear(); }
}
