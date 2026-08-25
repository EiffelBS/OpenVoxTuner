#pragma once
// PitchCurveEditorTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/ui/PitchCurveEditor.h"

class PitchCurveEditorInteractionTest : public juce::UnitTest
{
public:
    PitchCurveEditorInteractionTest() : juce::UnitTest ("PitchCurveEditor.Interactions") {}

    void runTest() override
    {
        using ui::PitchCurveEditor;

        beginTest ("snapTimeToGrid: grid enabled (snapToGridEnabled = true)");
        {
            // Exactly on a grid line -> stays in place.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.0, true), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.5, true), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.0, true), 1.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (2.5, true), 2.5, 1e-9);

            // Halfway between two lines (0.25) -> rounded to the nearest
            // line ("round half away from zero" rule: 0.25 -> 0.5).
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.25, true), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.75, true), 1.0, 1e-9);

            // Arbitrary offset -> always aligned to the 1/2 beat grid.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.27, true), 1.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (3.61, true), 3.5, 1e-9);

            // Negative value (should not happen since xToTime is clamped >= 0)
            // -> quantized toward 0 by the rounding.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (-0.1, true), 0.0, 1e-9);
        }

        beginTest ("snapTimeToGrid: grid disabled (snapToGridEnabled = false)");
        {
            // On a grid line -> stays in place (tolerance 0.05).
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.0, false), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.5, false), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.0, false), 1.0, 1e-9);

            // Very close to a line (<= 0.05) -> still aligned (magnet).
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.52, false), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.48, false), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.03, false), 1.0, 1e-9);

            // Farther from a line than the tolerance -> raw time kept.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.25, false), 0.25, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.75, false), 0.75, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.3,  false), 1.3,  1e-9);
        }

        beginTest ("clampScrollOffset: clamps the horizontal offset to >= 0");
        {
            // Negative -> 0 (prevents scrolling before the start of the timeline).
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (-1.0), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (-5.0), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (-0.0001), 0.0, 1e-9);

            // Zero and positive -> pass-through.
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (0.0),  0.0,  1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (2.3),  2.3,  1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (12.75), 12.75, 1e-9);
        }
    }
};

static PitchCurveEditorInteractionTest pitchCurveEditorInteractionTest;


