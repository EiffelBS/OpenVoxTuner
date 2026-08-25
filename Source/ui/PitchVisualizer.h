// PitchVisualizer.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/NoteUtils.h"
#include "PianoKeyboard.h"

namespace ui
{
    /**
     * Displays the input pitch curve and the corrected pitch curve,
     * with overlays:
     *   - the currently sung note (e.g. "F3")
     *   - the offset in cents relative to the quantized note
     *   - a vertical tuning meter (Antares / Studio One style)
     *   - horizontal lines for the notes of the selected scale
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
        /// Right-click opens the wrench menu (via onRightClick callback).
        void mouseDown (const juce::MouseEvent& event) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;

        /// Adds an input pitch sample (Hz).
        void pushInputPitch (float hz);
        /// Adds a corrected pitch sample (Hz).
        void pushOutputPitch (float hz);

        /// Updates the note / offset information for display.
        void setNoteInfo (const ovtdsp::NoteInfo& info);

        /// Sets the notes of the current scale (semitones 0..11) to draw
        /// the scale lines in the background.
        void setScaleIntervals (const juce::Array<int>& intervals);
        /// Provide the frequencies (Hz) of currently active harmony voices
        /// to be pushed into the harmony history (time series) so they are
        /// displayed as thin blue lines following the same timeline as input/output.
        void setHarmonyFrequencies (const juce::Array<float>& freqs);

        /// Show/hide harmony trace lines (blue) in the visualizer.
        void setShowHarmonies (bool show) { showHarmonies = show; repaint(); }
        bool getShowHarmonies() const { return showHarmonies; }

        /// Show/hide the "FOLLOWS MIDI IN" badge (driven by the MIDI target).
        void setMidiFollowActive (bool active) { midiFollowActive = active; repaint(); }
        bool getMidiFollowActive() const { return midiFollowActive; }

        /// Accessor to the built-in piano keyboard (R2.2 fix).
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

        /// Auto-center: keeps the output pitch vertically centered.
        void setAutoCenter (bool enabled);
        bool isAutoCenter() const { return autoCenter; }

        /// Zoom state save/restore (Hz range).
        void setZoomRange (float newFMin, float newFMax);
        float getFMin() const { return fMin; }
        float getFMax() const { return fMax; }

        /// Callback fired when zoom range changes (for persisting to APVTS).
        std::function<void(float fMinHz, float fMaxHz)> onZoomChanged;

        /// Callback fired on right-click (used to open the wrench menu).
        std::function<void()> onRightClick;

    private:
        // Pitch history (Hz), max size ~5 seconds at 30 fps.
        static constexpr int historySize = 150;
        juce::Array<float> inputHistory;
        juce::Array<float> outputHistory;

        // Note info + current scale (updated by the processor / editor).
        ovtdsp::NoteInfo noteInfo;
        juce::Array<int> scaleIntervals;

        // Vertical piano
        PianoKeyboard pianoKeyboard;
        // Harmony history: one history buffer per voice (fixed max voices)
        static constexpr int maxHarmonyVoices = 8;
        juce::Array<juce::Array<float>> harmonyHistory;
        bool showHarmonies = true;  // togglable via Curve Editor hamburger menu
        bool midiFollowActive = false;  // "Tuning follows MIDI IN" badge state

        // Storage of the latest value
        float latestInputHz = 0.0f;
        float latestOutputHz = 0.0f;

        // Theme colours.
        static const juce::Colour kBg;
        static const juce::Colour kGrid;
        static const juce::Colour kInputColour;
        static const juce::Colour kOutputColour;
        static const juce::Colour kScaleLineColour;
        static const juce::Colour kHarmonyColour;

        // Display limits (Hz, log scale).
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
        // `waveformBuffer` keeps the most recent audio block for the Line /
        // Mirror display types (unchanged behaviour).
        juce::AudioBuffer<float> waveformBuffer;
        double waveformSampleRate = 44100.0;
        bool hasWaveform = false;
        int currentDisplayType = 0;
        void paintWaveformOverlay (juce::Graphics& g, juce::Rectangle<int> plotArea);

        // Ring buffer of recent audio samples. The host audio block size is
        // typically < 512 samples, so a single block can never fill the 512-sample
        // FFT window the Spectral view needs. We accumulate recent samples here so
        // the Spectral (FFT) view always has enough data to compute a spectrum.
        static constexpr int kWaveRingCapacity = 2048;
        juce::AudioBuffer<float> waveformRing;        // circular, capacity kWaveRingCapacity
        int waveformRingWritePos = 0;
        int waveformTotalWritten = 0;
        juce::AudioBuffer<float> waveformTailBuffer;  // contiguous tail for the spectral draw

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

        // Animated zoom/scroll (smooth transitions) - used by the toolbar
        // buttons. Trackpad gestures apply immediately instead (see mouseWheelMove /
        // mouseMagnify) so they track the fingers 1:1 like the Curve Editor.
        float targetFMin = 50.0f;
        float targetFMax = 1500.0f;
        bool  animating = false;

        // Timestamp (ms) of the last pinch gesture, used to suppress the
        // concurrent trackpad scroll that can accompany a pinch.
        juce::uint32 lastMagnifyMs = 0;

        // Animated unified note badge width (smooth transition between
        // single-note and split-note display modes).
        float badgeAnimW = 90.0f;
        float badgeTargetW = 90.0f;
        float badgeSplitX = 90.0f;
        float badgeTargetSplitX = 90.0f;

        // Auto-center: keeps the output pitch vertically centered.
        bool  autoCenter = false;
        float smoothedOutputHz = 5.61f; // log(440 Hz) - initial seed

        // Setup helper for SVG icon buttons.
        void setupIconBtn (juce::DrawableButton& btn, const char* svgXml,
                           const juce::String& tooltip, bool isToggle = false);

        // Hz -> Y pixel conversion.
        float hzToY (float hz, int height) const;

        // Y pixel -> Hz (inverse of hzToY).
        float yToHz (float y, int height) const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchVisualizer)
    };
}



