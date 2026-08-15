// export.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#pragma once

#include "ovtchord/types.h"
#include <string>

namespace ovtchord
{
    // Serialize a ChordResult to a minimal JSON string (no external dependency).
    std::string chordResultToJson (const ChordResult& r);
}
