// ScaleQuantizer.h
// Quantificateur de pitch vers une gamme musicale (tonique + mode).
// Implementation effective en Phase 1.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// On utilise "atdsp" (autotune dsp) plutot que "dsp" pour eviter toute
// ambiguite avec le namespace "juce::dsp" apporte par JuceHeader.h.
namespace atdsp
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
        /// @return frequence cible en Hz, ou hzIn si pas de note dans la gamme.
        float quantize (float hzIn) const;

        /// Renvoie la liste actuelle des notes de la gamme (en demi-tons 0..11
        /// relatifs a C). Combine key+scale (ou gamme custom).
        const juce::Array<int>& getScaleIntervals() const { return intervals; }

        /// Indique si la gamme est en mode personnalise.
        bool isCustom() const { return currentScale == Scale::Custom; }

    private:
        int key = 0;       // 0-11
        Scale currentScale = Scale::Chromatic;
        juce::Array<int> intervals;        // demi-tons relatifs a C appartenant a la gamme
        juce::Array<int> customIntervals;  // gamme custom (sans decalage de key)

        // Reconstruit le tableau d'intervalles selon (key, scale).
        void rebuildIntervals();
    };
}
