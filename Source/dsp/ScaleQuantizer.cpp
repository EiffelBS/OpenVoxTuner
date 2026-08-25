// ScaleQuantizer.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "ScaleQuantizer.h"
#include <vector>

namespace ovtdsp
{
    // Semitones of one octave for each mode (relative to the tonic).
    static const std::vector<int> chromaticIntervals       = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    static const std::vector<int> majorIntervals           = { 0, 2, 4, 5, 7, 9, 11 };
    static const std::vector<int> melodicMinorIntervals    = { 0, 2, 3, 5, 7, 9, 11 };
    static const std::vector<int> harmonicMinorIntervals   = { 0, 2, 3, 5, 7, 8, 11 };
    static const std::vector<int> naturalMinorIntervals    = { 0, 2, 3, 5, 7, 8, 10 };
    static const std::vector<int> majorPentatonicIntervals = { 0, 2, 4, 7, 9 };
    static const std::vector<int> minorPentatonicIntervals = { 0, 3, 5, 7, 10 };
    static const std::vector<int> bluesIntervals           = { 0, 3, 5, 6, 7, 10 };
    static const std::vector<int> dorianIntervals          = { 0, 2, 3, 5, 7, 9, 10 };
    static const std::vector<int> phrygianIntervals        = { 0, 1, 3, 5, 7, 8, 10 };
    static const std::vector<int> lydianIntervals          = { 0, 2, 4, 6, 7, 9, 11 };
    static const std::vector<int> mixolydianIntervals      = { 0, 2, 4, 5, 7, 9, 10 };
    static const std::vector<int> locrianIntervals         = { 0, 1, 3, 5, 6, 8, 10 };
    static const std::vector<int> majorTriadIntervals      = { 0, 4, 7 };
    static const std::vector<int> minorTriadIntervals      = { 0, 3, 7 };

    ScaleQuantizer::ScaleQuantizer()
    {
        // By default, enable the full chromatic scale as custom.
        for (int i = 0; i < 12; ++i) customIntervals.add (i);
        rebuildIntervals();
    }

    void ScaleQuantizer::setKey (int keyInSemitones)
    {
        key = ((keyInSemitones % 12) + 12) % 12;
        rebuildIntervals();
    }

    void ScaleQuantizer::setScale (Scale s)
    {
        currentScale = s;
        rebuildIntervals();
    }

    void ScaleQuantizer::setCustomIntervals (const juce::Array<int>& notesInSemitones)
    {
        customIntervals.clear();
        for (int n : notesInSemitones)
        {
            const int m = ((n % 12) + 12) % 12;
            if (! customIntervals.contains (m))
                customIntervals.add (m);
        }
        // If empty, fall back to chromatic to avoid an empty scale.
        if (customIntervals.isEmpty())
            for (int i = 0; i < 12; ++i) customIntervals.add (i);
        rebuildIntervals();
    }

    void ScaleQuantizer::rebuildIntervals()
    {
        intervals.clear();

        if (currentScale == Scale::Custom)
        {
            for (int n : customIntervals)
                intervals.add (((n % 12) + 12) % 12);
            return;
        }

        static const int chromatic[]       = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        static const int major[]           = { 0, 2, 4, 5, 7, 9, 11 };
        static const int melodicMinor[]    = { 0, 2, 3, 5, 7, 9, 11 };
        static const int harmonicMinor[]   = { 0, 2, 3, 5, 7, 8, 11 };
        static const int naturalMinor[]    = { 0, 2, 3, 5, 7, 8, 10 };
        static const int majorPentatonic[] = { 0, 2, 4, 7, 9 };
        static const int minorPentatonic[] = { 0, 3, 5, 7, 10 };
        static const int blues[]           = { 0, 3, 5, 6, 7, 10 };
        static const int dorian[]          = { 0, 2, 3, 5, 7, 9, 10 };
        static const int phrygian[]        = { 0, 1, 3, 5, 7, 8, 10 };
        static const int lydian[]          = { 0, 2, 4, 6, 7, 9, 11 };
        static const int mixolydian[]      = { 0, 2, 4, 5, 7, 9, 10 };
        static const int locrian[]         = { 0, 1, 3, 5, 6, 8, 10 };

        const int* base = nullptr;
        int size = 0;

        switch (currentScale)
        {
            case Scale::Chromatic:       base = chromatic;       size = 12; break;
            case Scale::Major:           base = major;           size = 7; break;
            case Scale::MelodicMinor:    base = melodicMinor;    size = 7; break;
            case Scale::HarmonicMinor:   base = harmonicMinor;   size = 7; break;
            case Scale::NaturalMinor:    base = naturalMinor;    size = 7; break;
            case Scale::MajorPentatonic: base = majorPentatonic; size = 5; break;
            case Scale::MinorPentatonic: base = minorPentatonic; size = 5; break;
            case Scale::Blues:           base = blues;           size = 6; break;
            case Scale::Dorian:          base = dorian;          size = 7; break;
            case Scale::Phrygian:        base = phrygian;        size = 7; break;
            case Scale::Lydian:          base = lydian;          size = 7; break;
            case Scale::Mixolydian:      base = mixolydian;      size = 7; break;
            case Scale::Locrian:         base = locrian;         size = 7; break;
            case Scale::Custom:          return;
            default:
                // Safety net: fall back to Chromatic for unexpected enum values.
                base = chromatic; size = 12; break; 
        }

        if (base != nullptr)
        {
            for (int i = 0; i < size; ++i)
                intervals.add ((base[i] + key) % 12);
        }
    }

    float ScaleQuantizer::quantize (float hzIn, double positionPPQ) const
    {
        if (hzIn <= 0.0f) return 0.0f;

        // 1) Convert Hz -> semitones relative to A4 (440 Hz).
        const float semitones = hzToSemitones (hzIn);

        // 2) Separate the integer part (octave) and fractional part (position in the octave).
        //    The scale repeats at every octave, so we only quantize modulo 12.
        const float roundedSemi = std::round (semitones);
        const float frac = semitones - roundedSemi; // in [-0.5, 0.5]

        // 3) Convert the position in the octave to [0, 12).
        //    "roundedSemi" is the MIDI note in semitones relative to A4.
        //    A4 = 69 in MIDI. Semitone 0 = A, 1 = A#, ..., 12 = next A.
        const int midiRef = 69; // A4 in MIDI
        const int currentMidi = static_cast<int> (std::round (roundedSemi)) + midiRef;
        const int noteInOctave = ((currentMidi % 12) + 12) % 12; // [0,11]

        // 4) Find the closest scale semitone.
        //    If the exact note is in the scale, keep it.
        //    Otherwise, take the closest scale note (in modulo-12 cycles).
        if (intervals.size() == 0)
            return hzIn;

        // A note is allowed if it belongs to the scale, or if a chord-context
        // override is active at this position and the note is a chord tone
        // (this allows temporarily accepting out-of-scale notes when an
        // out-of-scale chord is played).
        const bool inScale = intervals.contains (noteInOctave);
        if (inScale || isChordToneAt (noteInOctave, positionPPQ))
        {
            // Already in the scale: the frequency would be unchanged IF the
            // quantizer were perfect (absolute T-Pain effect). But an autotune
            // must *correct* the pitch even if it is the right note
            // (e.g. 445Hz -> 440Hz).
            const int correctedMidi = currentMidi;
            return 440.0f * std::pow (2.0f, (correctedMidi - midiRef) / 12.0f);
        }

        // Find the closest scale note, using circular distance.
        // Circular distance between two notes modulo 12, within [-6, 6].
        auto circularDist = [] (int a, int b) -> int
        {
            int d = b - a;
            d = ((d + 6) % 12) - 6; // bring back into [-6, 5]
            if (d < -6) d += 12;
            return d;
        };

        int bestShift = 0;
        int bestDist = 100;
        for (int scaleNote : intervals)
        {
            int shift = circularDist (noteInOctave, scaleNote);
            int dist = std::abs (shift);
            if (dist < bestDist)
            {
                bestDist = dist;
                bestShift = shift;
                if (dist == 0) break;
            }
        }

        // 5) Recompute the corrected frequency.
        const int correctedMidi = currentMidi + bestShift;
        return 440.0f * std::pow (2.0f, (correctedMidi - midiRef) / 12.0f);

        // Note: the "frac" value is kept for a possible future extension
        // (quantization with microtonal preservation).
        juce::ignoreUnused (frac);
    }

    void ScaleQuantizer::setChordOverride (const juce::Array<int>& chordPitchClasses,
                                           double positionPPQ,
                                           double durationPPQ)
    {
        // Adds an override window. The UI rebuilds the whole list at every
        // refresh (clearChordOverrides + setChordOverride x N).
        ChordWindow w;
        w.chordNotes = chordPitchClasses;
        w.startPPQ  = positionPPQ;
        w.duration  = durationPPQ;
        const juce::ScopedLock lock (chordOverrideLock);
        chordWindows.add (w);
    }

    void ScaleQuantizer::clearChordOverrides()
    {
        const juce::ScopedLock lock (chordOverrideLock);
        chordWindows.clear(); // resets the list of windows
    }

    bool ScaleQuantizer::isChordOverrideActive (double positionPPQ) const
    {
        const juce::ScopedLock lock (chordOverrideLock);
        for (const auto& w : chordWindows)
            if (positionPPQ >= w.startPPQ && positionPPQ < w.startPPQ + w.duration)
                return true;
        return false;
    }

    bool ScaleQuantizer::isChordToneAt (int pitchClass, double pos) const
    {
        const juce::ScopedLock lock (chordOverrideLock);
        // The "live" chord (real-time MIDI/sidechain) takes priority over
        // ARA windows when active.
        if (liveChordActive && liveChordNotes.contains (pitchClass))
            return true;
        for (const auto& w : chordWindows)
            if (pos >= w.startPPQ && pos < w.startPPQ + w.duration
                && w.chordNotes.contains (pitchClass))
                return true;
        return false;
    }

    void ScaleQuantizer::setLiveChordOverride (const juce::Array<int>& chordPitchClasses)
    {
        const juce::ScopedLock lock (chordOverrideLock);
        liveChordNotes = chordPitchClasses;
        liveChordActive = true;
    }

    void ScaleQuantizer::clearLiveChordOverride()
    {
        const juce::ScopedLock lock (chordOverrideLock);
        liveChordNotes.clear();
        liveChordActive = false;
    }

    bool ScaleQuantizer::isLiveChordActive() const
    {
        const juce::ScopedLock lock (chordOverrideLock);
        return liveChordActive;
    }

    bool ScaleQuantizer::isNoteAllowed (int pitchClass, double pos) const
    {
        return intervals.contains (pitchClass) || isChordToneAt (pitchClass, pos);
    }

    bool ScaleQuantizer::isActiveChordOutOfScale (double pos) const
    {
        const juce::ScopedLock lock (chordOverrideLock);
        // The live chord (real-time) takes priority over ARA windows.
        if (liveChordActive)
        {
            for (int pc : liveChordNotes)
                if (! intervals.contains (pc))
                    return true;
            return false;
        }
        for (const auto& w : chordWindows)
        {
            if (pos >= w.startPPQ && pos < w.startPPQ + w.duration)
            {
                for (int pc : w.chordNotes)
                    if (! intervals.contains (pc))
                        return true;
                return false;
            }
        }
        return false;
    }
}



