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
//
// Modes:
//   - Legacy: single peaking EQ at 500Hz (original behavior)
//   - MultiFormant: 4 formant peaks (F1-F4) with configurable Q/gain

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovtdsp
{
    /**
     * Decale les formants d'un signal dans le sens oppose a une transposition.
     * Doit etre appele AVANT le PSOLA.
     */
    class FormantPreserver
    {
    public:
        enum class Mode
        {
            Legacy,       // Single peaking EQ at 500Hz (original)
            MultiFormant  // F1-F4 formant preservation
        };

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

        /// Selectionne le mode de preservation des formants
        void setMode (Mode m) { mode = m; }
        Mode getMode() const { return mode; }

        /// Configure un formant specifique (mode MultiFormant uniquement)
        /// @param index 0=F1, 1=F2, 2=F3, 3=F4
        /// @param freqHz frequence du formant en Hz
        /// @param q qualite (resonance)
        /// @param gainDb gain en dB
        void setFormant (int index, float freqHz, float q, float gainDb);

    private:
        double sampleRate = 44100.0;
        int blockSize = 512;
        
        float shiftSemitones = 0.0f;

        // Coefficients du filtre IIR (biquad par formant par canal).
        struct ChannelState
        {
            // Un biquad par formant (max 4).
            // `formants` stocke les coefficients CIBLES (recalculees a chaque
            // bloc selon le ratio). `smooth` stocke les coefficients REELS
            // appliques au signal : ils sont lisses vers les cibles block par
            // block afin d'eviter toute discontinuite (pop) quand le ratio
            // change brutalement au demarrage d'une note.
            struct BiquadState
            {
                float a1 = 0.0f, a2 = 0.0f;
                float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
                float z1 = 0.0f, z2 = 0.0f;
            };
            struct BiquadSmooth
            {
                float a1 = 0.0f, a2 = 0.0f;
                float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
            };
            BiquadState  formants[4];   // cibles (recalcul par bloc)
            BiquadSmooth smooth[4];     // coefficients appliques (lisses)
        };
        juce::Array<ChannelState> channels;

        // Coefficient de lissage des biquads (un pas par bloc, applique a
        // chaque echantillon du bloc pour rester buffer-size independent).
        //
        // 2026-07-23 (Fix AZ): increased from 0.002 (~2.9s TC at 256/44100,
        // which produced a 5Hz warble of formant frequencies when the input
        // pitch had any vibrato modulation) to 0.05 (~115ms TC). At 115ms
        // the smoother is fast enough to track the typical 5Hz vibrato
        // (|H(5Hz)| ~ 0.42, so the biquad response moves with ~half the
        // vibrato amplitude, which is the right perceptual balance:
        // "formants follow pitch" without being completely static), but
        // still smooths out per-block YIN jitter and OLA retarget steps.
        // Without this fix, the FormantPreserver acted as a 5Hz bandpass
        // on its own compensation ratio (1/sqrt(targetRatio)) and produced
        // an audible "warble" that the user reports as a "scratch" with
        // Flex>0 + Speed=0. The previous 2.9s TC was an unintended
        // side-effect of an old buffer-size-dependent formula.
        float biquadSmoothAlpha = 0.05f;

        // Configuration des formants par defaut (F1-F4 typiques voix masculine)
        struct FormantConfig
        {
            float freqHz = 500.0f;
            float q = 2.0f;
            float gainDb = 8.0f;
        };
        FormantConfig formantConfigs[4];

        Mode mode = Mode::Legacy;

        // Frequence de Nyquist securite.
        static constexpr float maxCutoffHz = 8000.0f;

        // Active / desactive le module.
        bool enabled = false;

        // Recalcule les coefficients biquad pour la freq de coupure demandee.
        void updateBiquadCoefficients (ChannelState::BiquadState& s, float freqHz, float q, float gainDb);

        // Met a jour tous les formants pour un canal selon le ratio
        void updateAllFormants (ChannelState& s, float compensationRatio, float shiftRatio);

        // Traite un canal avec tous les biquads en serie
        void processChannel (float* data, int numSamples, ChannelState& s);
    };
}
