#pragma once
// ScaleSnapPipelineTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "../Source/dsp/ScaleQuantizer.h"

static juce::String intervalString (const juce::Array<int>& a)
{
    juce::String s = "{ ";
    for (int i = 0; i < a.size(); ++i) { if (i) s << ", "; s << a[i]; }
    s << " }";
    return s;
}

class ScaleSnapPipelineTest : public juce::UnitTest
{
public:
    ScaleSnapPipelineTest() : juce::UnitTest ("ScaleSnapPipeline", "OVT") {}

    // Replicates scaleBox.onChange: for a named scale, write the 12 customN
    // flags from a temp quantizer built for (key, scale).
    void scaleBoxOnChange (int idx, juce::Array<int>& customN, int keyIdx)
    {
        if (idx < 0 || idx == 13) return; // Custom handled by the keyboard
        ovtdsp::ScaleQuantizer temp;
        temp.setKey (keyIdx);
        temp.setScale (static_cast<ovtdsp::Scale> (juce::jlimit (0, 13, idx)));
        auto intervals = temp.getScaleIntervals();
        for (int i = 0; i < 12; ++i)
            customN.set (i, intervals.contains (i) ? 1 : 0);
    }

    // Replicates syncParameters scale resolution.
    juce::Array<int> computeIntervals (int scaleIdx, int keyIdx, const juce::Array<int>& customN)
    {
        ovtdsp::ScaleQuantizer q;
        q.setKey (keyIdx);
        if (scaleIdx == 13) // Custom
        {
            juce::Array<int> customNotes;
            for (int i = 0; i < 12; ++i)
                if (customN[i] > 0) customNotes.add (i);
            q.setCustomIntervals (customNotes);
            q.setScale (ovtdsp::Scale::Custom);
        }
        else
        {
            q.setScale (static_cast<ovtdsp::Scale> (juce::jlimit (0, 15, scaleIdx)));
        }
        return q.getScaleIntervals();
    }

    // Replicates the toolbar scale-keyboard onUserInteraction (CURRENT code):
    // switches the scale param to Custom (index 13) and the combo display
    // to the last item, both without notification.
    void onUserInteractionCurrent (int& scaleIdx, int& comboIdx)
    {
        if (std::abs (static_cast<float> (scaleIdx) / 13.0f - 1.0f) > 0.01f)
        {
            scaleIdx = 13;
            comboIdx = 13; // dontSendNotification -> combo diverges from param
        }
    }

    void runTest() override
    {
        const int keyIdx = 0; // C

        beginTest ("Selecting Natural Minor snaps on all 7 natural-minor notes");
        {
            int scaleIdx = 4;            // param selected by combo
            int comboIdx = 4;            // combo display
            juce::Array<int> customN (12);
            scaleBoxOnChange (scaleIdx, customN, keyIdx);

            expectEquals (comboIdx, 4);
            auto intervals = computeIntervals (scaleIdx, keyIdx, customN);
            logMessage ("naturalMinorIntervals=" + intervalString (intervals));
            expect (intervals == juce::Array<int> { 0, 2, 3, 5, 7, 8, 10 });
            // D4(2), G4(7), A#4(10) are present => they snap.
            expect (intervals.contains (2));
            expect (intervals.contains (7));
            expect (intervals.contains (10));
        }

        beginTest ("Clicking scale-keyboard notes removes them and enters Custom mode");
        {
            int scaleIdx = 4, comboIdx = 4;
            juce::Array<int> customN (12);
            scaleBoxOnChange (scaleIdx, customN, keyIdx); // customN = natural minor

            // Toggle D(2), G(7), A#(10) OFF, as the keyboard buttons would.
            for (int note : { 2, 7, 10 })
            {
                customN.set (note, 0);
                onUserInteractionCurrent (scaleIdx, comboIdx);
            }

            logMessage ("comboIdx=" + juce::String (comboIdx)
                        + " scaleIdx=" + juce::String (scaleIdx));
            logMessage ("intervalsAfterClicks=" + intervalString (computeIntervals (scaleIdx, keyIdx, customN)));
            expectEquals (comboIdx, 13);   // combo shows Custom
            expectEquals (scaleIdx, 13);   // param is Custom
            auto intervals = computeIntervals (scaleIdx, keyIdx, customN);
            // Custom {C, Eb, F, Ab} => {0, 3, 5, 8}: D/G/A# are now absent.
            expect (intervals == juce::Array<int> { 0, 3, 5, 8 });
            expect (! intervals.contains (2));
            expect (! intervals.contains (7));
            expect (! intervals.contains (10));
        }

        beginTest ("Re-selecting Natural Minor from the combo restores snapping");
        {
            int scaleIdx = 4, comboIdx = 4;
            juce::Array<int> customN (12);
            scaleBoxOnChange (scaleIdx, customN, keyIdx);
            auto intervals = computeIntervals (scaleIdx, keyIdx, customN);
            expect (intervals == juce::Array<int> { 0, 2, 3, 5, 7, 8, 10 });
        }
    }
};

static ScaleSnapPipelineTest scaleSnapPipelineTest;


