// PianoKeyboard.h
// Composant GUI : un clavier de piano vertical affiche sur la gauche
// du curve editor. Permet a l'utilisateur d'identifier les notes
// affichees sur la grille de la pitch curve.
//
// Le piano va de bas en haut (notes graves en bas, aigues en haut),
// avec les touches blanches + noires dessinees correctement.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/NoteUtils.h"

namespace ui
{
    /**
     * Clavier de piano vertical (style "scrollable piano roll").
     * - Affiche les touches blanches en arriere-plan, les noires par-dessus.
     * - Affiche en surbrillance les notes appartenant a la gamme.
     * - Optionnel : un curseur "playhead" qui suit le pitch courant.
     */
    class PianoKeyboard : public juce::Component
    {
    public:
        PianoKeyboard();
        ~PianoKeyboard() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        /// Definit la plage de notes affichees (inclusives, en MIDI).
        void setRange (int lowestMidi, int highestMidi);

        /// Renvoie la plage courante.
        int getLowestMidi()  const { return lowestMidi; }
        int getHighestMidi() const { return highestMidi; }

        /// Definit les notes de la gamme (demi-tons 0..11 relatifs a C).
        void setScaleIntervals (const juce::Array<int>& intervals);

        /// Definit les pitches courants (pour la surbrillance des touches).
        void setCurrentPitches (float inputHz, float outputHz);

        /// Affiche les noms des notes (originale et corrigee) sous forme de
        /// labels colores colles en haut du clavier. Note : l'appelant doit
        /// fournir les noms formats (ex: "A#3", "C4").
        void setNoteNames (const juce::String& inputName, const juce::String& outputName);

        /// Renvoie le Y (en pixels) d'une note MIDI donnee.
        float midiToY (int midi) const;

        /// Renvoie la note MIDI correspondant a un Y en pixels.
        int   yToMidi (float y) const;

        /// Renvoie la note (Hz) correspondant a un Y en pixels.
        float yToHz (float y) const;

    private:
        int lowestMidi  = 36;  // C2
        int highestMidi = 96;  // C7

        juce::Array<int> scaleIntervals; // demi-tons 0..11 dans la gamme
        float currentInputHz = 0.0f;
        float currentOutputHz = 0.0f;

        // Labels pour afficher le nom des notes en temps reel (correctif R2.2).
        std::unique_ptr<juce::Label> inputNoteLabel;
        std::unique_ptr<juce::Label> outputNoteLabel;

        // Couleurs.
        static const juce::Colour kWhiteKey;
        static const juce::Colour kBlackKey;
        static const juce::Colour kWhiteKeyScale; // touche blanche appartenant a la gamme
        static const juce::Colour kBlackKeyScale;
        static const juce::Colour kBorder;
        static const juce::Colour kText;

        // Renvoie true si la note MIDI est une touche noire.
        static bool isBlackKey (int midi) noexcept;

        // Renvoie true si la note MIDI appartient a la gamme courante.
        bool isInScale (int midi) const noexcept;

        // Couleur de la touche (avec ou sans gamme).
        juce::Colour getKeyColour (int midi, bool black) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoKeyboard)
    };
}
