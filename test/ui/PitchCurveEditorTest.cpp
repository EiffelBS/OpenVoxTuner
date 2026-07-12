// PitchCurveEditorTest.cpp
// Tests unitaires des interactions souris de l'editeur de courbe :
//   - snapTimeToGrid  : quantification temporelle du clic sur la regle
//                       (grille de projet, 1/2 beat = noire).
//   - clampScrollOffset : bornage du decalage de scroll horizontal (>= 0).
//
// Ces deux methodes sont statiques/inline dans PitchCurveEditor.h, donc
// testables sans lier l'unite de compilation de l'editeur (pas de GUI a
// instancier). Elles ne dependent que de <cmath>.

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

        beginTest ("snapTimeToGrid : grille active (snapToGridEnabled = true)");
        {
            // Sur une ligne de grille exacte -> reste sur place.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.0, true), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.5, true), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.0, true), 1.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (2.5, true), 2.5, 1e-9);

            // Au milieu entre deux lignes (0.25) -> arrondi a la ligne la plus
            // proche (regle "round half away from zero" : 0.25 -> 0.5).
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.25, true), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.75, true), 1.0, 1e-9);

            // Decalage quelconque -> toujours aligne sur la grille 1/2 beat.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.27, true), 1.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (3.61, true), 3.5, 1e-9);

            // Valeur negative (ne devrait pas arriver car xToTime est borne >= 0)
            // -> quantifiee vers 0 par l'arrondi.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (-0.1, true), 0.0, 1e-9);
        }

        beginTest ("snapTimeToGrid : grille inactive (snapToGridEnabled = false)");
        {
            // Sur une ligne de grille -> reste sur place (tolerance 0.05).
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.0, false), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.5, false), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.0, false), 1.0, 1e-9);

            // Tres proche d'une ligne (<= 0.05) -> aligne quand meme (aimant).
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.52, false), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.48, false), 0.5, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.03, false), 1.0, 1e-9);

            // Plus loin qu'une ligne que la tolerance -> temps brut conserve.
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.25, false), 0.25, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (0.75, false), 0.75, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::snapTimeToGrid (1.3,  false), 1.3,  1e-9);
        }

        beginTest ("clampScrollOffset : borne le decalage horizontal a >= 0");
        {
            // Negatif -> 0 (empeche de defiler avant le debut de la timeline).
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (-1.0), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (-5.0), 0.0, 1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (-0.0001), 0.0, 1e-9);

            // Zero et positif -> passe-plat.
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (0.0),  0.0,  1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (2.3),  2.3,  1e-9);
            expectWithinAbsoluteError (PitchCurveEditor::clampScrollOffset (12.75), 12.75, 1e-9);
        }
    }
};

static PitchCurveEditorInteractionTest pitchCurveEditorInteractionTest;
