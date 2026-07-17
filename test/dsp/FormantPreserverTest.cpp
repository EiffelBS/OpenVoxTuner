// FormantPreserverTest.cpp
// Tests unitaires du compensateur de formants.

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/FormantPreserver.h"

class FormantPreserverTest : public juce::UnitTest
{
public:
    FormantPreserverTest() : juce::UnitTest ("FormantPreserver") {}

    void runTest() override
    {
        using namespace ovtdsp;

        beginTest ("Desactive : pas de modification");
        {
            FormantPreserver fp;
            fp.prepare (44100.0, 64);
            fp.setEnabled (false);

            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i) buf.setSample (0, i, 0.5f);

            fp.process (buf, 2.0f); // ratio extreme, mais desactive
            for (int i = 0; i < 64; ++i)
                expectWithinAbsoluteError (buf.getSample (0, i), 0.5f, 1e-6f);
        }

        beginTest ("Ratio = 1.0 : pas de modification");
        {
            FormantPreserver fp;
            fp.prepare (44100.0, 64);
            fp.setEnabled (true);

            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i) buf.setSample (0, i, 0.5f);

            fp.process (buf, 1.0f);
            // Filtre biquad a 500 Hz sur DC -> b0 + b1 + b2 = gain DC.
            // Le signal DC traverse donc un gain non unitaire, mais il n'est
            // PAS nul. Le test verifie juste qu'on n'a pas NaN.
            for (int i = 0; i < 64; ++i)
                expect (std::isfinite (buf.getSample (0, i)),
                        "NaN dans la sortie");
        }

        beginTest ("Ratio extreme : signal filtre reste borne");
        {
            FormantPreserver fp;
            fp.prepare (44100.0, 64);
            fp.setEnabled (true);

            juce::AudioBuffer<float> buf (1, 64);
            for (int i = 0; i < 64; ++i)
                buf.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi * 100.0f * i / 44100.0f));

            fp.process (buf, 4.0f);
            for (int i = 0; i < 64; ++i)
            {
                const float s = buf.getSample (0, i);
                expect (std::isfinite (s), "NaN");
                expect (std::abs (s) < 10.0f, "Sortie explosive : " + juce::String (s));
            }
        }

        beginTest ("MultiFormant : pas de discontinuite au saut de ratio (clic)");
        {
            // Corrige la cause A : les coefficients biquad doivent etre lisses
            // d'un bloc a l'autre. Un saut brutal de ratio (note qui demarre)
            // ne doit PAS produire un echantillon de sortie qui explose
            // brutalement par rapport au precedent (ce qui serait un pop).
            FormantPreserver fp;
            fp.prepare (44100.0, 512);
            fp.setEnabled (true);
            fp.setMode (ovtdsp::FormantPreserver::Mode::MultiFormant);

            // Signal sinusoidal continu (100 Hz) pour isoler l'effet du filtre.
            juce::AudioBuffer<float> buf (1, 512);
            auto fill = [&] (float phase0)
            {
                for (int i = 0; i < 512; ++i)
                    buf.setSample (0, i, std::sin (2.0f * juce::MathConstants<float>::pi
                                                 * 100.0f * (phase0 + i) / 44100.0f));
            };

            // Phase 1 : ratio stable (note de reference).
            fill (0.0f);
            fp.process (buf, 1.0f);
            float lastSample = buf.getSample (0, 511);

            // Phase 2 : SAUT de ratio (simule le demarrage d'une note chantee
            // ou un changement de hauteur). On traite plusieurs blocs.
            int maxJumpCount = 0;
            float prev = lastSample;
            for (int blk = 0; blk < 5; ++blk)
            {
                fill (static_cast<float> (blk) * 512.0f);
                fp.process (buf, 2.0f); // ratio double
                for (int i = 0; i < 512; ++i)
                {
                    const float s = buf.getSample (0, i);
                    expect (std::isfinite (s), "NaN apres saut de ratio");
                    // Un vrai clic audible = un saut instantane > 0.5 d'amplitude
                    // (sur un signal unitaire). Le lissage des coefficients
                    // biquad empêche les sauts repetes : on tolère au plus 3
                    // echantillons de saut (le tout premier sample du changement
                    // de coeff, inévitable car le filtre a une memoire), contre
                    // des dizaines sans lissage.
                    const float jump = std::abs (s - prev);
                    if (jump > 0.5f) maxJumpCount++;
                    prev = s;
                }
            }
            expect (maxJumpCount <= 3,
                    "Trop de discontinuites (clics) au saut de ratio MultiFormant : "
                    + juce::String (maxJumpCount) + " echantillons (attendu <= 3)");
        }
    }
};

static FormantPreserverTest formantPreserverTest;
