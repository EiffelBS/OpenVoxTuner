// chord_engine.h
// ovtchord — standalone chord detection library (MIDI + audio)
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.

#pragma once

#include "ovtchord/types.h"
#include <vector>

namespace ovtchord
{
    // Recognizes a chord from a pitch-class set. Shared by the MIDI and audio
    // modules so both produce consistent results.
    class ChordEngine
    {
    public:
        struct Config
        {
            int minNotes = 3;          // minimum notes to consider a chord
            float minConfidence = 0.5f;
            int stabilityFrames = 1;   // consecutive frames for a stable result
        };

        explicit ChordEngine (const Config& cfg = Config());

        // Recognize a chord from a pitch-class set.
        // `bassPitchClass` (-1 = unknown) is used to annotate inversions.
        ChordResult recognize (const std::vector<int>& pitchClasses,
                               int bassPitchClass = -1,
                               double timestamp = 0.0);

        void reset();

    private:
        Config cfg;
        std::vector<int> lastPitchClasses;
        int stableCount = 0;
    };
}
