// PitchDetector.h
// Module de detection de pitch (frequence fondamentale) par algorithme YIN.
// Implementation reelle (Phase 1).
//
// References :
//   de Cheveigne & Kawahara, "YIN, a fundamental frequency estimator for speech
//   and music", J. Acoust. Soc. Am. 111 (4), avril 2002.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    /**
     * Detecteur de pitch utilisant l'algorithme YIN.
     *
     * Limites :
     *   - Detection entre freqMinHz et freqMaxHz (defaut 50-1000 Hz).
     *   - Buffers d'entree >= 2 * maxLag echantillons.
     *   - Mono (un seul canal).
     */
    class PitchDetector
    {
    public:
        PitchDetector();
        ~PitchDetector();

        /// Prepare le detecteur avec le sample rate et la taille de bloc.
        /// Doit etre appele avant detectPitch.
        void prepare (double sampleRate, int blockSize);

        /// Reinitialise l'etat interne (a appeler lors d'un reset de transport).
        void reset();

        /// Calcule le pitch d'un buffer audio mono.
        /// @param samples   echantillons audio (float)
        /// @param numSamples nombre d'echantillons
        /// @return frequence fondamentale en Hz, ou 0.0 si pas de pitch detecte
        float detectPitch (const float* samples, int numSamples);

        /// Seuil de clarte YIN (0.0-1.0). Defaut 0.10.
        void setThreshold (float t) { threshold = juce::jlimit (0.01f, 0.99f, t); }
        float getThreshold() const  { return threshold; }

    private:
        double sampleRate = 44100.0;

        // Plage de frequences detectables.
        float freqMinHz = 50.0f;
        float freqMaxHz = 1000.0f;

        // Seuil de clarte (probabilite que le minimum soit correct).
        float threshold = 0.05f;

        // Buffers de travail.
        juce::HeapBlock<float> yinBuffer; // difference function cumulee
        int bufferSize = 0;              // taille du buffer
        int maxLag = 0;                  // plus grande periode (lag) a tester

        // Algorithme YIN.
        float computeYin (const float* samples, int numSamples);

        // Anti-octave error (Median filter + octave continuity)
        static constexpr int MEDIAN_SIZE = 5;
        float history[MEDIAN_SIZE] = {0.0f};
        int historyIdx = 0;
        // Dernier pitch valide pour la verification de continuite d'octave.
        float lastValidPitch = 0.0f;
        float getMedianFiltered(float newValue);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchDetector)
    };
}
