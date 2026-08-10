// test_midi_parser.cpp
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#include "ovtchord/midi_parser.h"
#include "midi/note_tracker.h"
#include "test_harness.h"

TEST (midi_parser_note_on_off)
{
    using namespace ovtchord;
    MidiParser p;
    // Note On C4 (60) vel 100, then Note Off C4.
    const uint8_t data[] = { 0x90, 60, 100, 0x80, 60, 0 };
    const auto ev = p.parse (data, sizeof (data), 0.0);
    CHECK (ev.size() == 2);
    if (ev.size() == 2)
    {
        CHECK (ev[0].status == 0x90);
        CHECK (ev[0].data1 == 60);
        CHECK (ev[0].data2 == 100);
        CHECK (ev[1].status == 0x80);
        CHECK (ev[1].data1 == 60);
    }
}

TEST (midi_parser_velocity_zero_is_note_off)
{
    using namespace ovtchord;
    MidiParser p;
    const uint8_t data[] = { 0x90, 60, 0 }; // Note On vel 0 == Note Off
    const auto ev = p.parse (data, sizeof (data), 0.0);
    CHECK (ev.size() == 1);
    if (ev.size() == 1)
    {
        CHECK (ev[0].status == 0x90);
        CHECK (ev[0].data1 == 60);
        CHECK (ev[0].data2 == 0);
    }
}

TEST (midi_parser_running_status)
{
    using namespace ovtchord;
    MidiParser p;
    // Note On 60, then running status: 64, 67 (all Note On, vel 100).
    const uint8_t data[] = { 0x90, 60, 100, 64, 100, 67, 100 };
    const auto ev = p.parse (data, sizeof (data), 0.0);
    CHECK (ev.size() == 3);
    if (ev.size() == 3)
    {
        CHECK (ev[0].data1 == 60);
        CHECK (ev[1].data1 == 64);
        CHECK (ev[2].data1 == 67);
        CHECK (ev[1].status == 0x90);
    }
}

TEST (midi_parser_ignores_sysex)
{
    using namespace ovtchord;
    MidiParser p;
    // SysEx then a Note On.
    const uint8_t data[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7, 0x90, 60, 100 };
    const auto ev = p.parse (data, sizeof (data), 0.0);
    CHECK (ev.size() == 1);
    if (ev.size() == 1)
    {
        CHECK (ev[0].status == 0x90);
        CHECK (ev[0].data1 == 60);
    }
}

TEST (note_tracker_pitch_class_set)
{
    using namespace ovtchord;
    NoteTracker t;
    // C major: C4(60), E4(64), G4(67).
    t.process ({ 0.0, 0x90, 60, 100 });
    t.process ({ 0.0, 0x90, 64, 100 });
    t.process ({ 0.0, 0x90, 67, 100 });
    auto pcs = t.pitchClassSet();
    CHECK (pcs.size() == 3);
    if (pcs.size() == 3)
    {
        CHECK (pcs[0] == 0); // C
        CHECK (pcs[1] == 4); // E
        CHECK (pcs[2] == 7); // G
    }
    // Release E.
    t.process ({ 0.0, 0x80, 64, 0 });
    pcs = t.pitchClassSet();
    CHECK (pcs.size() == 2);
    CHECK (t.lowestNote() == 60);
}

TEST (note_tracker_all_notes_off)
{
    using namespace ovtchord;
    NoteTracker t;
    t.process ({ 0.0, 0x90, 60, 100 });
    t.process ({ 0.0, 0x90, 64, 100 });
    t.process ({ 0.0, 0xB0, 123, 0 }); // CC 123 = All Notes Off
    CHECK (t.pitchClassSet().empty());
}
