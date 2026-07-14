#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    /**
     * Interface abstraite pour les moteurs de Pitch Shifting.
     * Permet d'interchanger RubberBand, SoundTouch ou une implémentation PSOLA
     * de manière transparente pour le reste du pipeline.
     */
    class IPitchShifter
    {
    public:
        virtual ~IPitchShifter() = default;

        /**
         * Initialise le shifter avec la frequence d'echantillonnage et la taille de bloc.
         */
        virtual void prepare (double sampleRate, int maximumBlockSize) = 0;

        /**
         * Remet a zero l'etat interne (ex: flush des FIFOs).
         */
        virtual void reset() = 0;

        /**
         * Traite un bloc audio en place.
         * @param buffer Le buffer audio stereo a modifier.
         * @param ratio Le ratio de transposition (ex: 1.0 = aucune, 2.0 = +1 octave).
         * @param f0 La frequence fondamentale detectee (necessaire pour certains algos comme PSOLA).
         */
        virtual void process (juce::AudioBuffer<float>& buffer, float ratio, float f0) = 0;

        /**
         * Retourne la latence interne du moteur en echantillons.
         */
        virtual int getLatencySamples() const = 0;
    };
}
