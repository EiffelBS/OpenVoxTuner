// RetargetEnvelopeTest.cpp
// Tests unitaires du filtre de retargeting (Speed).

#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/RetargetEnvelope.h"

class RetargetEnvelopeTest : public juce::UnitTest
{
public:
    RetargetEnvelopeTest() : juce::UnitTest ("RetargetEnvelope") {}

    void runTest() override
    {
        using namespace atdsp;

        beginTest ("Speed=0 -> reponse instantanee");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (0.0f);
            // Premier appel : la valeur doit deja etre egale a la cible.
            const float v = env.processSample (1.5f);
            expectWithinAbsoluteError (v, 1.5f, 1e-3f);
        }

        beginTest ("Speed=200 ms -> convergence progressive");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (200.0f);
            env.reset();
            // Pas a pas sur 1 seconde (44100 echantillons) avec cible 2.0
            // depuis 1.0 : on devrait etre proche de 2.0 a la fin.
            float v = 1.0f;
            for (int i = 0; i < 44100; ++i)
                v = env.processSample (2.0f);
            // Apres 1 s = 5 * tau, on est a ~99% de la cible.
            expect (v > 1.95f, "Convergence insuffisante : v=" + juce::String (v));
        }

        beginTest ("Cible = 1.0 -> reste a 1.0");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (50.0f);
            // Meme cible que la valeur initiale -> pas de drift.
            float v = 1.0f;
            for (int i = 0; i < 1000; ++i)
                v = env.processSample (1.0f);
            expectWithinAbsoluteError (v, 1.0f, 1e-5f);
        }

        beginTest ("Reset remet a 1.0");
        {
            RetargetEnvelope env;
            env.prepare (44100.0);
            env.setSpeed (50.0f);
            for (int i = 0; i < 100; ++i) (void) env.processSample (1.5f);
            env.reset();
            // Apres reset, prochaine cible doit etre appliquee depuis 1.0.
            const float v = env.processSample (1.5f);
            expect (v < 1.1f, "Reset non applique, v=" + juce::String (v));
        }
    }
};

static RetargetEnvelopeTest retargetEnvelopeTest;
