// chord_templates.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "core/chord_templates.h"

namespace ovtchord
{
    const std::vector<ChordTemplate>& chordTemplates()
    {
        static const std::vector<ChordTemplate> tpls = {
            { "",       { 0, 4, 7 } },
            { "m",      { 0, 3, 7 } },
            { "7",      { 0, 4, 7, 10 } },
            { "maj7",   { 0, 4, 7, 11 } },
            { "m7",     { 0, 3, 7, 10 } },
            { "m7b5",   { 0, 3, 6, 10 } },
            { "dim",    { 0, 3, 6 } },
            { "dim7",   { 0, 3, 6, 9 } },
            { "aug",    { 0, 4, 8 } },
            { "aug7",   { 0, 4, 8, 10 } },
            { "sus2",   { 0, 2, 7 } },
            { "sus4",   { 0, 5, 7 } },
            { "7sus4",  { 0, 5, 7, 10 } },
            { "6",      { 0, 4, 7, 9 } },
            { "m6",     { 0, 3, 7, 9 } },
            { "maj9",   { 0, 4, 7, 11, 14 } },
            { "m9",     { 0, 3, 7, 10, 14 } },
            { "9",      { 0, 4, 7, 10, 14 } },
            { "5",      { 0, 7 } },
        };
        return tpls;
    }
}
