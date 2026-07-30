#pragma once
// KeyBridgeTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/KeyBridge.h"

class KeyBridgeTest : public juce::UnitTest
{
public:
    KeyBridgeTest() : juce::UnitTest ("KeyBridge") {}

    void runTest() override
    {
        beginTest ("Unknown group has no published value");
        {
            ovtdsp::KeyBridge& b = ovtdsp::KeyBridge::getInstance();
            int key = -1, scale = -1; double ts = 0.0;
            expect (!b.read ("nonexistent-group-xyz", key, scale, ts),
                    "unpublished group must return false");
        }

        beginTest ("Companion publish is readable by the main instance");
        {
            ovtdsp::KeyBridge& b = ovtdsp::KeyBridge::getInstance();
            const juce::String group ("test-group-A");
            b.publish (group, 9, 4); // A natural minor (key 9, scale index 4)

            int key = -1, scale = -1; double ts = 0.0;
            expect (b.read (group, key, scale, ts), "published group must be readable");
            expectEquals (key, 9, "key should round-trip");
            expectEquals (scale, 4, "scale should round-trip");
            expect (ts > 0.0, "timestamp should be set");
        }

        beginTest ("Groups are independent");
        {
            ovtdsp::KeyBridge& b = ovtdsp::KeyBridge::getInstance();
            b.publish ("grp-1", 0, 1);
            b.publish ("grp-2", 5, 6);
            int k1 = -1, s1 = -1; double t1 = 0.0;
            int k2 = -1, s2 = -1; double t2 = 0.0;
            expect (b.read ("grp-1", k1, s1, t1));
            expect (b.read ("grp-2", k2, s2, t2));
            expectEquals (k1, 0); expectEquals (s1, 1);
            expectEquals (k2, 5); expectEquals (s2, 6);
        }

        beginTest ("Re-publishing a changed key/scale updates the slot");
        {
            ovtdsp::KeyBridge& b = ovtdsp::KeyBridge::getInstance();
            const juce::String group ("reuse-group");
            b.publish (group, 2, 1);
            int k = -1, s = -1; double t = 0.0;
            expect (b.read (group, k, s, t));
            expectEquals (k, 2); expectEquals (s, 1);

            b.publish (group, 7, 4);
            expect (b.read (group, k, s, t));
            expectEquals (k, 7); expectEquals (s, 4);
        }

        beginTest ("A/B/C/D groups map to independent slots (regression for group cross-talk)");
        {
            ovtdsp::KeyBridge& b = ovtdsp::KeyBridge::getInstance();
            // Simulate 4 companion instances, one per group, each with a
            // different detected key/scale.
            b.publish ("A", 4, 4);  // E natural minor
            b.publish ("B", 3, 4);  // D# natural minor
            b.publish ("C", 0, 1);  // C major (placeholder for "no detection" case)
            b.publish ("D", 0, 1);  // C natural minor

            int kA = -1, sA = -1; double tA = 0.0;
            int kB = -1, sB = -1; double tB = 0.0;
            int kC = -1, sC = -1; double tC = 0.0;
            int kD = -1, sD = -1; double tD = 0.0;

            expect (b.read ("A", kA, sA, tA), "group A must be readable");
            expect (b.read ("B", kB, sB, tB), "group B must be readable");
            expect (b.read ("C", kC, sC, tC), "group C must be readable");
            expect (b.read ("D", kD, sD, tD), "group D must be readable");

            expectEquals (kA, 4); expectEquals (sA, 4);  // E natural minor
            expectEquals (kB, 3); expectEquals (sB, 4);  // D# natural minor
            expectEquals (kC, 0); expectEquals (sC, 1);  // C major
            expectEquals (kD, 0); expectEquals (sD, 1);  // C natural minor

            // Cross-talk check: B must NOT read D's value.
            expect (kB != kD || sB != sD, "group B must not equal group D");
        }
    }
};

static KeyBridgeTest keyBridgeTest;


