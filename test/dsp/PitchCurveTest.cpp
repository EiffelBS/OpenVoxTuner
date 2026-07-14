// PitchCurveTest.cpp
// Tests unitaires de la courbe de pitch, en particulier snapToIntervals()
// qui pilote le "snap-to-scale" de l'editeur de courbe.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/PitchCurve.h"

class PitchCurveTest : public juce::UnitTest
{
public:
    PitchCurveTest() : juce::UnitTest ("PitchCurve") {}

    void runTest() override
    {
        using namespace ovtdsp;

        // Gamme Do Naturel Mineur : {C, D, Eb, F, G, Ab, Bb} = {0,2,3,5,7,8,10}
        juce::Array<int> naturalMinor;
        for (int i : { 0, 2, 3, 5, 7, 8, 10 })
            naturalMinor.add (i);

        // Frequences exactes (A4 = 440 Hz).
        const float d4  = 440.0f * std::pow (2.0f, (62 - 69) / 12.0f); // D4
        const float g4  = 440.0f * std::pow (2.0f, (67 - 69) / 12.0f); // G4
        const float aS4 = 440.0f * std::pow (2.0f, (70 - 69) / 12.0f); // A#4

        beginTest ("Note de la gamme cliquee exactement : reste sur la note");
        {
            expectWithinAbsoluteError (PitchCurve::snapToIntervals (d4,  naturalMinor), d4,  0.5f);
            expectWithinAbsoluteError (PitchCurve::snapToIntervals (g4,  naturalMinor), g4,  0.5f);
            expectWithinAbsoluteError (PitchCurve::snapToIntervals (aS4, naturalMinor), aS4, 0.5f);
        }

        beginTest ("Note de la gamme cliquee legerement a cote : SNAP vers la note exacte");
        {
            // Regression : avant le correctif, snapToIntervals renvoyait la valeur
            // brute cliquee (ex: 295 Hz) au lieu de la note exacte (D4 ~293.66 Hz),
            // donc le point ne semblait pas snapper pour les notes de la gamme.
            const float clickedSharpD  = 295.0f;
            const float clickedSharpG  = 394.0f;
            const float clickedFlatA   = 463.0f;

            const float snappedD = PitchCurve::snapToIntervals (clickedSharpD, naturalMinor);
            const float snappedG = PitchCurve::snapToIntervals (clickedSharpG, naturalMinor);
            const float snappedA = PitchCurve::snapToIntervals (clickedFlatA,  naturalMinor);

            // Doit revenir a la note exacte, PAS a la valeur brute cliquee.
            expectWithinAbsoluteError (snappedD, d4,  0.5f);
            expectWithinAbsoluteError (snappedG, g4,  0.5f);
            expectWithinAbsoluteError (snappedA, aS4, 0.5f);

            logMessage ("D4  clicked=" + juce::String (clickedSharpD)
                        + " snapped=" + juce::String (snappedD)
                        + " exact=" + juce::String (d4));
            logMessage ("G4  clicked=" + juce::String (clickedSharpG)
                        + " snapped=" + juce::String (snappedG)
                        + " exact=" + juce::String (g4));
            logMessage ("A#4 clicked=" + juce::String (clickedFlatA)
                        + " snapped=" + juce::String (snappedA)
                        + " exact=" + juce::String (aS4));
        }

        beginTest ("Note hors-gamme : snap vers la note de gamme la plus proche");
        {
            // E4 (329.63 Hz, note 4) n'est pas dans Do Nat. Mineur -> doit
            // revenir a Eb4 (311.13) ou F4 (349.23) ; le plus proche est Eb4.
            const float e4 = 329.63f;
            const float snapped = PitchCurve::snapToIntervals (e4, naturalMinor);
            const float eb4 = 440.0f * std::pow (2.0f, (63 - 69) / 12.0f);
            expectWithinAbsoluteError (snapped, eb4, 1.0f);
        }

        beginTest ("Ensemble d'intervalles vide : renvoie la valeur brute");
        {
            juce::Array<int> empty;
            const float hz = 293.66f;
            expectWithinAbsoluteError (PitchCurve::snapToIntervals (hz, empty), hz, 0.001f);
        }
    }
};

static PitchCurveTest pitchCurveTest;
