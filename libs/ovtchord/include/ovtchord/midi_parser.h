// midi_parser.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#pragma once

#include "ovtchord/types.h"
#include <cstddef>
#include <vector>

namespace ovtchord
{
    // Streaming MIDI 1.0 parser. Handles running status, Note On/Off (velocity
    // 0 = Note Off), and ignores system messages. Produces MidiEvents stamped
    // with the timestamp provided at parse time.
    class MidiParser
    {
    public:
        // Feed raw bytes; returns any complete events parsed. `timestampMs` is
        // stamped onto every event produced from this call.
        std::vector<MidiEvent> parse (const uint8_t* data, std::size_t size, double timestampMs);

        void reset();

    private:
        uint8_t runningStatus = 0; // last channel status (for running status)
        bool haveRunning = false;
        uint8_t pendingStatus = 0; // status awaiting data bytes
        uint8_t pendingData[2] = { 0, 0 };
        int expectData = 0;        // data bytes still expected
        bool inSysEx = false;
    };
}
