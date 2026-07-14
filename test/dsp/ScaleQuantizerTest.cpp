// ScaleQuantizerTest.cpp
// Tests unitaires du quantificateur de gamme.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/ScaleQuantizer.h"

class ScaleQuantizerTest : public juce::UnitTest
{
public:
    ScaleQuantizerTest() : juce::UnitTest ("ScaleQuantizer") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("Note deja dans la gamme (Do majeur, A4 = 440 Hz)");
        {
            ScaleQuantizer q;
            q.setKey (0); // C
            q.setScale (Scale::Major);
            // A4 = 440 Hz est dans la gamme de Do majeur.
            const float f = q.quantize (440.0f);
            expectWithinAbsoluteError (f, 440.0f, 0.5f);
        }

        beginTest ("Note hors gamme est ramenee a la note la plus proche");
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

        beginTest ("Transposition de tonique : La mineur");
        {
            ScaleQuantizer q;
            q.setKey (9);  // A
            q.setScale (Scale::NaturalMinor);
            // 220 Hz (A3) -> dans La mineur, devrait rester ~220.
            const float f = q.quantize (220.0f);
            expectWithinAbsoluteError (f, 220.0f, 0.5f);
        }

        beginTest ("Echelle chromatique");
        {
            ScaleQuantizer q;
            q.setKey (0); // C
            q.setScale (Scale::Chromatic);

            // En mode chromatique, TOUTE note exacte doit etre preservee.
            // Si la note est legerement fausse, elle doit etre quantifiee (Autotune).
            // 440 Hz = A4 -> exact
            expectWithinAbsoluteError (q.quantize (440.0f), 440.0f, 0.01f);
            
            // 445 Hz -> doit etre corrige vers 440 Hz (A4) car le mode chromatique est un Autotune.
        expectWithinAbsoluteError (q.quantize (445.0f), 440.0f, 0.01f);
            
            // 261.63 Hz = C4 -> exact
            expectWithinAbsoluteError (q.quantize (261.625565f), 261.625565f, 0.01f);
            
            // 266 Hz -> doit etre corrige vers 261.63 Hz (C4) ou 277.18 (C#4)
            // L'ecart vers C4 est d'environ 28 cents. C4 est le plus proche.
            expectWithinAbsoluteError (q.quantize (266.0f), 261.625565f, 0.01f);
            // 999 Hz -> B5 (987.77 Hz) ou C6 (1046.50 Hz)
            expectWithinAbsoluteError (q.quantize (999.0f), 987.7666f, 0.01f);
        }

        beginTest ("f0 = 0 -> retourne 0");
        {
            ScaleQuantizer q;
            const float f = q.quantize (0.0f);
            expectEquals (f, 0.0f);
        }

        beginTest ("Pentatonique majeure : notes correctes");
        {
            ScaleQuantizer q;
            q.setKey (0);
            q.setScale (Scale::MajorPentatonic);
            // Pentatonique majeure de Do : C D E G A
            // 261.63 (C4) -> in
            // 293.66 (D4) -> in
            // 329.63 (E4) -> in
            // 392.00 (G4) -> in
            // 440.00 (A4) -> in
            // 349.23 (F4) -> doit etre ramene (D# = 311 ou G = 392 ; F est plus proche de E)
            const float c = q.quantize (261.63f);
            const float f_sharp = q.quantize (349.23f); // devrait etre ~329.63 (E)
            expectWithinAbsoluteError (c, 261.63f, 0.5f);
            expectWithinAbsoluteError (f_sharp, 329.63f, 1.5f);
        }

        beginTest ("Modulo 12 de la tonique (12 = C, 13 = C#, etc.)");
        {
            ScaleQuantizer q;
            q.setKey (14); // 14 % 12 = 2 -> re
            q.setScale (Scale::Major);
            // La tonique devrait etre Re (D).
            // 293.66 (D4) -> devrait etre conserve.
            const float d = q.quantize (293.66f);
            expectWithinAbsoluteError (d, 293.66f, 0.5f);
        }
    }
};

static ScaleQuantizerTest scaleQuantizerTest;
