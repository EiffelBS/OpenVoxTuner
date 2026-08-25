#pragma once
// MidiImporterTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/MidiImporter.h"

/** RAII wrapper: writes a MidiFile to a temp file, deletes it on scope exit. */
class TempMidiFile
{
public:
    /** Build the temp file from the given MidiFile content.
     *  The file is deleted automatically when this object goes out of scope. */
    explicit TempMidiFile (const juce::MidiFile& mf)
        : file (juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("ovt_midi_importer_test_"
                                  + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30))
                                  + ".mid"))
    {
        juce::FileOutputStream out (file);
        mf.writeTo (out);
        out.flush();
    }

    ~TempMidiFile() { file.deleteFile(); }

    const juce::File& getFile() const { return file; }

    TempMidiFile (const TempMidiFile&) = delete;
    TempMidiFile& operator= (const TempMidiFile&) = delete;

private:
    juce::File file;
};

/** Test MIDI builder: 480 ticks/quarter, explicit 120 BPM tempo meta
 *  (so 1 second == 960 ticks after convertTimestampTicksToSeconds). */
class TestMidiBuilder
{
public:
    static constexpr int ticksPerQuarter = 480;

    /** Seconds -> ticks at 120 BPM / 480 TPQN. */
    static double secToTicks (double sec) { return sec * 2.0 * (double) ticksPerQuarter; }

    TestMidiBuilder()
    {
        // Explicit tempo meta event: deterministic timing conversion.
        sequence.addEvent (juce::MidiMessage::tempoMetaEvent (500000).withTimeStamp (0.0));
    }

    /** Add a complete note (start/end in seconds). */
    void addNote (int channel, int note, double startSec, double endSec, juce::uint8 velocity = 100)
    {
        sequence.addEvent (juce::MidiMessage::noteOn (channel, note, velocity)
                               .withTimeStamp (secToTicks (startSec)));
        sequence.addEvent (juce::MidiMessage::noteOff (channel, note, (juce::uint8) 0)
                               .withTimeStamp (secToTicks (endSec)));
    }

    /** Add a bare note-on that is never closed (edge case). */
    void addDanglingNoteOn (int channel, int note, double startSec)
    {
        sequence.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100)
                               .withTimeStamp (secToTicks (startSec)));
    }

    /** Finalize into a MidiFile ready to be written. */
    juce::MidiFile build()
    {
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (ticksPerQuarter);
        mf.addTrack (sequence);
        return mf;
    }

private:
    juce::MidiMessageSequence sequence;
};

/** Reference frequency of an equal-tempered MIDI note (A4 = 440 Hz). */
static double hzOf (int midiNote)
{
    return 440.0 * std::pow (2.0, (midiNote - 69) / 12.0);
}

class MidiImporterTest : public juce::UnitTest
{
public:
    MidiImporterTest() : juce::UnitTest ("MidiImporter") {}

    void runTest() override
    {
        using namespace ovtdsp;
        using Strategy = MidiExtractStrategy;

        beginTest ("analyzeFile: monophonic single-channel summary");
        {
            TestMidiBuilder b;
            b.addNote (1, 60, 0.00, 0.50); // C4
            b.addNote (1, 64, 0.50, 1.00); // E4
            b.addNote (1, 67, 1.00, 1.50); // G4
            b.addNote (1, 72, 1.50, 2.00); // C5
            TempMidiFile tmp (b.build());

            auto info = MidiImporter::analyzeFile (tmp.getFile());
            expect (info.isValid, "file should be valid");
            expectEquals (info.errorMessage, juce::String());
            expectEquals ((int) info.channels.size(), 1);
            expectEquals (info.channels[0].channel, 1);
            expectEquals (info.channels[0].numNotes, 4);
            expectEquals (info.channels[0].minNote, 60);
            expectEquals (info.channels[0].maxNote, 72);
            expectEquals (info.totalNotes, 4);
            // Duration: first note start (0.0) to last note end (2.0).
            expectWithinAbsoluteError (info.channels[0].durationSec, 2.0, 0.01);
        }

        beginTest ("analyzeFile: channel 10 percussion excluded");
        {
            TestMidiBuilder b;
            b.addNote (1, 60, 0.0, 0.5);
            b.addNote (10, 36, 0.0, 0.25); // percussion kick
            b.addNote (10, 42, 0.5, 0.75); // percussion hat
            TempMidiFile tmp (b.build());

            auto info = MidiImporter::analyzeFile (tmp.getFile());
            expect (info.isValid);
            expectEquals ((int) info.channels.size(), 1, "only channel 1 should be listed");
            expectEquals (info.channels[0].channel, 1);
            expectEquals (info.totalNotes, 1); // the single channel-1 note-on (ch10 excluded)
        }

        beginTest ("analyzeFile: multi-channel summary sorted by channel");
        {
            TestMidiBuilder b;
            b.addNote (2, 72, 0.00, 0.50); // channel 2 melody
            b.addNote (2, 74, 0.50, 1.00);
            b.addNote (1, 48, 0.00, 1.00); // channel 1 bass
            TempMidiFile tmp (b.build());

            auto info = MidiImporter::analyzeFile (tmp.getFile());
            expect (info.isValid);
            expectEquals ((int) info.channels.size(), 2);
            expectEquals (info.channels[0].channel, 1, "channels sorted ascending");
            expectEquals (info.channels[1].channel, 2);
            expectEquals (info.channels[0].numNotes, 1);
            expectEquals (info.channels[0].minNote, 48);
            expectEquals (info.channels[1].numNotes, 2);
            expectEquals (info.totalNotes, 3);
        }

        beginTest ("analyzeFile: garbage bytes rejected");
        {
            auto junk = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("ovt_midi_junk.mid");
            junk.replaceWithText ("this is definitely not a MIDI file");
            auto info = MidiImporter::analyzeFile (junk);
            junk.deleteFile();

            expect (! info.isValid, "garbage must not parse");
            expect (info.errorMessage.isNotEmpty(), "an error message must be reported");
        }

        beginTest ("analyzeFile: nonexistent path rejected");
        {
            auto missing = juce::File::getSpecialLocation (juce::File::tempDirectory)
                               .getChildFile ("ovt_midi_does_not_exist.mid");
            auto info = MidiImporter::analyzeFile (missing);

            expect (! info.isValid);
            expect (info.errorMessage.startsWith ("Cannot open file"),
                    "unexpected message: " + info.errorMessage);
        }

        beginTest ("analyzeFile: valid file without any notes rejected");
        {
            // A syntactically valid SMF that contains only a CC event.
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::controllerEvent (1, 7, 100).withTimeStamp (0.0));
            juce::MidiFile mf;
            mf.setTicksPerQuarterNote (TestMidiBuilder::ticksPerQuarter);
            mf.addTrack (seq);
            TempMidiFile tmp (mf);

            auto info = MidiImporter::analyzeFile (tmp.getFile());
            expect (! info.isValid, "a note-less file must be reported invalid");
            expect (info.errorMessage.contains ("no notes"));
        }

        beginTest ("importFrom: monophonic sequence -> one point per note");
        {
            TestMidiBuilder b;
            b.addNote (1, 60, 0.00, 0.50); // C4
            b.addNote (1, 64, 0.50, 1.00); // E4
            b.addNote (1, 67, 1.00, 1.50); // G4
            b.addNote (1, 72, 1.50, 2.00); // C5
            TempMidiFile tmp (b.build());

            auto curve = MidiImporter::importFrom (tmp.getFile(), Strategy::HighestNote);
            expectEquals (curve.getNumPoints(), 4, "one emitted point per note change");

            const double expectedTimes[] = { 0.0, 0.5, 1.0, 1.5 };
            const int expectedNotes[] = { 60, 64, 67, 72 };
            for (int i = 0; i < 4; ++i)
            {
                expectWithinAbsoluteError (curve.getPoint (i).time, expectedTimes[i], 0.001);
                expectWithinAbsoluteError ((double) curve.getPoint (i).pitch,
                                           hzOf (expectedNotes[i]), 0.5);
            }
        }

        beginTest ("importFrom: polyphonic chord reduction (highest vs lowest)");
        {
            // Staggered extents so every event time is distinct (the sweep
            // emits a transition point whenever the selection changes, which
            // is the documented behavior at note boundaries).
            TestMidiBuilder b;
            b.addNote (1, 60, 0.00, 1.00); // C4 sustained
            b.addNote (1, 76, 0.25, 0.75); // E5 overlapping
            TempMidiFile tmp (b.build());

            // Highest: C@0 -> E@0.25 -> back to C when E ends @0.75.
            auto highest = MidiImporter::importFrom (tmp.getFile(), Strategy::HighestNote);
            expectEquals (highest.getNumPoints(), 3);
            expectWithinAbsoluteError ((double) highest.getPoint (0).pitch, hzOf (60), 0.5);
            expectWithinAbsoluteError ((double) highest.getPoint (1).pitch, hzOf (76), 0.5);
            expectWithinAbsoluteError ((double) highest.getPoint (2).pitch, hzOf (60), 0.5);

            // Lowest: C4 always wins while it sustains -> single point.
            auto lowest = MidiImporter::importFrom (tmp.getFile(), Strategy::LowestNote);
            expectEquals (lowest.getNumPoints(), 1);
            expectWithinAbsoluteError ((double) lowest.getPoint (0).pitch, hzOf (60), 0.5);
        }

        beginTest ("importFrom: loudest note wins on overlap");
        {
            TestMidiBuilder b;
            b.addNote (1, 60, 0.00, 1.00, (juce::uint8) 50);  // quiet C4
            b.addNote (1, 67, 0.25, 0.75, (juce::uint8) 110); // loud G4 overlap
            TempMidiFile tmp (b.build());

            // C@0 -> G@0.25 (louder) -> back to C@0.75 when G ends.
            auto curve = MidiImporter::importFrom (tmp.getFile(), Strategy::LoudestNote);
            expectEquals (curve.getNumPoints(), 3);
            expectWithinAbsoluteError ((double) curve.getPoint (0).pitch, hzOf (60), 0.5);
            expectWithinAbsoluteError ((double) curve.getPoint (1).pitch, hzOf (67), 0.5);
            expectWithinAbsoluteError ((double) curve.getPoint (2).pitch, hzOf (60), 0.5);
        }

        beginTest ("importFrom: SpecificChannel keeps only the requested channel");
        {
            TestMidiBuilder b;
            b.addNote (1, 48, 0.0, 1.0); // bass on channel 1
            b.addNote (2, 76, 0.0, 1.0); // lead on channel 2
            TempMidiFile tmp (b.build());

            auto curve = MidiImporter::importFrom (tmp.getFile(), Strategy::SpecificChannel, 2);
            expectEquals (curve.getNumPoints(), 1, "channel 1 notes must be dropped");
            expectWithinAbsoluteError ((double) curve.getPoint (0).pitch, hzOf (76), 0.5);
        }

        beginTest ("importFrom: percussion-only file yields an empty curve");
        {
            TestMidiBuilder b;
            b.addNote (10, 36, 0.0, 0.5);
            b.addNote (10, 38, 0.5, 1.0);
            TempMidiFile tmp (b.build());

            auto curve = MidiImporter::importFrom (tmp.getFile(), Strategy::HighestNote);
            expectEquals (curve.getNumPoints(), 0, "channel 10 is always excluded");
        }

        beginTest ("importFrom: sub-20ms artefact notes are filtered");
        {
            TestMidiBuilder b;
            b.addNote (1, 60, 0.00, 0.50);   // real note
            b.addNote (1, 90, 0.25, 0.255);  // 5 ms blip -> below the 20 ms floor
            TempMidiFile tmp (b.build());

            auto curve = MidiImporter::importFrom (tmp.getFile(), Strategy::HighestNote);
            expectEquals (curve.getNumPoints(), 1, "the blip must not appear in the curve");
            expectWithinAbsoluteError ((double) curve.getPoint (0).pitch, hzOf (60), 0.5);
        }

        beginTest ("importFrom: dangling note-on (no note-off) ignored");
        {
            TestMidiBuilder b;
            b.addDanglingNoteOn (1, 64, 0.0);
            TempMidiFile tmp (b.build());

            // Documented behavior: analyzeFile counts raw note-ons (valid),
            // while importFrom requires paired spans (empty curve).
            auto info = MidiImporter::analyzeFile (tmp.getFile());
            expect (info.isValid);
            expectEquals (info.totalNotes, 1);

            auto curve = MidiImporter::importFrom (tmp.getFile(), Strategy::HighestNote);
            expectEquals (curve.getNumPoints(), 0);
        }

        beginTest ("importFrom: garbage file yields an empty curve");
        {
            auto junk = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("ovt_midi_junk2.mid");
            junk.replaceWithText ("\x00\x01\x02 still not midi \xff\xfe");
            auto curve = MidiImporter::importFrom (junk, Strategy::HighestNote);
            junk.deleteFile();

            expectEquals (curve.getNumPoints(), 0, "failed parse must give an empty curve");
        }
    }
};

static MidiImporterTest midiImporterTest;
