#pragma once
// KeyDetectorTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/KeyDetector.h"
#include "../../Source/dsp/ScaleQuantizer.h"

namespace
{
    // Build an audible Hz for a given pitch class (0=C .. 11=B) in octave 4.
    float pcToHz (int pc)
    {
        return ovtdsp::semitonesToHz (static_cast<float> ((pc - 9) + 12 * 4));
    }
}

class KeyDetectorTest : public juce::UnitTest
{
public:
    KeyDetectorTest() : juce::UnitTest ("KeyDetector") {}

    void runTest() override
    {
        const float blockDur = 0.05f; // 50 ms per block

        beginTest ("Empty / silence yields no estimate");
        {
            ovtdsp::KeyDetector d;
            d.setWindowSeconds (3.0f);
            int key = -1; bool minor = false; float conf = 0.0f;
            for (int i = 0; i < 20; ++i)
                d.addDetection (0.0f, 0.0f, blockDur); // silence only
            expect (!d.getEstimate (key, minor, conf), "silence must not produce an estimate");
        }

        beginTest ("C Major scale is detected as key C, major");
        {
            ovtdsp::KeyDetector d;
            d.setWindowSeconds (3.0f);
            // C major pitch classes (relative to C): C D E F G A B.
            // Tonic (C) and dominant (G) are emphasised, as in real music, to
            // resolve the relative-major/minor ambiguity. getEstimate is called
            // every block, exactly like in the real-time plug-in, so the EMA
            // steady state (not the last note's transient) drives the decision.
            // The detector works in an A-relative frame (A = pc 0), so the
            // expected detector key for C major is 3 (C is 3 semitones above A).
            const int major[] = { 0, 2, 4, 5, 7, 9, 11 };
            int key = -1; bool minor = false; float conf = 0.0f;
            bool got = false;
            int noteIdx = 0;
            for (int block = 0; block < 500 && !got; ++block)
            {
                const int pc = major[noteIdx % 7];
                ++noteIdx;
                float strength = 1.0f;
                if (pc == 0) strength = 3.0f;       // emphasise tonic C
                else if (pc == 7) strength = 2.0f;  // emphasise dominant G
                d.addDetection (pcToHz (pc), strength, blockDur);
                got = d.getEstimate (key, minor, conf);
            }
            expect (got, "C major should converge to an estimate");
            expectEquals (key, 3, "C major -> detector key 3 (A-relative)");
            expect (!minor, "C major -> major mode");
            expect (conf > 0.0f, "confidence should be positive");
        }

        beginTest ("A Natural Minor scale is detected as key A, minor");
        {
            ovtdsp::KeyDetector d;
            d.setWindowSeconds (3.0f);
            // A natural minor pitch classes (relative to C): A B C D E F G.
            // Tonic (A) and dominant (E) emphasised. In the A-relative detector
            // frame the expected detector key for A minor is 0 (A itself).
            const int minor[] = { 9, 11, 0, 2, 4, 5, 7 };
            int key = -1; bool isMinor = false; float conf = 0.0f;
            bool got = false;
            int noteIdx = 0;
            for (int block = 0; block < 500 && !got; ++block)
            {
                const int pc = minor[noteIdx % 7];
                ++noteIdx;
                float strength = 1.0f;
                if (pc == 9) strength = 3.0f;       // emphasise tonic A
                else if (pc == 4) strength = 2.0f;  // emphasise dominant E
                d.addDetection (pcToHz (pc), strength, blockDur);
                got = d.getEstimate (key, isMinor, conf);
            }
            expect (got, "A natural minor should converge to an estimate");
            expectEquals (key, 0, "A natural minor -> detector key 0 (A-relative)");
            expect (isMinor, "A natural minor -> minor mode");
        }

        beginTest ("Uniform chromatic input does not yield a confident estimate");
        {
            ovtdsp::KeyDetector d;
            d.setWindowSeconds (3.0f);
            int key = -1; bool minor = false; float conf = 0.0f;
            bool got = false;
            int pc = 0;
            for (int block = 0; block < 500 && !got; ++block)
            {
                d.addDetection (pcToHz (pc % 12), 1.0f, blockDur);
                ++pc;
                got = d.getEstimate (key, minor, conf);
            }
            expect (!got, "uniform chromatic content has no clear key");
        }

        beginTest ("reset() clears the accumulated profile");
        {
            ovtdsp::KeyDetector d;
            d.setWindowSeconds (3.0f);
            for (int i = 0; i < 30; ++i)
                d.addDetection (pcToHz (0), 1.0f, blockDur); // build C
            d.reset();
            int key = -1; bool minor = false; float conf = 0.0f;
            expect (!d.getEstimate (key, minor, conf), "after reset there must be no estimate");
        }
    }
};

static KeyDetectorTest keyDetectorTest;


