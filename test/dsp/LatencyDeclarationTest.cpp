#pragma once
// LatencyDeclarationTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The PDC declaration must be the measured effective delay of the OLA shifter at
// the selected Voice Type's floor, not the internal mode value. Two invariants
// make the table trustworthy:
//   - a higher latency mode never declares less than a lower one;
//   - a lower register never declares less than a higher one (the delay grows
//     with the period), and Universal declares the worst (Bass) case.
// The sample conversion is what the host actually receives, so it is pinned per
// sample rate here. The raw measurements live in
// docs/low-latency-shifter-analysis.md (Section 1.3) and
// tools/bench_pitchshift_latency.cpp (--latency-matrix).

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/LatencyDeclaration.h"

class LatencyDeclarationTest : public juce::UnitTest
{
public:
    LatencyDeclarationTest() : juce::UnitTest ("LatencyDeclaration") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("a higher latency mode never declares less than a lower one");
        for (int v = 0; v < kLatencyDeclarationVoiceTypes; ++v)
            for (int m = 1; m < kLatencyDeclarationModes; ++m)
                expect (declaredLatencyMs (m, v) >= declaredLatencyMs (m - 1, v),
                        "mode " + juce::String (m) + " declares " + juce::String (declaredLatencyMs (m, v))
                        + " ms, less than mode " + juce::String (m - 1) + " ("
                        + juce::String (declaredLatencyMs (m - 1, v)) + " ms) for voice type "
                        + juce::String (v));

        beginTest ("a lower register never declares less than a higher one");
        for (int m = 0; m < kLatencyDeclarationModes; ++m)
            for (int v = 1; v < kLatencyDeclarationVoiceTypes; ++v)
                expect (declaredLatencyMs (m, v) <= declaredLatencyMs (m, v - 1),
                        "voice type " + juce::String (v) + " declares more than the lower register "
                        + juce::String (v - 1) + " for mode " + juce::String (m));

        beginTest ("Universal declares the Bass floor (the worst supported case)");
        for (int m = 0; m < kLatencyDeclarationModes; ++m)
            expect (declaredLatencyMs (m, 0) == declaredLatencyMs (m, 1),
                    "Universal must use the Bass declaration for mode " + juce::String (m));

        beginTest ("declared values are sane");
        for (int m = 0; m < kLatencyDeclarationModes; ++m)
            for (int v = 0; v < kLatencyDeclarationVoiceTypes; ++v)
            {
                const int ms = declaredLatencyMs (m, v);
                expect (ms >= 10 && ms <= 60,
                        "declared latency " + juce::String (ms) + " ms is outside [10, 60] (mode "
                        + juce::String (m) + ", voice type " + juce::String (v) + ")");
            }

        beginTest ("sample conversion matches the millisecond value");
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int m = 0; m < kLatencyDeclarationModes; ++m)
                for (int v = 0; v < kLatencyDeclarationVoiceTypes; ++v)
                {
                    const int expected = (int) std::lround (declaredLatencyMs (m, v) * sr / 1000.0);
                    expect (declaredLatencySamples (sr, m, v) == expected,
                            "sample conversion at " + juce::String (sr) + " Hz, mode " + juce::String (m)
                            + ", voice type " + juce::String (v));
                }

        beginTest ("out-of-range indices are clamped");
        expect (declaredLatencyMs (-1, -1) == declaredLatencyMs (0, 0), "negative indices clamp low");
        expect (declaredLatencyMs (99, 99) == declaredLatencyMs (kLatencyDeclarationModes - 1,
                                                                kLatencyDeclarationVoiceTypes - 1),
                "oversized indices clamp high");
    }
};

static LatencyDeclarationTest latencyDeclarationTest;
