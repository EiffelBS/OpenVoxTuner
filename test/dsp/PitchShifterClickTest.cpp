#pragma once
// PitchShifterClickTest.cpp
// Unit test
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Source/dsp/PitchShifter.h"

class PitchShifterClickTest : public juce::UnitTest
{
public:
    PitchShifterClickTest() : juce::UnitTest ("PitchShifterClick") {}

    void runTest() override
    {
        using namespace ovtdsp;

        // ===================================================================
        // Test 1 : attaque forte + saut de pitch
        // ===================================================================
        beginTest ("Scenario realiste : note 200Hz -> saut 300Hz, aucun clic audible");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float ratio = 1.0f;          // pas de correction (isole le shifter)
            const float formantRatio = 1.0f;

            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            int clickCount = 0;
            float prev = 0.0f;
            double t = 0.0;                    // temps courant (echantillons)
            juce::String clickLog;

            auto feedBlock = [&] (float f0, double freqHz)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float s = 0.0f;
                    if (f0 > 0.0f)
                        s = std::sin (2.0 * juce::MathConstants<double>::pi
                                      * freqHz * t / sr);
                    in.setSample (0, i, s);
                    ++t;
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, f0);

                for (int i = 0; i < 512; ++i)
                {
                    const float s = out.getSample (0, i);
                    expect (std::isfinite (s), "NaN dans la sortie");
                    const float jump = std::abs (s - prev);
                    // Un clic audible = saut instantane > 0.1 d'amplitude
                    // (sur un signal sinusoidal unitaire).
                    if (jump > 0.15f)
                    {
                        ++clickCount;
                        clickLog += juce::String (static_cast<int> (t * 1000.0 / sr))
                                    + "ms(" + juce::String (jump, 3) + ") ";
                    }
                    prev = s;
                }
            };

            // 1) silence ~50 ms (f0 = 0)
            for (int b = 0; b < 5; ++b) feedBlock (0.0f, 0.0);
            // 2) attaque note 200 Hz ~200 ms
            for (int b = 0; b < 20; ++b) feedBlock (200.0f, 200.0);
            // 3) saut vers 300 Hz ~200 ms
            for (int b = 0; b < 20; ++b) feedBlock (300.0f, 300.0);

            // On ecrit quand meme le compte dans un fichier pour la tracabilite.
            {
                juce::File f ("c:/Users/User/Documents/trae_projects/OpenVoxTuner/test/dsp/click_count.txt");
                f.deleteFile();
                f.create();
                f.replaceWithText ("clickCount=" + juce::String (clickCount)
                                   + "\npositions=" + clickLog + "\n");
            }
            expectEquals (clickCount, 0,
                "Clic(s) audible(s) sur attaque/saut de note : " + clickLog);
        }

        // ===================================================================
        // Test 2 : STACCATO (meme note repetee avec gap court)
        // Verifie que les repetitions rapides d'une meme note ne produisent
        // pas de "pop" (sur-gain OLA) au debut de chaque attaque.
        // ===================================================================
        beginTest ("Staccato : 5 repetitions rapides meme note, pas de pop a chaque attaque");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float ratio = 1.0f;
            const float formantRatio = 1.0f;
            const float noteFreq = 200.0f;

            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            int clickCount = 0;
            float prev = 0.0f;
            double t = 0.0;
            juce::String clickLog;

            auto feedBlock = [&] (float f0, double freqHz)
            {
                for (int i = 0; i < 512; ++i)
                {
                    float s = 0.0f;
                    if (f0 > 0.0f)
                        s = std::sin (2.0 * juce::MathConstants<double>::pi
                                      * freqHz * t / sr);
                    in.setSample (0, i, s);
                    ++t;
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, f0);

                for (int i = 0; i < 512; ++i)
                {
                    const float s = out.getSample (0, i);
                    expect (std::isfinite (s), "NaN dans la sortie");
                    const float jump = std::abs (s - prev);
                    if (jump > 0.15f)
                    {
                        ++clickCount;
                        clickLog += juce::String (static_cast<int> (t * 1000.0 / sr))
                                    + "ms(" + juce::String (jump, 3) + ") ";
                    }
                    prev = s;
                }
            };

            // 1) silence initial ~50 ms
            for (int b = 0; b < 5; ++b) feedBlock (0.0f, 0.0);

            // 2) 5 repetitions staccato : note (~100 ms) + gap (~50 ms)
            for (int rep = 0; rep < 5; ++rep)
            {
                // Note 200 Hz pendant ~100 ms (10 blocs)
                for (int b = 0; b < 10; ++b) feedBlock (noteFreq, noteFreq);
                // Silence pendant ~50 ms (5 blocs)
                for (int b = 0; b < 5; ++b) feedBlock (0.0f, 0.0);
            }

            expectEquals (clickCount, 0,
                "Pop(s) audible(s) sur attaques staccato : " + clickLog);
        }

        // ===================================================================
        // Test 3 : pic d'amplitude a l'attaque (OLA burst "trompette")
        // Verifie que l'amplitude du signal apres l'enveloppe d'attaque ne
        // depasse pas 1.0x l'amplitude d'entree. Sans le clamp outPhase,
        // 5 grains en burst causent une somme OLA 2x -> clipping audible
        // meme si aucun saut > 0.1 n'est detecte (la rampe d'attaque lisse
        // la transition, mais le pic est sure Amplifie).
        // ===================================================================
        beginTest ("Attaque brute : pas de sur-amplification OLA (somme <= entree)");
        {
            PitchShifter ps;
            ps.prepare (44100.0, 512);
            ps.setAttackTimeMs (30.0f);

            const double sr = 44100.0;
            const float ratio = 1.0f;
            const float formantRatio = 1.0f;
            const float noteFreq = 200.0f;

            juce::AudioBuffer<float> in (1, 512);
            juce::AudioBuffer<float> out (1, 512);

            double t = 0.0;
            // Silence initial ~50 ms
            for (int b = 0; b < 5; ++b)
            {
                for (int i = 0; i < 512; ++i) { in.setSample (0, i, 0.0f); ++t; }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, 0.0f);
            }
            // Note 200 Hz pendant ~200 ms (40 blocs, ~464 ms)
            // On capture le max amplitude sur les 100 premiers ms
            // (l'attaque + le debut de la note stable).
            float maxAbs = 0.0f;
            double tAtMax = 0.0;
            const double inputAmp = 1.0; // sin amplitude
            for (int b = 0; b < 40; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = std::sin (2.0 * juce::MathConstants<double>::pi
                                              * noteFreq * t / sr);
                    in.setSample (0, i, s);
                    ++t;
                }
                out.setSize (1, 512, false, true, false);
                ps.process (in, out, ratio, formantRatio, noteFreq);
                for (int i = 0; i < 512; ++i)
                {
                    const float v = std::abs (out.getSample (0, i));
                    if (v > maxAbs) { maxAbs = v; tAtMax = (t - 512 + i) * 1000.0 / sr; }
                }
            }
            // Tolere 10% de sur-amplification (1.1) pour absorber les petites
            // fluctuations COLA. Un burst de 5 grains donnerait 2.0x, donc
            // largement au-dessus du seuil.
            expect (maxAbs <= inputAmp * 1.10f,
                "Sur-amplification OLA : max |out|=" + juce::String (maxAbs, 3)
                + " (a t=" + juce::String (tAtMax, 2) + "ms), attendu <= "
                + juce::String (inputAmp * 1.10f, 3)
                + " (10% au-dessus de l'amplitude d'entree " + juce::String (inputAmp, 2) + ")");
        }
    }
};

static PitchShifterClickTest pitchShifterClickTest;


