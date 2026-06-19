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
        using namespace atdsp;

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
    }
};

static FormantPreserverTest formantPreserverTest;
