// RetargetEnvelope.h
// Enveloppe de retargeting du pitch (style Antares Auto-Tune "Speed").
//
// Principe :
//   Speed definit un temps de reponse en millisecondes : c'est la duree
//   que met le ratio "applique" pour converger de 0 a ~63% de sa valeur
//   finale (constante de temps tau).
//
// Implementation : un filtre IIR passe-bas du 1er ordre sur le ratio :
//     y[n] = y[n-1] + (x[n] - y[n-1]) * alpha
// avec :
//     alpha = 1 - exp(-dt / tau)
//     dt = 1 / sampleRate
//     tau = speedMs / 1000
//
// Plus Speed est petit (0 ms), plus la correction est instantanee
// (effet "robotique" type T-Pain). Plus Speed est grand (200 ms),
// plus la correction est douce et naturelle.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace atdsp
{
    class RetargetEnvelope
    {
    public:
        RetargetEnvelope();
        ~RetargetEnvelope();

        void prepare (double sampleRate);
        void reset();

        /// Definit le temps de retargeting en millisecondes.
        void setSpeed (float ms);

        /// Traite un echantillon de ratio et retourne le ratio lisse.
        /// @param targetRatio  ratio cible (calcule par la quantification)
        /// @return             ratio applique apres lissage
        float processSample (float targetRatio);

        /// Variante "block" : applique le lissage en tenant compte du nombre
        /// d'echantillons dans le bloc, pour que la constante de temps soit
        /// INDEPENDANTE de la taille de buffer.
        /// Sans cela, appeler processSample() une fois par bloc avec un alpha
        /// per-sample donne un temps de reponse effectif de tau*numSamples
        /// (7.2s a 144 samples @ 44.1kHz pour speed=50ms -> le Speed n'a
        /// praticamente aucun effet a petit buffer).
        /// @param targetRatio  ratio cible
        /// @param numSamples   taille du bloc audio
        /// @return             ratio lisse applique pour ce bloc
        float processBlock (float targetRatio, int numSamples);

    private:
        double sampleRate = 44100.0;
        float speedMs = 50.0f;
        float currentValue = 1.0f;
        float alpha = 1.0f; // coefficient du filtre IIR (0 = pas de change, 1 = instantane)

        // Recalcule alpha en fonction de speedMs et sampleRate.
        void recomputeAlpha();
    };
}
