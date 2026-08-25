// NoteUtils.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace ovtdsp
{
    /**
     * Converts a frequency (Hz) to a MIDI note number (float).
     * Returns a negative number if hz <= 0.
     */
    inline float hzToMidiFloat (float hz) noexcept
    {
        if (hz <= 0.0f) return -1000.0f;
        return 69.0f + 12.0f * std::log2 (hz / 440.0f);
    }

    /**
     * Converts a MIDI note number to a frequency (Hz).
     */
    inline float midiToHz (float midi) noexcept
    {
        return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
    }

    /**
     * Computes the cents offset between a detected pitch and a target pitch.
     * Positive = detected pitch is ABOVE the target.
     *   cents = 1200 * log2(hzDetected / hzTarget)
     */
    inline float hzToCents (float hzDetected, float hzTarget) noexcept
    {
        if (hzDetected <= 0.0f || hzTarget <= 0.0f) return 0.0f;
        return 1200.0f * std::log2 (hzDetected / hzTarget);
    }

    /**
     * Returns the "scientific" octave of a MIDI number:
     *   C-1 = 0, C0 = 12, C1 = 24, ..., C4 = 60, A4 = 69, C5 = 72.
     */
    inline int midiToOctave (int midi) noexcept
    {
        return (midi / 12) - 1;
    }

    /**
     * Returns the note index within the octave [0..11] for a MIDI number.
     *   0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B.
     */
    inline int midiToNoteInOctave (int midi) noexcept
    {
        const int n = midi % 12;
        return (n < 0) ? n + 12 : n;
    }

    /**
     * Returns the short note name (C, C#, D, ..., B) for an index 0..11.
     */
    inline const char* noteInOctaveName (int idx) noexcept
    {
        static const char* names[12] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        if (idx < 0 || idx >= 12) return "?";
        return names[idx];
    }

    /**
     * Returns the full note name (e.g. "F3", "A#4") for a given Hz.
     * If hz <= 0, returns "--".
     */
    inline juce::String hzToNoteName (float hz) noexcept
    {
        if (hz <= 0.0f) return juce::String ("--");
        const float midiF = hzToMidiFloat (hz);
        const int midi = static_cast<int> (std::lround (midiF));
        const int note = midiToNoteInOctave (midi);
        const int oct  = midiToOctave (midi);
        return juce::String (noteInOctaveName (note)) + juce::String (oct);
    }

    /**
     * Returns the closest quantized note (Hz) in the scale, expressed as a
     * "F3" name + number of cents of deviation from the original note.
     *
     * @param hzIn     detected pitch in Hz
     * @param hzTarget target pitch (after quantization) in Hz
     * @return         struct with the note name, the number of cents, etc.
     */
    struct NoteInfo
    {
        juce::String name;       // e.g. "F3"
        juce::String targetName; // e.g. "F3" (target note after quantization)
        float midi      = 0.0f;  // MIDI float of the detected pitch
        float targetMidi= 0.0f;  // MIDI float of the target note
        int   octave    = 0;     // octave of the detected note
        int   noteInOct = 0;     // 0..11
        float cents     = 0.0f;  // deviation of detected pitch vs target (target = 0)
        bool  valid     = false; // false if hzIn <= 0
    };

    inline NoteInfo describePitch (float hzIn, float hzTarget) noexcept
    {
        NoteInfo info;
        if (hzIn <= 0.0f)
        {
            info.name = "--";
            info.targetName = "--";
            info.valid = false;
            return info;
        }
        info.valid     = true;
        info.midi      = hzToMidiFloat (hzIn);
        info.targetMidi= (hzTarget > 0.0f) ? hzToMidiFloat (hzTarget) : info.midi;
        const int midi = static_cast<int> (std::lround (info.midi));
        info.noteInOct = midiToNoteInOctave (midi);
        info.octave    = midiToOctave (midi);
        info.name      = juce::String (noteInOctaveName (info.noteInOct)) + juce::String (info.octave);

        const int tmidi = static_cast<int> (std::lround (info.targetMidi));
        info.targetName= juce::String (noteInOctaveName (midiToNoteInOctave (tmidi)))
                       + juce::String (midiToOctave (tmidi));

        // Cents = 100 * (semitones offset within an octave)
        // Take the offset within the octave for the detected pitch.
        const float frac = info.midi - std::floor (info.midi);
        info.cents = (frac - std::round (frac)) * 100.0f;
        // If we exceed 50 cents in absolute value, wrap relative to the
        // closest adjacent note.
        if (info.cents > 50.0f)  info.cents -= 100.0f;
        if (info.cents < -50.0f) info.cents += 100.0f;
        return info;
    }
}



