// Main.cpp
// Point d'entree de l'application de tests unitaires.
// Utilise le framework UnitTest de JUCE.

#include <juce_audio_processors/juce_audio_processors.h>

// Inclusion des tests (les fichiers .cpp eux-memes s'enregistrent
// automatiquement via static PitchDetectorTest...).
#include "dsp/PitchDetectorTest.cpp"
#include "dsp/ScaleQuantizerTest.cpp"
#include "dsp/RetargetEnvelopeTest.cpp"
#include "dsp/FormantPreserverTest.cpp"
#include "dsp/PitchCurveTest.cpp"
#include "dsp/VibratoTest.cpp"
#include "dsp/AttackAwareTest.cpp"
#include "dsp/KeyDetectorTest.cpp"
#include "dsp/KeyBridgeTest.cpp"
#include "dsp/SidechainBusLayoutTest.cpp"
#include "ScaleSnapPipelineTest.cpp"

int main (int argc, char* argv[])
{
    juce::UnitTestRunner runner;
    runner.runAllTests();

    // Affiche un resume dans la console.
    int numPassed = 0, numFailed = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* r = runner.getResult (i);
        if (r->failures > 0)
        {
            numFailed++;
            std::cout << "[FAIL] " << r->unitTestName << "\n";
            for (auto& msg : r->messages)
                std::cout << "       " << msg << "\n";
        }
        else
        {
            numPassed++;
            std::cout << "[ OK ] " << r->unitTestName
                      << " (" << r->passes << " assertions)\n";
        }
    }

    std::cout << "\n=========================\n";
    std::cout << "Resultat : " << numPassed << " OK, " << numFailed << " KO\n";
    std::cout << "=========================\n";

    return numFailed == 0 ? 0 : 1;
}
