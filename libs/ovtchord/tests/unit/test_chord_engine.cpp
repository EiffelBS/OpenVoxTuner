// test_chord_engine.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "ovtchord/chord_engine.h"
#include "test_harness.h"

TEST (chord_engine_c_major)
{
    using namespace ovtchord;
    ChordEngine e;
    const auto r = e.recognize ({ 0, 4, 7 }, -1, 0.0); // C E G
    CHECK (r.valid);
    CHECK (r.rootPitchClass == 0);
    CHECK (r.symbol == "C");
}

TEST (chord_engine_g7)
{
    using namespace ovtchord;
    ChordEngine e;
    const auto r = e.recognize ({ 7, 11, 2, 5 }, -1, 0.0); // G B D F
    CHECK (r.valid);
    CHECK (r.rootPitchClass == 7);
    CHECK (r.symbol == "G7");
}

TEST (chord_engine_csus4)
{
    using namespace ovtchord;
    ChordEngine e;
    const auto r = e.recognize ({ 0, 5, 7 }, -1, 0.0); // C F G
    CHECK (r.valid);
    CHECK (r.rootPitchClass == 0);
    CHECK (r.symbol == "Csus4");
}

TEST (chord_engine_am)
{
    using namespace ovtchord;
    ChordEngine e;
    const auto r = e.recognize ({ 9, 0, 4 }, -1, 0.0); // A C E
    CHECK (r.valid);
    CHECK (r.rootPitchClass == 9);
    CHECK (r.symbol == "Am");
}

TEST (chord_engine_inversion)
{
    using namespace ovtchord;
    ChordEngine e;
    const auto r = e.recognize ({ 0, 4, 7 }, 4, 0.0); // C E G, bass E
    CHECK (r.valid);
    CHECK (r.symbol == "C/E");
}

TEST (chord_engine_too_few_notes)
{
    using namespace ovtchord;
    ChordEngine e;
    const auto r = e.recognize ({ 0, 4 }, -1, 0.0); // only 2 notes
    CHECK (! r.valid);
}

TEST (chord_engine_stability)
{
    using namespace ovtchord;
    ChordEngine::Config cfg;
    cfg.stabilityFrames = 3;
    ChordEngine e (cfg);
    // First frame: not yet stable.
    auto r = e.recognize ({ 0, 4, 7 }, -1, 0.0);
    CHECK (! r.valid);
    r = e.recognize ({ 0, 4, 7 }, -1, 0.0);
    CHECK (! r.valid);
    r = e.recognize ({ 0, 4, 7 }, -1, 0.0);
    CHECK (r.valid);
    CHECK (r.symbol == "C");
}
