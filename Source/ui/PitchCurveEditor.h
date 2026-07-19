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

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
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
        void mouseExit (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        // Trackpad pinch-to-zoom (macOS delivers a pinch as mouseMagnify; the
        // Ctrl/Cmd + wheel equivalent on desktop is handled in mouseWheelMove).
        void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;

        // === Keyboard (copy/paste, undo/redo) ===
        bool keyPressed (const juce::KeyPress& key) override;

        // === API publique ===

        /// Acces a la courbe (lecture seule recommande depuis l'exterieur).
        const ovtdsp::PitchCurve& getCurve() const { return curve; }

        /// Remplace la courbe (apres chargement de preset par exemple).
        /// Reinitialise aussi les options d'edition (snap, grid, step) a leurs
        /// valeurs par defaut pour eviter les conflits d'etat UI.
        void setCurve (const ovtdsp::PitchCurve& newCurve);
        
        /// Reinitialise les options d'edition aux valeurs par defaut :
        /// snap=ON, stepMode=OFF, snapToGrid=OFF.
        void resetEditState();

        /// Capture la valeur du pitch courant (fourni par le processor) comme
        /// un point sur la courbe au temps 'currentTime'. Utilise en mode
        /// "Live recording".
        void capturePitch (float hz, double currentTime);

        /// Definit le zoom en temps (secondes affichees) et le zoom en pitch.
        void setViewRange (double secondsVisible, float minHz, float maxHz);

        /// Zoom la vue en pitch (range plus etroit), centre sur le pitch central courant.
        void zoomIn();
        /// Dezoom la vue en pitch (range plus large).
        void zoomOut();
        /// Pan la vue en pitch vers le haut (pitches plus aigus).
        void scrollUp();
        /// Pan la vue en pitch vers le bas (pitches plus graves).
        void scrollDown();

        /// Exporte l'editeur de courbe en image PNG (resolution 2x), comme
        /// le visualiseur Live. Utilise le paint() de l'editeur directement.
        /// @param filePath  chemin de destination (.png)
        /// @return          true si l'export a reussi
        bool exportAsImage (const juce::File& filePath);
        /// Reinitialise la vue : range de pitch par defaut et scroll temporel au debut.
        void resetView();

        /// Active/desactive le snap a la gamme.
        void setSnapEnabled (bool b);
        bool isSnapEnabled() const { return snapEnabled; }

        /// Active ou desactive le magnetisme sur la grille temporelle.
        void setSnapToGridEnabled (bool b);
        bool isSnapToGridEnabled() const { return snapToGridEnabled; }

        /// Active ou desactive le mode "escalier" (Step Mode) pour l'interpolation.
        void setStepModeEnabled (bool b);
        bool isStepModeEnabled() const { return curve.isStepMode(); }

        /// Vide la courbe (Reset)
        void clearCurve() { curve.clear(); repaint(); notifyChanged(); }

        /// Ghost curve overlay for preset morphing (semi-transparent target curve).
        /// The curve is COPIED by value: the ghost must never keep a raw pointer to a
        /// caller-owned object, because the editor keeps painting it on the message
        /// thread while the caller's object (e.g. a unique_ptr<MorphState> for an A/B
        /// slot) can be destroyed/replaced at any time. Holding a dangling pointer
        /// here crashed paint() with EXC_BAD_ACCESS (ghostCurve->getPitchAt reading a
        /// freed MorphState). Passing nullptr (or a curve with < 2 points) clears it.
        void setGhostCurve (const ovtdsp::PitchCurve* ghost)
        {
            if (ghost != nullptr && ghost->getNumPoints() >= 2)
            {
                ghostCurve = *ghost;
                hasGhostCurve = true;
            }
            else
            {
                hasGhostCurve = false;
            }
            repaint();
        }

        /// Definit la gamme (pour le snap).
        void setKeyAndScale (int key, ovtdsp::Scale scale);

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

        /// Push an input pitch sample with its timestamp for trace display.
        void addInputTraceSample (double time, float hz);
        /// Clear the input pitch trace.
        void clearInputTrace();

        /// Active/desactive l'affichage de la trace d'entree live (monitoring/capture).
        /// Par defaut desactivee : la courbe editable s'affiche proprement au lancement,
        /// sans la trace rouge de l'entree audio (qui peut capter un signal parasite).
        void setShowInputTrace (bool show) { showInputTrace = show; if (! show) clearInputTrace(); repaint(); }
        /// Retourne l'etat d'affichage de la trace d'entree live.
        bool getShowInputTrace() const { return showInputTrace; }

        /// Definit la position du playhead (en PPQ) et l'etat de lecture
        /// du DAW. L'auto-scroll n'est actif que si le DAW joue vraiment.
        void setPlayheadTime (double time, bool isHostPlaying, bool isLooping = false);

        /// Replace le playhead au debut et fait defiler la vue pour reveler le
        /// temps 0. Utilise par le bouton "Retour au debut" et l'item de menu
        /// "Reset Playhead" pour garantir un retour fiable au premier clic
        /// (sans dependre du detecteur de seek de setPlayheadTime). Preserve
        /// le zoom et la plage de pitch.
        void returnToStart();

        /// Snap temporel (grille de projet) utilise par le clic sur la regle et
        /// le double-clic. Public/statique pour permettre des tests unitaires.
        static double snapTimeToGrid (double t, bool snapToGridEnabled)
        {
            const double gridStep = 0.5; // grille de projet (noire = 1.0 beat)
            const double nearest = std::round (t / gridStep) * gridStep;
            if (snapToGridEnabled)
                return nearest;
            if (std::abs (t - nearest) < 0.05)
                return nearest;
            return t;
        }
        /// Borne le decalage de scroll horizontal (>= 0). Public/statique pour tests.
        static double clampScrollOffset (double offset)
        {
            return offset < 0.0 ? 0.0 : offset;
        }

        /// Callback de demande de seek (clic sur la regle). Relie l'editeur au
        /// transport du processeur sans couplage direct editeur->processeur.
        std::function<void(double)> onSeek;

        /// Definit le nombre de mesures visibles (1, 2, 4, 8).
        void setMeasuresVisible (int measures);

        /// Definit la time signature courante (numerator/denominator).
        void setTimeSignature (int numerator, int denominator);

        /// Active/desactive le defilement automatique (auto-scroll ARA / standalone timeline).
        void setAutoScroll (bool enabled);
        /// Retourne l'etat de l'auto-scroll (utilise par le menu Options).
        bool getAutoScroll() const { return autoScrollEnabled; }

        void setWaveformOverlay (const float* samples, int numSamples, double sampleRate);

        /// Set the waveform display type (0=Bars, 1=Filled, 2=Line, 3=Mirror).
        void setDisplayType (int type) { currentDisplayType = type; repaint(); }

        /// Enable/disable the "Piano Roll" editing metaphor. The SAME PitchCurve
        /// model is edited; only the rendering and hit-testing change (notes are
        /// snapped to the keyboard rows instead of a continuous line). This is a
        /// second metaphor that does not replace the curve editor.
        ///
        /// When Piano Roll mode is enabled, Step Mode is forced on regardless of
        /// the loaded preset: a piano-roll layout shows discrete note rows, so a
        /// sliding (linear) interpolation would be visually inconsistent with the
        /// keyboard rows and would also make the audio glide between notes.
        void setPianoRollMode (bool b)
        {
            pianoRollMode = b;
            if (b)
                curve.setStepMode (true);
            repaint();
        }
        bool isPianoRollMode() const { return pianoRollMode; }

        /// Active/desactive l'edition (utilise pour griser en mode Auto).
        void setEditorEnabled (bool b);
        bool isEditorEnabled() const { return editorEnabled; }

        /// Vide les traces d'harmonie (pour cacher quand on entre en onglet Curve Editor)
        void clearHarmonyTraces();

        /// Acces au clavier piano (pour le configurer depuis l'exterieur).
        PianoKeyboard& getPianoKeyboard() { return pianoKeyboard; }

        /// Undo/Redo button access (for PluginEditor to position them)
        juce::TextButton& getUndoButton() { return undoButton; }
        juce::TextButton& getRedoButton() { return redoButton; }

        /// Refresh all translatable strings after a language change.
        void refreshTranslations();

        // Listener (un seul pour MVP).
        void addListener (Listener* l) { listener = l; }
        void removeListener() { listener = nullptr; }

        std::function<void(const juce::MouseEvent&)> onRightClick;

    private:
        // === Donnees ===
        ovtdsp::PitchCurve curve;
        ovtdsp::PitchCurve ghostCurve;   // owned copy of the morph-target overlay
        bool hasGhostCurve = false;    // whether the ghost overlay is currently active

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

        // Scroll horizontal a la souris (bouton milieu), actif quand auto-scroll OFF.
        bool isMiddleScrolling = false;
        double middleDragStartX = 0.0;
        double middleDragStartScroll = 0.0;

        // Snap et gamme.
        bool snapEnabled = true;
        bool snapToGridEnabled = false;
        int  keyIdx = 0;
        ovtdsp::Scale currentScale = ovtdsp::Scale::Major;
        juce::Array<int> customIntervalsCache; // copie locale des notes (mode Custom)
        juce::Array<int> scaleIntervals; // notes de la gamme pour les lignes de reference

        // Etat d'activation (false en mode Auto -> lecture seule).
        bool editorEnabled = true;

        // "Piano Roll" editing metaphor (second metaphor for the same curve).
        bool pianoRollMode = false;

        // Vue.
        double timeVisible = 16.0; // calcul automatique par recalculateTimeVisible()
        float  minHz = 50.0f;
        float  maxHz = 1000.0f;

        // Measures and time signature (Feature 1).
        int measuresVisible = 4;
        int timeSigNum = 4;
        int timeSigDen = 4;
        void recalculateTimeVisible();

        // Borne le range de pitch (1..8 octaves, C0..C9) comme mouseWheelMove.
        void clampPitchRange();

        // Zoom la vue en pitch autour d'un pitch de reference (souris / doigt).
        // factor > 1 => zoom avant (range plus etroit), borne a 1..8 octaves.
        void applyZoom (float anchorPitch, float factor);

        // Auto-scroll (Feature 2).
        double scrollOffset = 0.0;
        bool autoScrollEnabled = false;
        // True when the playhead loops on the Measures window (Standalone, or the
        // VST3 "Loop Playhead (Measures)" option). In that mode the view is locked
        // to the loop window, so manual middle-button horizontal scroll is disabled.
        bool loopingPlayhead = false;
        bool wasPlayingLastFrame = false;

        // Harmony traces storage (per-voice time/pitch samples)
        static constexpr int maxHarmonyVoices = 8;
        juce::Array<juce::Array<double>> harmonyTimes;
        juce::Array<juce::Array<float>>  harmonyPitches;

        // Input pitch trace (red line, same as PitchVisualizer)
        juce::Array<double> inputTraceTimes;
        juce::Array<float>  inputTracePitches;
        bool showInputTrace = true;

        // Position du playhead (secondes). 0 par defaut.
        double playheadTime = 0.0;

        // Hover cursor (horizontal line + note/Hz readout).
        float hoverMouseX = 0.0f;
        float hoverMouseY = 0.0f;
        bool isMouseOverPlot = false;

        // Waveform overlay (same data as PitchVisualizer).
        juce::AudioBuffer<float> waveformBuffer;
        bool hasWaveform = false;
        int currentDisplayType = 0;

        // Ring buffer of recent audio samples so the Spectral (FFT) view always
        // has a >= 512-sample window to compute a spectrum, regardless of the
        // host audio block size.
        static constexpr int kWaveRingCapacity = 2048;
        juce::AudioBuffer<float> waveformRing;        // circular, capacity kWaveRingCapacity
        int waveformRingWritePos = 0;
        int waveformTotalWritten = 0;
        juce::AudioBuffer<float> waveformTailBuffer;  // contiguous tail for the spectral draw

        // Couleurs.
        static const juce::Colour kCurveColour;
        static const juce::Colour kPointColour;
        static const juce::Colour kGridColour;

        // Hover et Tooltip
        int hoverIndex = -1;
        juce::String getNoteName (float hz) const;

        // === Copy/paste + Undo/Redo ===
        static juce::Array<ovtdsp::PitchPoint> clipboard;
        juce::UndoManager undoManager;

        void performCopy();
        void performPaste();
        void performDeleteSelected();
        void performUndo() { undoManager.undo(); repaint(); }
        void performRedo() { undoManager.redo(); repaint(); }
        void registerUndoableAction (juce::UndoableAction* action) { undoManager.beginNewTransaction(); undoManager.perform (action); }
        void beginTransaction (const juce::String& name) { undoManager.beginNewTransaction (name); }

        // Snapshot de la courbe avant une modification (pour undo)
        ovtdsp::PitchCurve pendingUndoSnapshot;

        // === Undoable action helper ===
        struct CurveEditAction : public juce::UndoableAction
        {
            ovtdsp::PitchCurve before;
            ovtdsp::PitchCurve after;
            PitchCurveEditor* editor;

            CurveEditAction (PitchCurveEditor* e) : editor (e) { before = e->curve; }
            void setAfter() { after = editor->curve; }

            bool perform() override;
            bool undo() override;
        };

        // Conversion temps / pitch <-> pixels.
        double timeToX (double t) const;
        double xToTime (float x) const;
        float  pitchToY (float p) const;
        float  yToPitch (float y) const;

        // Snap a frequency to the nearest MIDI note (piano-roll editing).
        float snapToNearestNote (float hz) const;

        // Trouve le point le plus proche d'une position pixel.
        int findPointAtPixel (juce::Point<float> p, float maxDist = 30.0f) const;

        // Notifie le listener.
        void notifyChanged();

        // Renders the piano-roll metaphor (note rows + blocks) when pianoRollMode is on.
        void drawPianoRoll (juce::Graphics& g);

        Listener* listener = nullptr;

        // Undo/Redo buttons (positioned by parent)
        // Hex escapes produce raw UTF-8 bytes (>127) which juce::String(const char*)
        // rejects in Debug; wrap in CharPointer_UTF8 so JUCE decodes them correctly.
        juce::TextButton undoButton { juce::CharPointer_UTF8 ("\xe2\x86\xb6") };  // Undo arrow symbol ↶
        juce::TextButton redoButton { juce::CharPointer_UTF8 ("\xe2\x86\xb7") };  // Redo arrow symbol ↷

        // Piano Roll mode toggle (second editing metaphor for the same curve).
        juce::TextButton pianoRollButton { "Piano Roll" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchCurveEditor)
    };
}
