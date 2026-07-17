// PitchShifterOutputTest.cpp
// Non-regression : le PitchShifter doit produire un signal NON nul
// en sortie quand on lui fournit un sinus vocal soutenu (f0 constant).
//
// Ce test aurait attrape immediatement le bug de la correction B
// (gain de grain divise par la somme COLA BRUTE ~3850 au lieu de la
// valeur par-point ~1.88 -> gain ~2000x trop faible -> silence total).

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/PitchShifter.h"

class PitchShifterOutputTest : public juce::UnitTest
{
public:
    PitchShifterOutputTest() : juce::UnitTest ("PitchShifterOutput") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("Sinus 200 Hz soutenu : la sortie n'est pas nulle (regression B)");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float f0 = 200.0f;           // note chantee stable
            const float ratio = 1.0f;          // pas de correction
            const float formantRatio = 1.0f;

            // Traite ~0.5 s de sinus ; verifie la RMS de sortie sur la
            // seconde moitie (apres la fade-in de demarrage ~20 ms).
            const int blocks = 24;              // 24 * 512 ~ 0.28 s
            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            double sumSq = 0.0;
            int nRms = 0;
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const double t = static_cast<double> (b) * 512.0 + i;
                    in.setSample (0, i,
                        std::sin (2.0 * juce::MathConstants<double>::pi
                                 * f0 * t / sr));
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, f0);

                // RMS sur la 2e moitie du bloc (apres fade-in locale).
                for (int i = 256; i < 512; ++i)
                {
                    const float s = out.getSample (0, i);
                    expect (std::isfinite (s), "NaN dans la sortie du PitchShifter");
                    sumSq += static_cast<double> (s) * s;
                    ++nRms;
                }
            }

            const double rms = std::sqrt (sumSq / static_cast<double> (nRms));
            // Un gain de grain correct donne une RMS de l'ordre de 0.5-0.7
            // pour un sinus unitaire. On exige simplement qu'elle soit
            // clairement non nulle (bug B donnait ~1e-4 ou moins).
            expect (rms > 0.05,
                    "Sortie du PitchShifter quasi-nulle (silence) : "
                    + juce::String (rms, 6)
                    + " -> bug de gain de grain suspecte");
        }
    }
};

static PitchShifterOutputTest pitchShifterOutputTest;
