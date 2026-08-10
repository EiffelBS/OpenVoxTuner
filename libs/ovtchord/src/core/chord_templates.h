// chord_templates.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Internal module: the chord template table. Not part of the public API.

#pragma once

#include <string>
#include <vector>

namespace ovtchord
{
    struct ChordTemplate
    {
        const char* name;            // suffix, e.g. "", "m", "7", "maj7"
        std::vector<int> intervals;  // relative to root, sorted ascending
    };

    // The list of supported chord templates (see plan §1.2).
    const std::vector<ChordTemplate>& chordTemplates();
}
