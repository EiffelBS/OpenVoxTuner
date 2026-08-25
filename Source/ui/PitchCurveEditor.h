// PitchCurveEditor.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/PitchCurve.h"
#include "../dsp/ScaleQuantizer.h"
#include "PianoKeyboard.h"

namespace ui
{
    /**
     * Interactive PitchCurve editor.
     * Communicates with the processor via a listener (curve changes).
     */
    class PitchCurveEditor : public juce::Component,
                             public juce::Timer
    {
    public:
        /** Notifies the processor of a curve change. */
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

        // === Mouse input ===
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

        // === Public API ===

        /// Access to the curve (read-only recommended from outside).
        const ovtdsp::PitchCurve& getCurve() const { return curve; }

        /// Replaces the curve (e.g. after loading a preset).
        /// Also resets the editing options (snap, grid, step) to their
        /// default values to avoid UI state conflicts.
        void setCurve (const ovtdsp::PitchCurve& newCurve);

        /// Resets the editing options to their defaults:
        /// snap=ON, stepMode=OFF, snapToGrid=OFF.
        void resetEditState();

        /// Captures the current pitch value (provided by the processor) as
        /// a point on the curve at time 'currentTime'. Used in
        /// "Live recording" mode.
        void capturePitch (float hz, double currentTime);

        /// Sets the time zoom (displayed seconds) and the pitch zoom.
        void setViewRange (double secondsVisible, float minHz, float maxHz);

        /// Zooms the pitch view in (narrower range), centered on the current center pitch.
        void zoomIn();
        /// Zooms the pitch view out (wider range).
        void zoomOut();
        /// Pans the pitch view up (higher pitches).
        void scrollUp();
        /// Pans the pitch view down (lower pitches).
        void scrollDown();

        /// Exports the curve editor as a PNG image (2x resolution), like
        /// the Live visualizer. Uses the editor's paint() directly.
        /// @param filePath  destination path (.png)
        /// @return          true if the export succeeded
        bool exportAsImage (const juce::File& filePath);
        /// Resets the view: default pitch range and time scroll back to the start.
        void resetView();

        /// Enables/disables snap to scale.
        void setSnapEnabled (bool b);
        bool isSnapEnabled() const { return snapEnabled; }

        /// Enables or disables snapping to the time grid.
        void setSnapToGridEnabled (bool b);
        bool isSnapToGridEnabled() const { return snapToGridEnabled; }

        /// Enables or disables "staircase" mode (Step Mode) for interpolation.
        void setStepModeEnabled (bool b);
        bool isStepModeEnabled() const { return curve.isStepMode(); }

        /// Clear the curve (Reset)
        void clearCurve() { curve.clear(); repaint(); notifyChanged(); }

        /** Import a pitch curve from an external source (e.g. MIDI file).
         *  Records an undo snapshot so the import can be undone with Ctrl+Z.
         *  Step mode is forced ON (MIDI notes are discrete). */
        void importMidiCurve (const ovtdsp::PitchCurve& newCurve);

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

        /// Sets the scale (for snapping).
        void setKeyAndScale (int key, ovtdsp::Scale scale);

        /// Sets the scale notes for the piano display.
        /// List of semitones 0..11 relative to C.
        void setScaleIntervals (const juce::Array<int>& intervals);

        /// Returns the scale notes (semitones 0..11) for Custom mode.
        /// In non-Custom mode, intervals are computed from key+scale.
        const juce::Array<int>& getCustomIntervals() const { return customIntervalsCache; }

        /// Sets the scale notes for Custom mode.
        void setCustomIntervals (const juce::Array<int>& intervals)
        {
            customIntervalsCache = intervals;
        }

        /// Push current harmony frequencies (at given transport time) so the
        /// editor can draw harmony traces aligned with the curve timeline.
        void addHarmonySamples (double time, const juce::Array<float>& freqs);

        /// Enables/disables the harmony trace display (blue lines).
        /// Enabled by default: harmony voice traces are shown as soon as
        /// samples are received via addHarmonySamples().
        void setShowHarmoniesTrace (bool show) { showHarmoniesTrace = show; repaint(); }
        /// Returns the harmony trace display state.
        bool getShowHarmoniesTrace() const { return showHarmoniesTrace; }

        /// Show/hide the "FOLLOWS MIDI IN" badge (driven by the MIDI target).
        void setMidiFollowActive (bool active) { midiFollowActive = active; repaint(); }
        bool getMidiFollowActive() const { return midiFollowActive; }

        /// Set the frequency (Hz) of the MIDI note driving the tuning, used to
        /// draw a dashed target line in the editor (0 = none).
        void setMidiTargetHz (float hz) { midiTargetHz = hz; repaint(); }
        float getMidiTargetHz() const { return midiTargetHz; }

        /// Push an input pitch sample with its timestamp for trace display.
        void addInputTraceSample (double time, float hz);
        /// Clear the input pitch trace.
        void clearInputTrace();

        /// Enables/disables the live input trace display (monitoring/capture).
        /// Disabled by default: the editable curve displays cleanly at startup,
        /// without the red audio input trace (which may pick up stray signal).
        void setShowInputTrace (bool show) { showInputTrace = show; if (! show) clearInputTrace(); repaint(); }
        /// Returns the live input trace display state.
        bool getShowInputTrace() const { return showInputTrace; }

        /// Sets the playhead position (in PPQ) and the DAW playing state.
        /// Auto-scroll is only active if the DAW is really playing.
        void setPlayheadTime (double time, bool isHostPlaying, bool isLooping = false);

        /// Moves the playhead back to the start and scrolls the view to reveal
        /// time 0. Used by the "Return to start" button and the "Reset Playhead"
        /// menu item to guarantee a reliable return on the first click
        /// (without relying on the seek detector in setPlayheadTime). Keeps
        /// the zoom and pitch range.
        void returnToStart();

        /// Time snapping (project grid) used by ruler clicks and
        /// double-clicks. Public/static to allow unit testing.
        static double snapTimeToGrid (double t, bool snapToGridEnabled)
        {
            const double gridStep = 0.5; // project grid (quarter note = 1.0 beat)
            const double nearest = std::round (t / gridStep) * gridStep;
            if (snapToGridEnabled)
                return nearest;
            if (std::abs (t - nearest) < 0.05)
                return nearest;
            return t;
        }
        /// Clamps the horizontal scroll offset (>= 0). Public/static for tests.
        static double clampScrollOffset (double offset)
        {
            return offset < 0.0 ? 0.0 : offset;
        }

        /// Seek request callback (ruler click). Connects the editor to the
        /// processor transport without direct editor->processor coupling.
        std::function<void(double)> onSeek;

        /// Sets the number of visible measures (1, 2, 4, 8).
        void setMeasuresVisible (int measures);

        /// Sets the current time signature (numerator/denominator).
        void setTimeSignature (int numerator, int denominator);

        /// Enables/disables automatic scrolling (ARA auto-scroll / standalone timeline).
        void setAutoScroll (bool enabled);
        /// Returns the auto-scroll state (used by the Options menu).
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

        /// Enables/disables editing (used to gray out in Auto mode).
        void setEditorEnabled (bool b);
        bool isEditorEnabled() const { return editorEnabled; }

        /// Clears the harmony traces (to hide them when entering the Curve Editor tab)
        void clearHarmonyTraces();

        /// Access to the piano keyboard (to configure it from outside).
        PianoKeyboard& getPianoKeyboard() { return pianoKeyboard; }

        /// Undo/Redo button access (for PluginEditor to position them)
        juce::TextButton& getUndoButton() { return undoButton; }
        juce::TextButton& getRedoButton() { return redoButton; }

        /// Refresh all translatable strings after a language change.
        void refreshTranslations();

        // Listener (a single one for MVP).
        void addListener (Listener* l) { listener = l; }
        void removeListener() { listener = nullptr; }

        std::function<void(const juce::MouseEvent&)> onRightClick;

    private:
        // === Data ===
        ovtdsp::PitchCurve curve;
        ovtdsp::PitchCurve ghostCurve;   // owned copy of the morph-target overlay
        bool hasGhostCurve = false;    // whether the ghost overlay is currently active

        // Piano keyboard displayed on the left.
        PianoKeyboard pianoKeyboard;

        // Ongoing drag.
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

        // Mouse horizontal scrolling (middle button), active when auto-scroll is OFF.
        bool isMiddleScrolling = false;
        double middleDragStartX = 0.0;
        double middleDragStartScroll = 0.0;

        // Snap and scale.
        bool snapEnabled = true;
        bool snapToGridEnabled = false;
        int  keyIdx = 0;
        ovtdsp::Scale currentScale = ovtdsp::Scale::Major;
        juce::Array<int> customIntervalsCache; // local copy of the notes (Custom mode)
        juce::Array<int> scaleIntervals; // scale notes for the reference lines

        // Enabled state (false in Auto mode -> read-only).
        bool editorEnabled = true;

        // "Piano Roll" editing metaphor (second metaphor for the same curve).
        bool pianoRollMode = false;

        // View.
        double timeVisible = 16.0; // automatically computed by recalculateTimeVisible()
        float  minHz = 50.0f;
        float  maxHz = 1000.0f;

        // Measures and time signature (Feature 1).
        int measuresVisible = 4;
        int timeSigNum = 4;
        int timeSigDen = 4;
        void recalculateTimeVisible();

        // Clamps the pitch range (1..8 octaves, C0..C9) like mouseWheelMove.
        void clampPitchRange();

        // Zooms the pitch view around a reference pitch (mouse / finger).
        // factor > 1 => zoom in (narrower range), clamped to 1..8 octaves.
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
        // Toggle for harmony blue lines (set by the Curve Editor hamburger menu).
        bool showHarmoniesTrace = true;

        // Input pitch trace (red line, same as PitchVisualizer)
        juce::Array<double> inputTraceTimes;
        juce::Array<float>  inputTracePitches;
        bool showInputTrace = true;
        bool midiFollowActive = false;  // "Tuning follows MIDI IN" badge state
        float midiTargetHz = 0.0f;      // MIDI target note frequency for the line

        // Playhead position (seconds). 0 by default.
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

        // Colors.
        static const juce::Colour kCurveColour;
        static const juce::Colour kPointColour;
        static const juce::Colour kGridColour;

        // Hover and Tooltip
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

        // Curve snapshot before a modification (for undo)
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

        // Time / pitch <-> pixel conversion.
        double timeToX (double t) const;
        double xToTime (float x) const;
        float  pitchToY (float p) const;
        float  yToPitch (float y) const;

        // Snap a frequency to the nearest MIDI note (piano-roll editing).
        float snapToNearestNote (float hz) const;

        // Finds the point closest to a pixel position.
        int findPointAtPixel (juce::Point<float> p, float maxDist = 30.0f) const;

        // Notifies the listener.
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



