// midi_parser.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "ovtchord/midi_parser.h"

namespace ovtchord
{
    void MidiParser::reset()
    {
        runningStatus = 0;
        haveRunning = false;
        pendingStatus = 0;
        pendingData[0] = pendingData[1] = 0;
        expectData = 0;
        inSysEx = false;
    }

    std::vector<MidiEvent> MidiParser::parse (const uint8_t* data, std::size_t size, double timestampMs)
    {
        std::vector<MidiEvent> out;
        for (std::size_t i = 0; i < size; ++i)
        {
            const uint8_t b = data[i];

            if (b >= 0x80)
            {
                // Status byte.
                if (b == 0xF0) { inSysEx = true; continue; }   // SysEx start
                if (b == 0xF7) { inSysEx = false; continue; }  // SysEx end
                if (b >= 0xF8) continue;                       // real-time, ignore
                if (b >= 0xF1 && b <= 0xF6) continue;          // system common, ignore

                // Channel message (0x80..0xEF).
                inSysEx = false;
                pendingStatus = b;
                runningStatus = b;
                haveRunning = true;
                // Program change (0xC0) and channel pressure (0xD0) carry 1 data byte.
                expectData = ((b & 0xF0) == 0xC0 || (b & 0xF0) == 0xD0) ? 1 : 2;
                pendingData[0] = pendingData[1] = 0;
                continue;
            }

            // Data byte.
            if (inSysEx) continue;

            if (expectData == 0)
            {
                // Running status: reuse the last channel status.
                if (!haveRunning) continue;
                pendingStatus = runningStatus;
                expectData = ((runningStatus & 0xF0) == 0xC0 || (runningStatus & 0xF0) == 0xD0) ? 1 : 2;
                pendingData[0] = pendingData[1] = 0;
            }

            pendingData[2 - expectData] = b;
            --expectData;
            if (expectData == 0)
            {
                MidiEvent e;
                e.timestamp = timestampMs;
                e.status = pendingStatus;
                e.data1 = pendingData[0];
                e.data2 = pendingData[1];
                out.push_back (e);
            }
        }
        return out;
    }
}
