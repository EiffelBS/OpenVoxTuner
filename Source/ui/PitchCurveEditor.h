// PitchCurveEditor.h
// Composant interactif d'edition de pitch curve (mode "graphic" du plugin).
//
// Fonctionnalites :
//   - Affiche une grille de notes horizontale (C3, C4, C5)
//   - Affiche les points de la courbe et la courbe interpolee
//   - Drag des points avec la souris (mise a jour du pitch)
//   - Double-clic pour ajouter un point
//   - Clic droit (ou Alt+clic) pour supprimer un point
//   - Snapping optionnel a la gamme
//   - Bouton "Live" : capture du pitch courant comme nouveau point
//   - Bouton preset : menu deroulant (default, spoken, lyric, rap, robot)

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "../dsp/PitchCurve.h"
#include "../dsp/ScaleQuantizer.h"
#include "PianoKeyboard.h"

namespace ui
{
    /**
     * Editeur interactif de PitchCurve.
     * Communique avec le processor via un listener (changement de courbe).
     */
    class PitchCurveEditor : public juce::Component,
                             public juce::Timer
    {
    public:
        /** Notifie le processor d'un changement de courbe. */
        class Listener
        {
        public:
            virtual ~Listener() = default;
            virtual void pitchCurveChanged() = 0;
        };

        PitchCurveEditor();
        ~PitchCurveEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void timerCallback() override;

        // === Saisie souris ===
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

        // === API publique ===

        /// Acces a la courbe (lecture seule recommande depuis l'exterieur).
        const atdsp::PitchCurve& getCurve() const { return curve; }

        /// Remplace la courbe (apres chargement de preset par exemple).
        void setCurve (const atdsp::PitchCurve& newCurve);

        /// Capture la valeur du pitch courant (fourni par le processor) comme
        /// un point sur la courbe au temps 'currentTime'. Utilise en mode
        /// "Live recording".
        void capturePitch (float hz, double currentTime);

        /// Definit le zoom en temps (secondes affichees) et le zoom en pitch.
        void setViewRange (double secondsVisible, float minHz, float maxHz);

        /// Active/desactive le snap a la gamme.
        void setSnapEnabled (bool b);
        bool isSnapEnabled() const { return snapEnabled; }

        /// Active ou desactive le magnetisme sur la grille temporelle.
        void setSnapToGridEnabled (bool b);

        /// Active ou desactive le mode "escalier" (Step Mode) pour l'interpolation.
        void setStepModeEnabled (bool b);
        bool isStepModeEnabled() const { return curve.isStepMode(); }

        /// Vide la courbe (Reset)
        void clearCurve() { curve.clear(); repaint(); notifyChanged(); }

        /// Definit la gamme (pour le snap).
        void setKeyAndScale (int key, atdsp::Scale scale);

        /// Definit les notes de la gamme pour l'affichage du piano.
        /// Liste de demi-tons 0..11 relatifs a C.
        void setScaleIntervals (const juce::Array<int>& intervals);

        /// Renvoie les notes de la gamme (demi-tons 0..11) pour le mode Custom.
        /// En mode non-Custom, les intervalles sont calcules depuis key+scale.
        const juce::Array<int>& getCustomIntervals() const { return customIntervalsCache; }

        /// Definit les notes de la gamme pour le mode Custom.
        void setCustomIntervals (const juce::Array<int>& intervals)
        {
            customIntervalsCache = intervals;
        }

        /// Push current harmony frequencies (at given transport time) so the
        /// editor can draw harmony traces aligned with the curve timeline.
        void addHarmonySamples (double time, const juce::Array<float>& freqs);

        /// Definit la position du playhead (en PPQ) et l'etat de lecture
        /// du DAW. L'auto-scroll n'est actif que si le DAW joue vraiment.
        void setPlayheadTime (double time, bool isHostPlaying);

        /// Definit le nombre de mesures visibles (1, 2, 4, 8).
        void setMeasuresVisible (int measures);

        /// Definit la time signature courante (numerator/denominator).
        void setTimeSignature (int numerator, int denominator);

        /// Active/desactive le defilement automatique (auto-scroll ARA).
        void setAutoScroll (bool enabled);

        /// Definit le nombre de mesures sans passer par le combo.
        void setMeasuresWithoutCombo (int measures);

        /// Accesseurs pour les contrôles embarqués (synchronisation PluginEditor)
        juce::ComboBox& getMeasuresBox() { return measuresBox; }
        juce::ToggleButton& getAutoScrollToggle() { return autoScrollToggle; }

        /// Active/desactive l'edition (utilise pour griser en mode Auto).
        void setEditorEnabled (bool b);
        bool isEditorEnabled() const { return editorEnabled; }

        /// Acces au clavier piano (pour le configurer depuis l'exterieur).
        PianoKeyboard& getPianoKeyboard() { return pianoKeyboard; }

        // Listener (un seul pour MVP).
        void addListener (Listener* l) { listener = l; }
        void removeListener() { listener = nullptr; }

        std::function<void(const juce::MouseEvent&)> onRightClick;

    private:
        // === Donnees ===
        atdsp::PitchCurve curve;

        // Clavier piano affiche sur la gauche.
        PianoKeyboard pianoKeyboard;

        // Drag en cours.
        int  dragIndex = -1;
        bool isDragging = false;
        bool isDraggingSelection = false;
        bool isMarqueeSelecting = false;
        juce::Point<float> marqueeStart;
        juce::Rectangle<float> marqueeRect;
        juce::Array<int> selectedIndices;
        juce::Array<double> selectionStartTimes;
        juce::Array<float> selectionStartPitches;
        int selectionAnchorLocal = -1;

        // Snap et gamme.
        bool snapEnabled = true;
        bool snapToGridEnabled = false;
        int  keyIdx = 0;
        atdsp::Scale currentScale = atdsp::Scale::Major;
        juce::Array<int> customIntervalsCache; // copie locale des notes (mode Custom)

        // Etat d'activation (false en mode Auto -> lecture seule).
        bool editorEnabled = true;

        // Vue.
        double timeVisible = 16.0; // calcul automatique par recalculateTimeVisible()
        float  minHz = 50.0f;
        float  maxHz = 1000.0f;

        // Measures and time signature (Feature 1).
        int measuresVisible = 4;
        int timeSigNum = 4;
        int timeSigDen = 4;
        void recalculateTimeVisible();

        // Auto-scroll (Feature 2).
        double scrollOffset = 0.0;
        bool autoScrollEnabled = false;
        bool wasPlayingLastFrame = false;
        double stoppedPlayheadTime = -1.0;

        // Embedded controls (Feature 1 & 2).
        // These are child components owned by PitchCurveEditor so they are
        // properly parented inside the tab page (not overlapping tab buttons).
        juce::ComboBox measuresBox;
        juce::Label    measuresLabel;
        juce::ToggleButton autoScrollToggle;

        // Harmony traces storage (per-voice time/pitch samples)
        static constexpr int maxHarmonyVoices = 8;
        juce::Array<juce::Array<double>> harmonyTimes;
        juce::Array<juce::Array<float>>  harmonyPitches;

        // Position du playhead (secondes). 0 par defaut.
        double playheadTime = 0.0;

        // Couleurs.
        static const juce::Colour kCurveColour;
        static const juce::Colour kPointColour;
        static const juce::Colour kGridColour;

        // Hover et Tooltip
        int hoverIndex = -1;
        juce::String getNoteName (float hz) const;

        // Conversion temps / pitch <-> pixels.
        double timeToX (double t) const;
        double xToTime (float x) const;
        float  pitchToY (float p) const;
        float  yToPitch (float y) const;

        // Trouve le point le plus proche d'une position pixel.
        int findPointAtPixel (juce::Point<float> p, float maxDist = 30.0f) const;

        // Notifie le listener.
        void notifyChanged();

        Listener* listener = nullptr;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchCurveEditor)
    };
}
