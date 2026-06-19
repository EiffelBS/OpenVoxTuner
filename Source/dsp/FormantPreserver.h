// FormantPreserver.h
// Module de compensation de formants (Phase 4).
//
// Principe : quand on transpose un signal par PSOLA, les formants (resonances
// du conduit vocal) sont decales avec le pitch, ce qui donne un effet
// "chipmunk" (tete qui devient plus fine quand on monte).
//
// La technique classique (Moulines & Charpentier 1990) consiste a :
//   1) Filtrer le signal par un passe-bas dont la frequence de coupure est
//      proportionnelle au pitch (en log). Cela deplace les formants
//      artificiellement dans le sens inverse.
//   2) Appliquer ensuite le PSOLA.
//   3) Resampler pour ajuster la duree si necessaire.
//
// Implementation simplifiee : un filtre IIR passe-bas du 2nd ordre dont
// la frequence de coupure suit le ratio de transposition. Le filtre est
// tres leger (1-2 coeffs mis a jour par bloc).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace atdsp
{
    /**
     * Decale les formants d'un signal dans le sens oppose a une transposition.
     * Doit etre appele AVANT le PSOLA.
     */
    class FormantPreserver
    {
    public:
        FormantPreserver();
        ~FormantPreserver();

        void prepare (double sampleRate, int blockSize);
        void reset();

        /// Applique le deplacement de formants au buffer.
        /// @param ratio ratio de transposition (1.0 = passthrough)
        void process (juce::AudioBuffer<float>& buffer, float ratio);

        void setEnabled (bool b) { enabled = b; }
        bool isEnabled() const   { return enabled; }

        /// Definit le decalage manuel de formants en demi-tons
        void setFormantShift (float semitones) { shiftSemitones = semitones; }

    private:
        double sampleRate = 44100.0;
        int blockSize = 512;
        
        float shiftSemitones = 0.0f;

        // Coefficients du filtre IIR passe-bas (Butterworth 2nd ordre).
        // Stockes par canal.
        struct ChannelState
        {
            // Coefficients du biquad (forme directe transposed II).
            float a1 = 0.0f, a2 = 0.0f;
            float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            // Etats (delais).
            float z1 = 0.0f, z2 = 0.0f;
        };
        juce::Array<ChannelState> channels;

        // Frequence de coupure de reference (formant moyen, 500 Hz).
        // Le filtre deplace les formants en multipliant cette freq par (1/ratio).
        // -> quand ratio > 1 (pitch up), on baisse les formants avant PSOLA,
        //    pour que PSOLA les remonte ensuite a leur position d'origine.
        static constexpr float referenceFormantHz = 500.0f;

        // Frequence de Nyquist securite.
        static constexpr float maxCutoffHz = 8000.0f;

        // Active / desactive le module.
        bool enabled = false;

        // Recalcule les coefficients biquad pour la freq de coupure demandee.
        void updateCoefficients (ChannelState& s, float cutoffHz);

        // Traite un canal avec le biquad.
        void processChannel (float* data, int numSamples, ChannelState& s);
    };
}
