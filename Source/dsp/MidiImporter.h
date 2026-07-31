// MidiImporter.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchCurve.h"

namespace ovtdsp
{
    /** Metadata about a single MIDI channel found in a file. */
    struct MidiChannelInfo
    {
        int channel = 0;            // MIDI channel 1-16
        int numNotes = 0;           // total note-on count
        int minNote = 127;          // lowest MIDI note number
        int maxNote = 0;            // highest MIDI note number
        double durationSec = 0.0;   // time of last event - time of first event
    };

    /** Result of analyzing a MIDI file (metadata only, no curve generated). */
    struct MidiImportInfo
    {
        juce::Array<MidiChannelInfo> channels; // one per active channel (excluding ch10)
        int totalNotes = 0;
        double totalDurationSec = 0.0;
        int numTracks = 0;
        bool isValid = false;
        juce::String errorMessage;
    };

    /** Strategy for reducing polyphonic MIDI to a monophonic curve. */
    enum class MidiExtractStrategy
    {
        HighestNote,       // highest active note at each instant (lead melody)
        LowestNote,        // lowest active note (bass line)
        LoudestNote,       // highest velocity (interpreted as emphasis)
        SpecificChannel    // all notes from a single MIDI channel
    };

    /**
     * Utility class to analyze and import MIDI files into a PitchCurve.
     * Uses juce::MidiFile for parsing (available via juce_audio_basics).
     */
    class MidiImporter
    {
    public:
        /** Analyze a .mid file and return metadata (channels, note counts, etc.).
         *  Does NOT generate a curve. Use this to decide which channel/strategy
         *  to apply before calling importFrom().
         *  @param midiFile  path to the .mid file
         *  @return          MidiImportInfo with parsed metadata or error message
         */
        static MidiImportInfo analyzeFile (const juce::File& midiFile);

        /** Import a .mid file and return a PitchCurve.
         *  @param midiFile           path to the .mid file
         *  @param strategy           polyphony reduction strategy
         *  @param targetChannel      MIDI channel (1-16), used only with SpecificChannel
         *  @return                   the generated PitchCurve (empty if parse failed)
         */
        static PitchCurve importFrom (
            const juce::File& midiFile,
            MidiExtractStrategy strategy,
            int targetChannel = 1);

    private:
        MidiImporter() = delete; // static-only class
    };
}
