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
        /// Trackpad pinch-to-zoom (macOS delivers a pinch as mouseMagnify;
        /// the Ctrl/Cmd + wheel equivalent is handled in mouseWheelMove).
        void mouseMagnify (const juce::MouseEvent& e, float scaleFactor) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;

        /// Ajoute un echantillon de pitch (Hz) d'entree.
        void pushInputPitch (float hz);
        /// Ajoute un echantillon de pitch (Hz) corrige.
        void pushOutputPitch (float hz);

        /// Met a jour les informations de note / offset pour l'affichage.
        void setNoteInfo (const ovtdsp::NoteInfo& info);

        /// Definit les notes de la gamme actuelle (demi-tons 0..11) pour
        /// tracer les lignes de la gamme en arriere-plan.
        void setScaleIntervals (const juce::Array<int>& intervals);
        /// Provide the frequencies (Hz) of currently active harmony voices
        /// to be pushed into the harmony history (time series) so they are
        /// displayed as thin blue lines following the same timeline as input/output.
        void setHarmonyFrequencies (const juce::Array<float>& freqs);

        /// Accesseur au clavier de piano integre (correctif R2.2).
        PianoKeyboard& getPianoKeyboard() { return pianoKeyboard; }

        /// Export the visualizer as an image file.
        /// @param filePath  destination file path (.png or .jpg)
        /// @return          true if the image was saved successfully
        bool exportAsImage (const juce::File& filePath);

        /// Set waveform data for ARA2 overlay display.
        /// @param samples  audio sample data
        /// @param numSamples  number of samples
        /// @param sampleRate  sample rate in Hz
        void setWaveformOverlay (const float* samples, int numSamples, double sampleRate);

        /// Set the waveform display type (0=Line, 1=Mirror).
        void setDisplayType (int type) { currentDisplayType = type; repaint(); }

        /// Public scroll methods (called by UI buttons).
        void scrollUp();
        void scrollDown();
        void zoomIn();
        void zoomOut();
        void resetView();

    private:
        // Historique des pitches (Hz), taille max ~5 secondes a 30 fps.
        static constexpr int historySize = 150;
        juce::Array<float> inputHistory;
        juce::Array<float> outputHistory;

        // Note info + gamme courante (mis a jour par le processor / editor).
        ovtdsp::NoteInfo noteInfo;
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
        static const juce::Colour kHarmonyColour;

        // Limites d'affichage (Hz, echelle log).
        float fMin = 50.0f;
        float fMax = 1500.0f;

        // Default frequency range (for reset).
        static constexpr float kDefaultFMin = 50.0f;
        static constexpr float kDefaultFMax = 1500.0f;

        // Scroll/zoom control buttons (SVG icon buttons).
        juce::DrawableButton zoomInButton    { "Zoom In",    juce::DrawableButton::ImageOnButtonBackground };
        juce::DrawableButton zoomOutButton   { "Zoom Out",   juce::DrawableButton::ImageOnButtonBackground };
        juce::DrawableButton scrollUpButton  { "Scroll Up",  juce::DrawableButton::ImageOnButtonBackground };
        juce::DrawableButton scrollDownButton{ "Scroll Down",juce::DrawableButton::ImageOnButtonBackground };
        juce::DrawableButton resetViewButton { "Reset View", juce::DrawableButton::ImageOnButtonBackground };

        // === ARA2 Waveform overlay ===
        juce::AudioBuffer<float> waveformBuffer;
        double waveformSampleRate = 44100.0;
        bool hasWaveform = false;
        int currentDisplayType = 0;
        void paintWaveformOverlay (juce::Graphics& g, juce::Rectangle<int> plotArea);

        // Hover state for frequency cursor display.
        bool isMouseOverPlot = false;
        float hoverHz = 0.0f;

        // === Tuning Statistics ===
        static constexpr int statsWindowSize = 300;  // ~10 seconds at 30fps
        juce::Array<float> centsHistory;
        float avgCents = 0.0f;
        float stdDevCents = 0.0f;
        float percentInTune = 0.0f;  // % within +/- 15 cents
        int totalSamples = 0;
        int inTuneSamples = 0;

        /** Recalculate tuning statistics (avg, stddev, in-tune %) from centsHistory. */
        void updateStatistics();

        // Animated zoom/scroll (smooth transitions) — used by the toolbar
        // buttons. Trackpad gestures apply immediately instead (see mouseWheelMove /
        // mouseMagnify) so they track the fingers 1:1 like the Curve Editor.
        float targetFMin = 50.0f;
        float targetFMax = 1500.0f;
        bool  animating = false;

        // Timestamp (ms) of the last pinch gesture, used to suppress the
        // concurrent trackpad scroll that can accompany a pinch.
        juce::uint32 lastMagnifyMs = 0;

        // Setup helper for SVG icon buttons.
        void setupIconBtn (juce::DrawableButton& btn, const char* svgXml,
                           const juce::String& tooltip, bool isToggle = false);

        // Conversion Hz -> Y pixel.
        float hzToY (float hz, int height) const;

        // Y pixel -> Hz (inverse of hzToY).
        float yToHz (float y, int height) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchVisualizer)
    };
}
