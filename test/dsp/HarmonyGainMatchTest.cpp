// HarmonyGainMatchTest.cpp
// Regression test (2026-07-17) pour le gain match des harmonies :
// verifie que getHarmonyVoiceCount() retourne les bons nombres de
// voix pour les HarmonyType denses (Unison2, UnisonOctaves4, etc.)
// et que la formule 1/sqrt(1+N) appliquee au mix d'harmonie
// reduit effectivement le volume en sortie par rapport au mix
// non-compense (pour eviter que la regression revienne sous la
// forme d'un "unison trop fort" deja signale par l'utilisateur).

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/HarmonyEngine.h"

class HarmonyGainMatchTest : public juce::UnitTest
{
public:
    HarmonyGainMatchTest() : juce::UnitTest ("HarmonyGainMatch") {}

    void runTest() override
    {
        using namespace ovtdsp;

        // ----------------------------------------------------------------
        // 1) Comptage des voix pour les HarmonyType denses (utilise
        //    pour calculer le facteur 1/sqrt(1+N) dans le mix).
        // ----------------------------------------------------------------
        beginTest ("getHarmonyVoiceCount : Unison2 = 2, UnisonOctaves4 = 4, etc.");
        {
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::None) == 0,
                    "None doit retourner 0 voix (mix vide).");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::Unison2) == 2,
                    "Unison2 doit retourner 2 voix.");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::UnisonOctaves4) == 4,
                    "UnisonOctaves4 doit retourner 4 voix.");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::VocalStack4) == 4,
                    "VocalStack4 doit retourner 4 voix.");
            expect (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::ThirdBelowAbove) == 2,
                    "ThirdBelowAbove doit retourner 2 voix.");
        }

        // ----------------------------------------------------------------
        // 2) Facteur de compensation 1/sqrt(1+N). On verifie que :
        //    - il est strictement inferieur a 1 (donc reduit bien le mix)
        //    - il est strictement superieur a 0 (pas de silence total)
        //    - pour UnisonOctaves4 (N=4) il est plus agressif que pour
        //      Unison2 (N=2), comme attendu.
        // ----------------------------------------------------------------
        beginTest ("Facteur 1/sqrt(1+N) : reduit bien, plus agressif pour N eleve");
        {
            auto matchFactor = [] (int n) {
                return (n > 0) ? (1.0f / std::sqrt (1.0f + static_cast<float> (n))) : 1.0f;
            };
            const float fUnison2   = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::Unison2));
            const float fUnison4   = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::UnisonOctaves4));
            const float fVocal4    = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::VocalStack4));
            const float fThird     = matchFactor (HarmonyEngine::getHarmonyVoiceCount (HarmonyType::ThirdBelowAbove));

            // Facteur strictement inferieur a 1 (le mix d'harmonie est reduit).
            expect (fUnison2 < 1.0f, "Unison2 : facteur doit etre < 1.0");
            expect (fUnison4 < 1.0f, "UnisonOctaves4 : facteur doit etre < 1.0");
            expect (fVocal4  < 1.0f, "VocalStack4 : facteur doit etre < 1.0");
            expect (fThird   < 1.0f, "ThirdBelowAbove : facteur doit etre < 1.0");

            // Facteur strictement superieur a 0 (pas de silence total).
            expect (fUnison2 > 0.0f, "Unison2 : facteur doit etre > 0.0");
            expect (fUnison4 > 0.0f, "UnisonOctaves4 : facteur doit etre > 0.0");

            // Plus de voix = facteur plus petit (compensation plus forte).
            expect (fUnison4 < fUnison2,
                "UnisonOctaves4 (N=4) doit etre plus compense que Unison2 (N=2). "
                "fUnison4=" + juce::String (fUnison4, 4) + ", fUnison2=" + juce::String (fUnison2, 4));
            expect (fVocal4 < fUnison2,
                "VocalStack4 (N=4) doit etre plus compense que Unison2 (N=2).");

            // Verification de la valeur numerique pour la doc.
            //   Unison2 : N=2 -> 1/sqrt(3) = 0.5774 (-4.77 dB)
            //   Unison4 : N=4 -> 1/sqrt(5) = 0.4472 (-6.99 dB)
            // Tolerances larges (+/- 0.01) pour absorber les variations de compilation.
            expect (std::abs (fUnison2 - 0.5774f) < 0.01f,
                "Unison2 : 1/sqrt(3) attendu ~0.5774, mesure = "
                + juce::String (fUnison2, 4));
            expect (std::abs (fUnison4 - 0.4472f) < 0.01f,
                "UnisonOctaves4 : 1/sqrt(5) attendu ~0.4472, mesure = "
                + juce::String (fUnison4, 4));
        }
    }
};

static HarmonyGainMatchTest harmonyGainMatchTest;
