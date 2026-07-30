// NoteUtils.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>

namespace ovtdsp
{
    /**
     * Convertit une frequence (Hz) en numero de note MIDI (float).
     * Retourne un nombre negatif si hz <= 0.
     */
    inline float hzToMidiFloat (float hz) noexcept
    {
        if (hz <= 0.0f) return -1000.0f;
        return 69.0f + 12.0f * std::log2 (hz / 440.0f);
    }

    /**
     * Convertit un numero de note MIDI en frequence (Hz).
     */
    inline float midiToHz (float midi) noexcept
    {
        return 440.0f * std::pow (2.0f, (midi - 69.0f) / 12.0f);
    }

    /**
     * Calcule le nombre de cents d'offset entre un pitch detecte et un pitch
     * cible. Positif = pitch detecte est AU-DESSUS de la cible.
     *   cents = 1200 * log2(hzDetected / hzTarget)
     */
    inline float hzToCents (float hzDetected, float hzTarget) noexcept
    {
        if (hzDetected <= 0.0f || hzTarget <= 0.0f) return 0.0f;
        return 1200.0f * std::log2 (hzDetected / hzTarget);
    }

    /**
     * Renvoie l'octave "scientifique" d'un numero MIDI :
     *   C-1 = 0, C0 = 12, C1 = 24, ..., C4 = 60, A4 = 69, C5 = 72.
     */
    inline int midiToOctave (int midi) noexcept
    {
        return (midi / 12) - 1;
    }

    /**
     * Renvoie l'indice de note dans l'octave [0..11] pour un numero MIDI.
     *   0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B.
     */
    inline int midiToNoteInOctave (int midi) noexcept
    {
        const int n = midi % 12;
        return (n < 0) ? n + 12 : n;
    }

    /**
     * Renvoie le nom court de la note (C, C#, D, ..., B) pour un indice 0..11.
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
     * Renvoie le nom complet de la note (ex: "F3", "A#4") pour un Hz donne.
     * Si hz <= 0, retourne "--".
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
     * Renvoie la note quantifiee (Hz) la plus proche dans la gamme,
     * exprimee comme nom "F3" + nombre de cents de deviation par rapport
     * a la note d'origine.
     *
     * @param hzIn     pitch detecte en Hz
     * @param hzTarget pitch cible (apres quantification) en Hz
     * @return         struct avec le nom de la note, le nombre de cents, etc.
     */
    struct NoteInfo
    {
        juce::String name;       // ex: "F3"
        juce::String targetName; // ex: "F3" (note cible apres quantification)
        float midi      = 0.0f;  // MIDI float du pitch detecte
        float targetMidi= 0.0f;  // MIDI float de la note cible
        int   octave    = 0;     // octave de la note detectee
        int   noteInOct = 0;     // 0..11
        float cents     = 0.0f;  // deviation du pitch detecte vs cible (cible = 0)
        bool  valid     = false; // false si hzIn <= 0
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
        // On prend l'offset dans l'octave pour le pitch detecte.
        const float frac = info.midi - std::floor (info.midi);
        info.cents = (frac - std::round (frac)) * 100.0f;
        // Si on depasse 50 cents en valeur absolue, on rapporte par rapport
        // a la note adjacente la plus proche.
        if (info.cents > 50.0f)  info.cents -= 100.0f;
        if (info.cents < -50.0f) info.cents += 100.0f;
        return info;
    }
}



