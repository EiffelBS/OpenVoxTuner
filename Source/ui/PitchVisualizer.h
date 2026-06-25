// PitchVisualizer.h
// Composant GUI : visualisation en temps reel des courbes de pitch.
// Affiche la pitch curve d'entree, la pitch curve corrigee, la note
// actuellement chantee, l'offset en cents, et les notes de la gamme.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/NoteUtils.h"
#include "PianoKeyboard.h"

namespace ui
{
    /**
     * Affiche la pitch curve d'entree et la pitch curve corrigee,
     * avec en overlay :
     *   - la note actuellement chantee (ex: "F3")
     *   - l'offset en cents par rapport a la note quantifiee
     *   - un meter de tuning vertical (style Antares / Studio One)
     *   - les lignes horizontales des notes de la gamme selectionnee
     */
    class PitchVisualizer : public juce::Component,
                            public juce::Timer
    {
    public:
        PitchVisualizer();
        ~PitchVisualizer() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        /// Ajoute un echantillon de pitch (Hz) d'entree.
        void pushInputPitch (float hz);
        /// Ajoute un echantillon de pitch (Hz) corrige.
        void pushOutputPitch (float hz);

        /// Met a jour les informations de note / offset pour l'affichage.
        void setNoteInfo (const atdsp::NoteInfo& info);

        /// Definit les notes de la gamme actuelle (demi-tons 0..11) pour
        /// tracer les lignes de la gamme en arriere-plan.
        void setScaleIntervals (const juce::Array<int>& intervals);
        /// Provide the frequencies (Hz) of currently active harmony voices
        /// to be pushed into the harmony history (time series) so they are
        /// displayed as thin blue lines following the same timeline as input/output.
        void setHarmonyFrequencies (const juce::Array<float>& freqs);

        /// Accesseur au clavier de piano integre (correctif R2.2).
        PianoKeyboard& getPianoKeyboard() { return pianoKeyboard; }

    private:
        // Historique des pitches (Hz), taille max ~5 secondes a 30 fps.
        static constexpr int historySize = 150;
        juce::Array<float> inputHistory;
        juce::Array<float> outputHistory;

        // Note info + gamme courante (mis a jour par le processor / editor).
        atdsp::NoteInfo noteInfo;
        juce::Array<int> scaleIntervals;

        // Piano vertical
        PianoKeyboard pianoKeyboard;
        // Harmony history: one history buffer per voice (fixed max voices)
        static constexpr int maxHarmonyVoices = 8;
        juce::Array<juce::Array<float>> harmonyHistory;
        
        // Stockage de la derniere valeur
        float latestInputHz = 0.0f;
        float latestOutputHz = 0.0f;

        // Couleurs du theme.
        static const juce::Colour kBg;
        static const juce::Colour kGrid;
        static const juce::Colour kInputColour;
        static const juce::Colour kOutputColour;
        static const juce::Colour kScaleLineColour;
        static const juce::Colour kHarmonyColour;  // bleu pour les harmonies generees

        // Limites d'affichage (Hz, echelle log).
        float fMin = 50.0f;
        float fMax = 1500.0f;

        // Conversion Hz -> Y pixel.
        float hzToY (float hz, int height) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchVisualizer)
    };
}
