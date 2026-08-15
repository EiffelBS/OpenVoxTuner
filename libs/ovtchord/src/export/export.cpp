// export.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "ovtchord/export.h"

namespace ovtchord
{
    std::string chordResultToJson (const ChordResult& r)
    {
        std::string s = "{";
        s += "\"valid\":" + std::string (r.valid ? "true" : "false") + ",";
        s += "\"root\":" + std::to_string (r.rootPitchClass) + ",";
        s += "\"symbol\":\"" + r.symbol + "\",";
        s += "\"confidence\":" + std::to_string (r.confidence) + ",";
        s += "\"timestamp\":" + std::to_string (r.timestamp) + ",";
        s += "\"pitchClasses\":[";
        for (std::size_t i = 0; i < r.pitchClasses.size(); ++i)
        {
            if (i) s += ",";
            s += std::to_string (r.pitchClasses[i]);
        }
        s += "]}";
        return s;
    }
}
