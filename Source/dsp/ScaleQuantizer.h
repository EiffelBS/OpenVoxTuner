// ScaleQuantizer.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// On utilise "ovtdsp" (autotune dsp) plutot que "dsp" pour eviter toute
// ambiguite avec le namespace "juce::dsp" apporte par JuceHeader.h.
namespace ovtdsp
{
    /**
     * Modes musicaux supportes par le plugin.
     * Les indices correspondent aux valeurs du parametre "scale".
     */
    enum class Scale
    {
        Chromatic       = 0,
        Major           = 1,
        MelodicMinor    = 2,
        HarmonicMinor   = 3,
        NaturalMinor    = 4,
        MajorPentatonic = 5,
        MinorPentatonic = 6,
        Blues           = 7,
        Dorian          = 8,
        Phrygian        = 9,
        Lydian          = 10,
        Mixolydian      = 11,
        Locrian         = 12,
        Custom          = 13
    };

    /**
     * Convertit une frequence (Hz) en demi-tons par rapport a A4 (440 Hz).
     */
    inline float hzToSemitones (float hz)
    {
        if (hz <= 0.0f) return -100.0f;
        return 12.0f * std::log2 (hz / 440.0f);
    }

    /**
     * Convertit des demi-tons par rapport a A4 en frequence (Hz).
     */
    inline float semitonesToHz (float semi)
    {
        return 440.0f * std::pow (2.0f, semi / 12.0f);
    }

    /**
     * Quantificateur : prend un pitch detecte et retourne le pitch cible
     * le plus proche dans la gamme selectionnee.
     */
    class ScaleQuantizer
    {
    public:
        ScaleQuantizer();

        /// Definit la tonique (0=C, 1=C#, ..., 11=B).
        void setKey (int keyInSemitones);

        /// Definit le mode / gamme.
        void setScale (Scale scale);

        /// Definit la gamme personnalisee (12 booleens, 0=C, 1=C#, ..., 11=B).
        /// Sans effet si le mode courant n'est pas Custom.
        void setCustomIntervals (const juce::Array<int>& notesInSemitones);

        /// Quantifie une frequence vers la note la plus proche de la gamme.
        /// @param hzIn          frequence detectee en Hz.
        /// @param positionPPQ   position courante en temps PPQ (croches). Utilise
        ///                       uniquement pour evaluer une eventuelle override
        ///                       de contexte d'accord (voir setChordOverride).
        /// @return frequence cible en Hz, ou hzIn si pas de note dans la gamme.
        float quantize (float hzIn, double positionPPQ = 0.0) const;

        /// Renvoie la liste actuelle des notes de la gamme (en demi-tons 0..11
        /// relatifs a C). Combine key+scale (ou gamme custom).
        const juce::Array<int>& getScaleIntervals() const { return intervals; }

        /// Indique si la gamme est en mode personnalise.
        bool isCustom() const { return currentScale == Scale::Custom; }

        // === Override de contexte d'accord (ARA) ===
        // Temporairement, les notes appartenant a un accord hors gamme sont
        // acceptees (glissent vers leur pitch exact) meme si elles ne sont pas
        // dans la gamme courante. Chaque fenetre est valable sur une zone de
        // temps PPQ donnee par updateAraMetadata().

        /// Definit (ajoute) une fenetre d'override pour un accord.
        /// @p chordPitchClasses  classes de hauteur (0..11) du accord (root, tierce,
        ///                        quinte, ... et le basse si differente).
        /// @p positionPPQ        position de debut de la fenetre (croches).
        /// @p durationPPQ        duree de validite (croches). Une valeur > 0.
        void setChordOverride (const juce::Array<int>& chordPitchClasses,
                               double positionPPQ,
                               double durationPPQ);

        /// Reinitialise toutes les fenetres d'override.
        void clearChordOverrides();

        /// Vrai si une fenetre d'override couvre @p positionPPQ.
        bool isChordOverrideActive (double positionPPQ) const;

        /// Vrai si @p pitchClass (0..11) est autorée au moment @p pos :
        /// note de gamme, OU accord actif contenant cette classe.
        bool isNoteAllowed (int pitchClass, double pos) const;

        // === Override d'accord "live" (temps réel : MIDI / sidechain) ===
        // Représente l'accord courant détecté en temps réel (source MIDI ou
        // sidechain). Il a PRIORITÉ sur les fenêtres ARA quand il est actif.
        // Quand il est inactif, on retombe sur les fenêtres ARA (chord track).

        /// Définit l'accord live courant (classes de hauteur 0..11).
        void setLiveChordOverride (const juce::Array<int>& chordPitchClasses);

        /// Désactive l'accord live (retour aux fenêtres ARA).
        void clearLiveChordOverride();

        /// Vrai si un accord live est actuellement actif.
        bool isLiveChordActive() const;

        /// Vrai si l'override d'accord actif à @p pos contient au moins une
        /// classe de hauteur HORS de la gamme courante (l'override élargit
        /// donc les notes autorisées au-delà de la gamme). Faux si aucun
        /// accord actif, ou si tous les tons de l'accord sont dans la gamme.
        bool isActiveChordOutOfScale (double pos) const;

    private:
        int key = 0;       // 0-11
        Scale currentScale = Scale::Chromatic;
        juce::Array<int> intervals;        // demi-tons relatifs a C appartenant a la gamme
        juce::Array<int> customIntervals;  // gamme custom (sans decalage de key)

        // Reconstruit le tableau d'intervalles selon (key, scale).
        void rebuildIntervals();

        /// Fenetre d'override d'accord (thread UI ecrit / thread audio lit).
        struct ChordWindow
        {
            juce::Array<int> chordNotes;  // classes de hauteur 0..11 du chord
            double startPPQ  = 0.0;
            double duration  = 0.0;       // en croches ; 0 = inactif
        };

        /// Vrai si @p pitchClass est un ton d'accord couvrant @p pos (sans
        /// retester la gamme — appelée par isNoteAllowed/quantize).
        bool isChordToneAt (int pitchClass, double pos) const;

        mutable juce::CriticalSection chordOverrideLock;
        juce::Array<ChordWindow> chordWindows;

        /// Accord "live" courant (temps réel MIDI/sidechain), prioritaire sur
        /// les fenêtres ARA. Vide + liveChordActive=false = inactif.
        juce::Array<int> liveChordNotes;
        bool liveChordActive = false;
    };
}



