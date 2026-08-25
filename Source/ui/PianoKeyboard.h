// PianoKeyboard.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/NoteUtils.h"

namespace ui
{
    /**
     * Vertical piano keyboard ("scrollable piano roll" style).
     * - Draws white keys in the background, black keys on top.
     * - Highlights notes that belong to the scale.
     * - Optional: a "playhead" cursor following the current pitch.
     */
    class PianoKeyboard : public juce::Component
    {
    public:
        PianoKeyboard();
        ~PianoKeyboard() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        /// Sets the displayed note range (inclusive, in MIDI).
        void setRange (int lowestMidi, int highestMidi);

        /// Returns the current range.
        int getLowestMidi()  const { return lowestMidi; }
        int getHighestMidi() const { return highestMidi; }

        /// Sets the scale notes (semitones 0..11 relative to C).
        void setScaleIntervals (const juce::Array<int>& intervals);

        /// Sets the current pitches (used for key highlighting).
        void setCurrentPitches (float inputHz, float outputHz);

        /// Returns the Y position (in pixels) of a given MIDI note.
        float midiToY (int midi) const;

        /// Returns the MIDI note matching a given Y in pixels.
        int   yToMidi (float y) const;

        /// Returns the pitch (Hz) matching a given Y in pixels.
        float yToHz (float y) const;

        /// Normalized position [0,1] (0 = low/bottom, 1 = high/top) of a MIDI note within
        /// a [lowestMidi, highestMidi] range, using piano geometry: all white keys
        /// share the same height, and black keys are centered on
        /// the boundary between their two neighboring white keys (consistent height).
        static float midiToNorm (int midi, int lowestMidi, int highestMidi);

        /// Returns true if the MIDI note is a black key.
        static bool isBlackKey (int midi) noexcept;

    private:
        int lowestMidi  = 36;  // C2
        int highestMidi = 96;  // C7

        juce::Array<int> scaleIntervals; // semitones 0..11 in the scale
        float currentInputHz = 0.0f;
        float currentOutputHz = 0.0f;

        // Colors.
        static const juce::Colour kWhiteKey;
        static const juce::Colour kBlackKey;
        static const juce::Colour kWhiteKeyScale; // white key belonging to the scale
        static const juce::Colour kBlackKeyScale;
        static const juce::Colour kBorder;
        static const juce::Colour kText;

        // Returns true if the MIDI note belongs to the current scale.
        bool isInScale (int midi) const noexcept;

        // Key color (with or without scale).
        juce::Colour getKeyColour (int midi, bool black) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboard)
    };
}



