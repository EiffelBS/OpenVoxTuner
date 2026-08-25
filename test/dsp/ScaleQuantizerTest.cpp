#pragma once
// ScaleQuantizerTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/ScaleQuantizer.h"

class ScaleQuantizerTest : public juce::UnitTest
{
public:
    ScaleQuantizerTest() : juce::UnitTest ("ScaleQuantizer") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("Note already in scale (C major, A4 = 440 Hz)");
        {
            ScaleQuantizer q;
            q.setKey (0); // C
            q.setScale (Scale::Major);
            // A4 = 440 Hz is in the C major scale.
            const float f = q.quantize (440.0f);
            expectWithinAbsoluteError (f, 440.0f, 0.5f);
        }

        beginTest ("Out-of-scale note is brought back to the closest note");
        {
            ScaleQuantizer q;
            q.setKey (0); // C
            q.setScale (Scale::Major);

            juce::String s = "Intervals: ";
            for (int i : q.getScaleIntervals()) s += juce::String(i) + " ";
            logMessage (s);

            const float f = q.quantize (466.16f);
            logMessage ("Quantized 466.16 to: " + juce::String(f));
            expectWithinAbsoluteError (f, 440.0f, 1.0f);
        }

        beginTest ("Root transposition: A minor");
        {
            ScaleQuantizer q;
            q.setKey (9);  // A
            q.setScale (Scale::NaturalMinor);
            // 220 Hz (A3) -> in A minor, should stay ~220.
            const float f = q.quantize (220.0f);
            expectWithinAbsoluteError (f, 220.0f, 0.5f);
        }

        beginTest ("Chromatic scale");
        {
            ScaleQuantizer q;
            q.setKey (0); // C
            q.setScale (Scale::Chromatic);

            // In chromatic mode, ANY exact note must be preserved.
            // If the note is slightly off, it must be quantized (Autotune).
            // 440 Hz = A4 -> exact
            expectWithinAbsoluteError (q.quantize (440.0f), 440.0f, 0.01f);

            // 445 Hz -> must be corrected toward 440 Hz (A4) since chromatic mode is an Autotune.
            expectWithinAbsoluteError (q.quantize (445.0f), 440.0f, 0.01f);

            // 261.63 Hz = C4 -> exact
            expectWithinAbsoluteError (q.quantize (261.625565f), 261.625565f, 0.01f);

            // 266 Hz -> must be corrected toward 261.63 Hz (C4) or 277.18 (C#4)
            // The offset toward C4 is about 28 cents. C4 is the closest.
            expectWithinAbsoluteError (q.quantize (266.0f), 261.625565f, 0.01f);
            // 999 Hz -> B5 (987.77 Hz) or C6 (1046.50 Hz)
            expectWithinAbsoluteError (q.quantize (999.0f), 987.7666f, 0.01f);
        }

        beginTest ("f0 = 0 -> returns 0");
        {
            ScaleQuantizer q;
            const float f = q.quantize (0.0f);
            expectEquals (f, 0.0f);
        }

        beginTest ("Major pentatonic: correct notes");
        {
            ScaleQuantizer q;
            q.setKey (0);
            q.setScale (Scale::MajorPentatonic);
            // C major pentatonic: C D E G A
            // 261.63 (C4) -> in
            // 293.66 (D4) -> in
            // 329.63 (E4) -> in
            // 392.00 (G4) -> in
            // 440.00 (A4) -> in
            // 349.23 (F4) -> must be pulled back (D# = 311 or G = 392; F is closest to E)
            const float c = q.quantize (261.63f);
            const float f_sharp = q.quantize (349.23f); // should be ~329.63 (E)
            expectWithinAbsoluteError (c, 261.63f, 0.5f);
            expectWithinAbsoluteError (f_sharp, 329.63f, 1.5f);
        }

        beginTest ("Root modulo 12 (12 = C, 13 = C#, etc.)");
        {
            ScaleQuantizer q;
            q.setKey (14); // 14 % 12 = 2 -> D
            q.setScale (Scale::Major);
            // The root should be D.
            // 293.66 (D4) -> should be kept.
            const float d = q.quantize (293.66f);
            expectWithinAbsoluteError (d, 293.66f, 0.5f);
        }
    }
};

static ScaleQuantizerTest scaleQuantizerTest;


