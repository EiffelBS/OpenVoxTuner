// ScaleQuantizer.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// We use "ovtdsp" (autotune dsp) rather than "dsp" to avoid any ambiguity
// with the "juce::dsp" namespace brought in by JuceHeader.h.
namespace ovtdsp
{
    /**
     * Musical modes supported by the plugin.
     * Indices match the values of the "scale" parameter.
     */
    enum class Scale
    {
        Chromatic       = 0,
        Major           = 1,
        MelodicMinor    = 2,
        HarmonicMinor   = 3,
        NaturalMinor    = 4,
        MajorPentatonic = 5,
        MinorPentatonic = 6,
        Blues           = 7,
        Dorian          = 8,
        Phrygian        = 9,
        Lydian          = 10,
        Mixolydian      = 11,
        Locrian         = 12,
        Custom          = 13
    };

    /**
     * Converts a frequency (Hz) into semitones relative to A4 (440 Hz).
     */
    inline float hzToSemitones (float hz)
    {
        if (hz <= 0.0f) return -100.0f;
        return 12.0f * std::log2 (hz / 440.0f);
    }

    /**
     * Converts semitones relative to A4 into a frequency (Hz).
     */
    inline float semitonesToHz (float semi)
    {
        return 440.0f * std::pow (2.0f, semi / 12.0f);
    }

    /**
     * Quantizer: takes a detected pitch and returns the closest target
     * pitch in the selected scale.
     */
    class ScaleQuantizer
    {
    public:
        ScaleQuantizer();

        /// Sets the tonic/key (0=C, 1=C#, ..., 11=B).
        void setKey (int keyInSemitones);

        /// Sets the mode / scale.
        void setScale (Scale scale);

        /// Sets the custom scale (12 booleans, 0=C, 1=C#, ..., 11=B).
        /// No effect if the current mode is not Custom.
        void setCustomIntervals (const juce::Array<int>& notesInSemitones);

        /// Quantizes a frequency to the closest note of the scale.
        /// @param hzIn          detected frequency in Hz.
        /// @param positionPPQ   current position in PPQ time (quarter notes). Used
        ///                       only to evaluate a possible chord-context override
        ///                       (see setChordOverride).
        /// @return target frequency in Hz, or hzIn if no note is in the scale.
        float quantize (float hzIn, double positionPPQ = 0.0) const;

        /// Returns the current list of scale notes (in semitones 0..11
        /// relative to C). Combines key+scale (or custom scale).
        const juce::Array<int>& getScaleIntervals() const { return intervals; }

        /// Indicates whether the scale is in custom mode.
        bool isCustom() const { return currentScale == Scale::Custom; }

        // === Chord context override (ARA) ===
        // For now, notes belonging to an out-of-scale chord are accepted
        // (they glide to their exact pitch) even if they are not in the
        // current scale. Each window is valid over a PPQ time range given
        // by updateAraMetadata().

        /// Defines (adds) an override window for a chord.
        /// @p chordPitchClasses  pitch classes (0..11) of the chord (root, third,
        ///                        fifth, ... and the bass note if different).
        /// @p positionPPQ        window start position (quarter notes).
        /// @p durationPPQ        validity duration (quarter notes). A value > 0.
        void setChordOverride (const juce::Array<int>& chordPitchClasses,
                               double positionPPQ,
                               double durationPPQ);

        /// Resets all override windows.
        void clearChordOverrides();

        /// True if an override window covers @p positionPPQ.
        bool isChordOverrideActive (double positionPPQ) const;

        /// True if @p pitchClass (0..11) is allowed at time @p pos:
        /// scale note, OR active chord containing this class.
        bool isNoteAllowed (int pitchClass, double pos) const;

        // === "Live" chord override (real-time: MIDI / sidechain) ===
        // Represents the current chord detected in real time (MIDI or
        // sidechain source). It has PRIORITY over ARA windows when active.
        // When inactive, we fall back to the ARA windows (chord track).

        /// Sets the current live chord (pitch classes 0..11).
        void setLiveChordOverride (const juce::Array<int>& chordPitchClasses);

        /// Disables the live chord (back to ARA windows).
        void clearLiveChordOverride();

        /// True if a live chord is currently active.
        bool isLiveChordActive() const;

        /// True if the active chord override at @p pos contains at least one
        /// pitch class OUTSIDE the current scale (the override thus widens
        /// the allowed notes beyond the scale). False if no active chord,
        /// or if all chord tones are within the scale.
        bool isActiveChordOutOfScale (double pos) const;

    private:
        int key = 0;       // 0-11
        Scale currentScale = Scale::Chromatic;
        juce::Array<int> intervals;        // semitones relative to C belonging to the scale
        juce::Array<int> customIntervals;  // custom scale (without key offset)

        // Rebuilds the interval array from (key, scale).
        void rebuildIntervals();

        /// Chord override window (UI thread writes / audio thread reads).
        struct ChordWindow
        {
            juce::Array<int> chordNotes;  // pitch classes 0..11 of the chord
            double startPPQ  = 0.0;
            double duration  = 0.0;       // in quarter notes; 0 = inactive
        };

        /// True if @p pitchClass is a chord tone covering @p pos (without
        /// re-testing the scale - called by isNoteAllowed/quantize).
        bool isChordToneAt (int pitchClass, double pos) const;

        mutable juce::CriticalSection chordOverrideLock;
        juce::Array<ChordWindow> chordWindows;

        /// Current "live" chord (real-time MIDI/sidechain), with priority
        /// over ARA windows. Empty + liveChordActive=false = inactive.
        juce::Array<int> liveChordNotes;
        bool liveChordActive = false;
    };
}



