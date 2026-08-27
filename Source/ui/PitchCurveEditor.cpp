// PitchCurveEditor.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PitchCurveEditor.h"
#include "OVTFonts.h"
#include "OVTTheme.h"
#include "OVTLanguages.h"
#include "../dsp/NoteUtils.h"
#include <cmath>

namespace ui
{
    const juce::Colour PitchCurveEditor::kCurveColour = juce::Colour (0xff4caf50); // green
    const juce::Colour PitchCurveEditor::kPointColour = juce::Colour (0xffe91e63); // pink
    const juce::Colour PitchCurveEditor::kGridColour  = juce::Colour (0x40ffffff);

    // Static clipboard for copy/paste across instances
    juce::Array<ovtdsp::PitchPoint> PitchCurveEditor::clipboard;

    // Full chromatic interval set [0, 11], used by the "snap off" magnetism
    // branch (snap to the nearest chromatic note when close enough).
    static const juce::Array<int>& getChromaticIntervals()
    {
        static const juce::Array<int> chromatic = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
        return chromatic;
    }

    PitchCurveEditor::PitchCurveEditor()
    {
        // Do NOT load the "default" preset here - the parent editor will
        // call setCurve() from the processor's pitchCurve on the first
        // timer tick (pendingCurveRestore flag). Loading "default" here
        // would flash the default preset before the real curve is synced.
        // We keep curve as an empty/zero-state object until synced.
        startTimerHz (30);

        // Allocate the spectral ring buffer (recent audio samples for the FFT view).
        waveformRing.setSize (1, kWaveRingCapacity);
        // Make sure the editor intercepts clicks even when disabled
        // (the editorEnabled state only blocks internal logic, not events).
        setInterceptsMouseClicks (true, true);
        setEnabled (true);

        // The piano keyboard is placed on the left, in the reserved area
        // (resized in resized()). We add it as a child component.
        addAndMakeVisible (pianoKeyboard);
        // The piano keyboard is display-only: it does not respond to
        // clicks. Its click interception is disabled so it does not
        // swallow mouse events intended for the curve editor (the parent),
        // which used to prevent dragging points (bug identified on 2026-06-10).
        pianoKeyboard.setInterceptsMouseClicks (false, false);
        // Default range: C2 -> C7 (enough for vocals).
        pianoKeyboard.setRange (36, 96);

        // Keyboard focus for copy/paste, undo/redo
        setWantsKeyboardFocus (true);
        setFocusContainerType (juce::Component::FocusContainerType::none);

        // init harmony buffers
        harmonyTimes.clear();
        harmonyPitches.clear();
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            harmonyTimes.add (juce::Array<double>());
            harmonyPitches.add (juce::Array<float>());
        }

        // Default states
        snapEnabled = true;
        snapToGridEnabled = true;
        setStepModeEnabled(true);

        // Undo/Redo buttons
        auto setupUndoBtn = [this] (juce::TextButton& btn, const juce::String& tip)
        {
            btn.setColour (juce::TextButton::buttonColourId, ebs::accentSoft());
                        // These chips float over the theme-invariant dark canvas
            // (vizBg), so their label stays bright under both themes.
            btn.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.92f));
            btn.setTooltip (tip);
            addAndMakeVisible (btn);
        };
        setupUndoBtn (undoButton, ovt::tr(ovt::Keys::kTooltipUndo));
        setupUndoBtn (redoButton, ovt::tr(ovt::Keys::kTooltipRedo));

        undoButton.onClick = [this] { performUndo(); };
        redoButton.onClick = [this] { performRedo(); };

        // Piano Roll mode toggle (second editing metaphor for the same curve).
        pianoRollButton.setColour (juce::TextButton::buttonColourId, ebs::accentSoft());
        pianoRollButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.92f));
        pianoRollButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff4caf50));
        pianoRollButton.setButtonText (ovt::tr (ovt::Keys::kButtonPianoRoll));
        pianoRollButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPianoRoll));
        pianoRollButton.setClickingTogglesState (true);
        pianoRollButton.setToggleState (false, juce::dontSendNotification);
        pianoRollButton.onClick = [this]
        {
            setPianoRollMode (pianoRollButton.getToggleState());
        };
        addAndMakeVisible (pianoRollButton);

        // Theme broadcast: keep the chip toolbar palette-current on switches.
        ebs::subscribeTheme (this);
    }

    PitchCurveEditor::~PitchCurveEditor()
    {
        // Theme broadcast teardown must precede member destruction.
        ebs::unsubscribeTheme (this);
        stopTimer();
    }

    void PitchCurveEditor::themeChanged() { restyleChipButtons(); }

    void PitchCurveEditor::restyleChipButtons()
    {
        juce::TextButton* chips[] = { &undoButton, &redoButton, &pianoRollButton };
        for (auto* c : chips)
        {
            // Canvas-overlay tokens: soft-accent body, bright fixed text.
            c->setColour (juce::TextButton::buttonColourId,  ebs::accentSoft());
            c->setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.92f));
            c->repaint();
        }
    }

    void PitchCurveEditor::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds();

        // Transparent background to let the PluginEditor gradient show through.
        g.fillAll (juce::Colours::transparentBlack);

        // The curve editing area starts after the piano keyboard (on the left).
        const int pianoW = pianoKeyboard.getWidth();
        const int rulerH = 24;
        const auto plotArea = juce::Rectangle<int> (pianoW, rulerH,
                                                    b.getWidth() - pianoW,
                                                    b.getHeight() - rulerH);

        // Clipping so we do not draw over the piano keyboard.
        g.saveState();
        g.reduceClipRegion (juce::Rectangle<int>(pianoW, 0, b.getWidth() - pianoW, b.getHeight()));

        // === Ruler background ===
        g.setColour (ebs::rulerBg());
        g.fillRect (pianoW, 0, b.getWidth() - pianoW, rulerH);
        
        // Ruler bottom border
        g.setColour (ebs::curveGrid());
        g.drawHorizontalLine (rulerH, static_cast<float> (pianoW), static_cast<float> (b.getWidth()));

        // === Waveform overlay ===
        if (hasWaveform)
        {
            const auto waveformArea = juce::Rectangle<int> (pianoW, rulerH,
                                                             b.getWidth() - pianoW, b.getHeight() - rulerH);

            // The Spectral (FFT) view needs a contiguous >= 512-sample window,
            // rebuilt from the ring buffer of recent samples.
            if (currentDisplayType == static_cast<int> (ovt::WaveformDisplayType::Spectral))
            {
                const int cap = kWaveRingCapacity;
                const int available = juce::jmin (cap, waveformTotalWritten);
                if (available >= (1 << 9))
                {
                    const int tailStart = (waveformRingWritePos - available + cap) % cap;
                    waveformTailBuffer.setSize (1, available, false, false, true);
                    if (tailStart + available <= cap)
                        waveformTailBuffer.copyFrom (0, 0, waveformRing, 0, tailStart, available);
                    else
                    {
                        const int first = cap - tailStart;
                        waveformTailBuffer.copyFrom (0, 0, waveformRing, 0, tailStart, first);
                        waveformTailBuffer.copyFrom (0, first, waveformRing, 0, 0, available - first);
                    }
                    ovt::drawWaveformOverlay (g, waveformTailBuffer.getReadPointer (0),
                                              available, waveformArea,
                                              ovt::WaveformDisplayType::Spectral);
                }
            }
            else if (waveformBuffer.getNumSamples() > 0)
            {
                ovt::drawWaveformOverlay (g, waveformBuffer.getReadPointer (0),
                                          waveformBuffer.getNumSamples(), waveformArea,
                                          static_cast<ovt::WaveformDisplayType> (currentDisplayType));
            }
        }

        // === Grid: horizontal lines for the C2, C3, C4, C5, C6 octaves ===
        g.setColour (ebs::curveGrid());
        const float refFreqs[] = { 65.4f, 130.8f, 261.6f, 523.3f, 1046.5f };
        const char* labels[]    = { "C2",   "C3",   "C4",   "C5",   "C6" };
        if (! pianoRollMode)
        for (int i = 0; i < 5; ++i)
        {
            const float y = pitchToY (refFreqs[i]);
            g.drawHorizontalLine (static_cast<int> (y), static_cast<float> (pianoW), static_cast<float> (b.getWidth()));
            // Text shifted to the right to avoid overlap
            // g.setColour (ebs::curveGrid().withAlpha (0.7f));
            // g.drawText (labels[i], pianoW + 4, static_cast<int> (y) - 7, 28, 14, juce::Justification::left);
            g.setColour (ebs::curveGrid());
        }

        // === Scale note lines (horizontal lines for notes in the current scale) ===
        if (! pianoRollMode && ! scaleIntervals.isEmpty())
        {
            g.setColour (ebs::scaleLine());
            const float lowestHz = minHz;
            const float highestHz = maxHz;
            const int lowestMidi = static_cast<int> (std::ceil (ovtdsp::hzToMidiFloat (lowestHz)));
            const int highestMidi = static_cast<int> (std::floor (ovtdsp::hzToMidiFloat (highestHz)));
            for (int midi = lowestMidi; midi <= highestMidi; ++midi)
            {
                const int noteInOct = ovtdsp::midiToNoteInOctave (midi);
                if (noteInOct == 0) continue; // skip C (already drawn as octave grid)
                if (! scaleIntervals.contains (noteInOct)) continue;
                const float hz = ovtdsp::midiToHz (static_cast<float> (midi));
                const float y = pitchToY (hz);
                g.drawHorizontalLine (static_cast<int> (y),
                                      static_cast<float> (pianoW),
                                      static_cast<float> (b.getWidth()));
            }
        }

        // === Piano-roll metaphor (notes snapped to the keyboard rows) ===
        if (pianoRollMode)
            drawPianoRoll (g);

        // === Vertical lines (bar/beat reference) and ruler ===
        const double beatUnit = 4.0 / timeSigDen;
        const double ppqPerBar = timeSigNum * beatUnit;
        const double rulerStart = scrollOffset;
        const double rulerEnd = scrollOffset + timeVisible;
        const double alignedStart = std::floor (rulerStart / beatUnit) * beatUnit;

        for (double t = alignedStart; t <= rulerEnd; t += beatUnit)
        {
            if (t < rulerStart) continue;

            const double x = timeToX (t);
            bool isBarStart = (std::abs (std::fmod (t, ppqPerBar)) < 0.001)
                           || (std::abs (std::fmod (t, ppqPerBar) - ppqPerBar) < 0.001);
            bool isBeat = true;

            // Vertical grid line
            g.setColour (ebs::curveGrid().withAlpha (isBarStart ? 0.6f : (isBeat ? 0.3f : 0.1f)));
            g.drawVerticalLine (static_cast<int> (x), rulerH, static_cast<float> (b.getHeight()));

            // Tick marks in the ruler
            if (isBarStart)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.drawVerticalLine (static_cast<int> (x), rulerH - 4.0f, rulerH);

                // Ruler text: bar number (1, 2, 3...)
                double barDouble = t / ppqPerBar;
                int bar = static_cast<int> (std::floor (barDouble)) + 1;
                g.setFont (ovt::fontRuler());
                g.drawText (juce::String (bar),
                            static_cast<int> (x) + 4, 0, 40, rulerH,
                            juce::Justification::centredLeft);
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.drawVerticalLine (static_cast<int> (x), rulerH - 2.0f, rulerH);

                // Beat subdivisions: "1.1", "1.2", "1.3"
                // We only label beats that are not bar starts and are integer positions
                double barDouble = t / ppqPerBar;
                int bar = static_cast<int> (std::floor (barDouble)) + 1;
                double beatInBar = (t - (bar - 1) * ppqPerBar) / beatUnit;
                int beatInt = static_cast<int> (std::round (beatInBar)) + 1;
                // Only label if this is an exact beat (not a subdivision from rounding)
                if (std::abs (beatInBar - (beatInt - 1)) < 0.01)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.5f));
                    g.setFont (ovt::fontOctaveLabel());
                    g.drawText (juce::String (bar) + "." + juce::String (beatInt),
                                static_cast<int> (x) + 2, rulerH - 10, 30, 12,
                                juce::Justification::centredLeft);
                }
            }
        }

        // === Ghost curve overlay (preset morphing target) ===
        if (hasGhostCurve && ghostCurve.getNumPoints() >= 2)
        {
            juce::Path ghostPath;
            const int N = 200;
            for (int i = 0; i <= N; ++i)
            {
                const double t = (timeVisible * i) / N;
                const float hz = ghostCurve.getPitchAt (t, 0.0f);
                if (hz <= 0.0f) continue;
                const float x = static_cast<float> (timeToX (t));
                const float y = pitchToY (hz);
                if (i == 0) ghostPath.startNewSubPath (x, y);
                else ghostPath.lineTo (x, y);
            }
            g.setColour (juce::Colour (0x30ffffff)); // 19% opacity white
            g.strokePath (ghostPath, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }

        // === Interpolated curve ===
        if (! pianoRollMode && curve.getNumPoints() >= 2)
        {
            juce::Path p;
            
            if (curve.isStepMode())
            {
                // Staircase drawing (Step mode)
                for (int i = 0; i < curve.getNumPoints(); ++i)
                {
                    const auto& pt = curve.getPoint (i);
                    const float x = static_cast<float> (timeToX (pt.time));
                    const float y = pitchToY (pt.pitch);
                    
                    if (i == 0)
                    {
                        p.startNewSubPath (static_cast<float> (timeToX (0.0)), y);
                        p.lineTo (x, y);
                    }
                    else
                    {
                        const auto& prev = curve.getPoint (i - 1);
                        const float prevY = pitchToY (prev.pitch);
                        p.lineTo (x, prevY);
                        p.lineTo (x, y);
                    }
                }
                
                // Extrapolate to the right
                const auto& last = curve.getPoint (curve.getNumPoints() - 1);
                p.lineTo (static_cast<float> (timeToX (timeVisible)), pitchToY (last.pitch));
            }
            else
            {
                // Linear drawing
                const int N = 200;
                for (int i = 0; i <= N; ++i)
                {
                    const double t = (timeVisible * i) / N;
                    const float hz = curve.getPitchAt (t, 0.0f);
                    if (hz <= 0.0f) continue;
                    const float x = static_cast<float> (timeToX (t));
                    const float y = pitchToY (hz);
                    if (i == 0) p.startNewSubPath (x, y);
                    else p.lineTo (x, y);
                }
            }
            
            g.setColour (kCurveColour.withAlpha (0.7f));
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }

        // Draw harmony traces (dashed blue lines) aligned with timeline.
        // Gated by showHarmoniesTrace so the Curve Editor hamburger menu
        // "Show Harmonies Trace" actually toggles these lines.
        if (showHarmoniesTrace)
        {
            juce::Colour harmonyCol = juce::Colour (0xff1A9AF0).withAlpha (0.8f);
            const float strokeW = 1.0f;
            const float dashes[] = { 6.0f, 4.0f };

            for (int v = 0; v < maxHarmonyVoices; ++v)
            {
                auto& times = harmonyTimes.getReference(v);
                auto& pitches = harmonyPitches.getReference(v);
                if (times.size() <= 1) continue;

                juce::Path hp;
                bool started = false;
                double previousTime = 0.0;
                bool hasPreviousTime = false;
                for (int i = 0; i < times.size(); ++i)
                {
                    double t = times[i];
                    float pHz = pitches[i];
                    // Break only on a transport rewind. Missing timer samples
                    // are handled by the UI-side hold in PluginEditor, just
                    // like the red input trace, so a sustained note remains a
                    // continuous line.
                    if (hasPreviousTime && t < previousTime)
                        started = false;
                    previousTime = t;
                    hasPreviousTime = true;
                    if (pHz <= 0.0f) { started = false; continue; }
                    float x = static_cast<float> (timeToX (t));
                    float y = pitchToY (pHz);
                    if (!started) { hp.startNewSubPath (x, y); started = true; }
                    else hp.lineTo (x, y);
                }
                if (!hp.isEmpty())
                {
                    g.setColour (harmonyCol);
                    g.strokePath (hp, juce::PathStrokeType (0.8f));
                }
            }
        }

        // Draw input pitch trace (red line, same as PitchVisualizer).
        // Hidden by default (showInputTrace) so the editable curve is clean on launch.
        if (showInputTrace && inputTraceTimes.size() > 1)
        {
            juce::Colour inputCol = juce::Colour (0xffe91e63).withAlpha (0.5f);
            juce::Path ip;
            bool started = false;
            for (int i = 0; i < inputTraceTimes.size(); ++i)
            {
                float hz = inputTracePitches[i];
                if (hz <= 0.0f) { started = false; continue; }
                float x = static_cast<float> (timeToX (inputTraceTimes[i]));
                float y = pitchToY (hz);
                if (!started) { ip.startNewSubPath (x, y); started = true; }
                else ip.lineTo (x, y);
            }
            if (!ip.isEmpty())
            {
                g.setColour (inputCol);
                g.strokePath (ip, juce::PathStrokeType (1.5f));
            }
        }

        if (isMarqueeSelecting)
        {
            juce::Rectangle<float> r = marqueeRect;
            r = r.getIntersection (plotArea.toFloat());
            if (! r.isEmpty())
            {
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.fillRect (r);
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.drawRect (r, 1.0f);
            }
        }

        // === Points (circles) and dynamic Tooltip ===
        int activePointIndex = (isDragging && dragIndex >= 0) ? dragIndex : hoverIndex;

        if (! pianoRollMode)
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const auto& pt = curve.getPoint (i);
            const float x = static_cast<float> (timeToX (pt.time));
            const float y = pitchToY (pt.pitch);
            
            // Highlight the hovered point or the one being dragged
            bool isActive = (i == activePointIndex);
            float radius = isActive ? 8.0f : 6.0f;
            const bool isSelected = selectedIndices.contains (i);
            
            g.setColour (kPointColour);
            g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (juce::Colours::white);
            g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, (isActive || isSelected) ? 2.0f : 1.0f);
            if (isSelected)
            {
                g.setColour (juce::Colours::white.withAlpha (0.4f));
                g.drawEllipse (x - radius - 2.0f, y - radius - 2.0f, (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f, 1.0f);
            }

            // Display the tooltip for the active point
            if (isActive)
            {
                juce::String noteStr = getNoteName(pt.pitch);
                
                // Format the time to match the ruler (Bar.Beat.Decimal)
                int measure = static_cast<int>(pt.time / 4.0) + 1;
                int beat = static_cast<int>(std::fmod(pt.time, 4.0)) + 1;
                int decimal = static_cast<int>(std::round(std::fmod(pt.time, 1.0) * 100.0));
                juce::String timeStr = juce::String(measure) + "." + juce::String(beat);
                if (decimal > 0 && decimal < 100)
                    timeStr += "." + juce::String(decimal).paddedLeft('0', 2);
                
                juce::String tooltipText = noteStr + " | " + timeStr;
                
                // Tooltip background
                int textW = 85;
                int textH = 20;
                juce::Rectangle<float> tooltipBounds (x - textW / 2.0f, y - radius - textH - 5.0f, static_cast<float>(textW), static_cast<float>(textH));
                
                // Keep the tooltip below the top edge
                if (tooltipBounds.getY() < 0) tooltipBounds.setY(y + radius + 5.0f);
                
                g.setColour (juce::Colours::black.withAlpha(0.8f));
                g.fillRoundedRectangle(tooltipBounds, 4.0f);
                
                g.setColour (juce::Colours::white);
                g.setFont (ovt::fontVersion());
                g.drawText (noteStr, tooltipBounds, juce::Justification::centred, false);
            }
        }

        // === Playhead (vertical playback line) ===
        double displayPlayhead = playheadTime;
        if (displayPlayhead >= scrollOffset && displayPlayhead <= scrollOffset + timeVisible)
        {
            const float x = static_cast<float> (timeToX (displayPlayhead));
            g.setColour (juce::Colours::red.withAlpha (0.8f));
            g.drawVerticalLine (static_cast<int> (x), rulerH, static_cast<float> (b.getHeight()));
        }

        // === Help label (bottom-right corner) ===
        g.setColour (juce::Colours::grey.withAlpha(0.6f));
        g.setFont (ovt::fontRuler());
        const juce::String modifierName =
#if JUCE_MAC
            "Cmd";
#else
            "Ctrl";
#endif
        g.drawText (ovt::tr(ovt::Keys::kHintScrollZoom) + modifierName + ovt::tr(ovt::Keys::kHintZoom),
                    b.getWidth() - 280, b.getHeight() - 36, 270, 14, juce::Justification::bottomRight);
        g.drawText (ovt::tr(ovt::Keys::kHintAddPoint),
                    b.getWidth() - 280, b.getHeight() - 20, 270, 14, juce::Justification::bottomRight);

        g.restoreState();

        // === Hover cursor (horizontal line + note/Hz readout) ===
        if (isMouseOverPlot && hoverMouseY >= (float) rulerH)
        {
            // Horizontal cursor line
            g.setColour (juce::Colour (0x44ffffff));
            g.drawHorizontalLine (static_cast<int> (hoverMouseY),
                                  static_cast<float> (pianoW), static_cast<float> (b.getWidth()));

            // Readout box with note name and Hz
            const float hz = yToPitch (hoverMouseY);
            if (hz > 0.0f)
            {
                const juce::String noteName = getNoteName (hz);
                const juce::String hzText = juce::String (static_cast<int> (std::round (hz))) + " Hz";
                const juce::String readout = noteName + "  " + hzText;
                g.setFont (ovt::fontReadout());
                // Round UP instead of truncating: a float-to-int cast can shave
                // a pixel off the measured width, which used to make drawText
                // fall back to "..." ellipsis on the Hz suffix. Add 2px of
                // safety margin to absorb any kerning / sub-pixel mismatch
                // between getStringWidth and the actual glyph layout.
                const int textW = static_cast<int> (std::ceil (
                    juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), readout))) + 2;
                const int boxW = textW + 14;
                const int boxH = 16;
                int boxX = b.getWidth() - boxW - 8;
                int boxY = static_cast<int> (hoverMouseY) - boxH - 4;
                if (boxY < rulerH) boxY = static_cast<int> (hoverMouseY) + 4;
                // Keep the whole readout inside the editor so the "Hz" suffix
                // is never clipped by the component's right edge.
                boxX = juce::jmin (boxX, b.getWidth() - boxW - 2);
                boxX = juce::jmax (boxX, pianoW + 2);
                g.setColour (juce::Colour (0xcc15151e));
                g.fillRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 3.0f);
                g.setColour (juce::Colour (0x661A9AF0));
                g.drawRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 3.0f, 0.5f);
                g.setColour (juce::Colours::white);
                g.drawText (readout, boxX + 7, boxY, boxW - 14, boxH,
                            juce::Justification::centredLeft);
            }
        }

        // Vertical separator between the piano and the editing area.
        g.setColour (ebs::isDark() ? juce::Colour (0xff2a2a36) : juce::Colour (0xff8a8a96));
        g.drawVerticalLine (pianoW, rulerH, static_cast<float> (b.getHeight()));

        // === Gray overlay when the editor is disabled (Live mode) ===
        if (!editorEnabled)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRect (plotArea);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (ebs::fontComboBox());
            g.drawText (ovt::tr(ovt::Keys::kHintLiveMode),
                        plotArea.getX(), plotArea.getY(), plotArea.getWidth(), plotArea.getHeight(),
                        juce::Justification::centred);
        }

        // === "FOLLOWS MIDI IN" badge (pulsing glow) ===
        if (midiFollowActive)
        {
            const float pulse = 0.5f + 0.5f * std::sin (juce::Time::getMillisecondCounter() * 0.006f);
            const juce::String midiTxt = ovt::tr (ovt::Keys::kLabelMidiFollowBadge);
            g.setFont (ovt::fontMeter0());
            const int textW = g.getCurrentFont().getStringWidth (midiTxt);
            const int padX = 10;
            const int w = textW + padX * 2;
            const int h = 20;
            const int x = plotArea.getRight() - w - 12;
            const int y = plotArea.getY() + 10;
            const juce::Colour base = juce::Colour (0xffff9800); // amber
            // Outer glow (pulsing)
            g.setColour (base.withAlpha (0.22f + 0.20f * pulse));
            g.fillRoundedRectangle ((float) (x - 4), (float) (y - 4),
                                    (float) (w + 8), (float) (h + 8), 10.0f);
            // Body
            g.setColour (base.withAlpha (0.9f));
            g.fillRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 6.0f);
            // Pulsing border
            g.setColour (juce::Colours::white.withAlpha (0.5f + 0.45f * pulse));
            g.drawRoundedRectangle ((float) x, (float) y, (float) w, (float) h, 6.0f, 1.0f);
            // Text (dark on amber for contrast)
            g.setColour (juce::Colours::black);
            g.drawText (midiTxt, x, y, w, h, juce::Justification::centred);
        }

        // === MIDI IN target line (dashed, amber) ===
        if (midiFollowActive && midiTargetHz > 0.0f)
        {
            const float ty = pitchToY (midiTargetHz);
            if (ty >= rulerH - 1.0f && ty <= (float) b.getHeight() + 1.0f)
            {
                const juce::Colour midiLineCol = juce::Colour (0xffff9800); // amber
                g.setColour (midiLineCol.withAlpha (0.9f));
                const float dashLen = 10.0f;
                const float gapLen = 5.0f;
                const float xStart = (float) pianoW;
                const float xEnd = (float) b.getWidth();
                float dx = 0.0f;
                while (xStart + dx < xEnd)
                {
                    const float x0 = xStart + dx;
                    const float x1 = juce::jmin (x0 + dashLen, xEnd);
                    g.drawLine (x0, ty, x1, ty, 1.6f);
                    dx += dashLen + gapLen;
                }
                // Note name label at the left edge of the plot
                const juce::String midiNote = ovtdsp::hzToNoteName (midiTargetHz);
                g.setFont (ovt::fontLegend());
                const int labelW = 46;
                const int labelH = 12;
                const int labelX = pianoW + 4;
                const int labelY = (int) ty - labelH - 1;
                if (labelY >= rulerH)
                {
                    g.setColour (juce::Colour (0xcc15151e));
                    g.fillRoundedRectangle ((float) labelX, (float) labelY,
                                            (float) labelW, (float) labelH, 3.0f);
                    g.setColour (midiLineCol);
                    g.drawText (midiNote, labelX, labelY, labelW, labelH,
                                juce::Justification::centred);
                }
            }
        }

        // Visual feedback for horizontal scrolling (middle button): tint + border.
        if (isMiddleScrolling)
        {
            g.setColour (juce::Colours::yellow.withAlpha (0.12f));
            g.fillRect (plotArea);
            g.setColour (juce::Colours::yellow.withAlpha (0.55f));
            g.drawRect (plotArea.toFloat(), 2.0f);
        }
    }

    void PitchCurveEditor::resized()
    {
        // The piano keyboard takes the vertical strip on the left (60 px wide).
        const int pianoW = 60;
        const int rulerH = 24;
        pianoKeyboard.setBounds (0, rulerH, pianoW, getHeight() - rulerH);

        // Undo/Redo buttons (bottom-left, below piano keyboard)
        const int btnSize = 22;
        const int btnGap = 4;
        const int btnY = pianoKeyboard.getBottom() + 4;
        undoButton.setBounds (2, btnY, btnSize, btnSize);
        redoButton.setBounds (2 + btnSize + btnGap, btnY, btnSize, btnSize);

        // Piano Roll toggle: top-right of the plot area.
        pianoRollButton.setBounds (getWidth() - 110, 2, 104, 20);
    }

    void PitchCurveEditor::timerCallback()
    {
        repaint();
    }

    void PitchCurveEditor::addHarmonySamples (double time, const juce::Array<float>& freqs)
    {
        // Push samples per voice and trim old entries outside the visible window
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            float pitch = 0.0f;
            if (v < freqs.size()) pitch = freqs[v];

            auto& times = harmonyTimes.getReference(v);
            auto& pitches = harmonyPitches.getReference(v);

            times.add (time);
            pitches.add (pitch);

            // Trim old samples (keep samples within [playheadTime - timeVisible, playheadTime])
            while (times.size() > 0 && times[0] < (playheadTime - timeVisible))
            {
                times.remove(0);
                pitches.remove(0);
            }
        }
    }

    void PitchCurveEditor::addInputTraceSample (double time, float hz)
    {
        inputTraceTimes.add (time);
        inputTracePitches.add (hz);
        // Trim old samples outside the visible window
        while (inputTraceTimes.size() > 0 && inputTraceTimes[0] < (playheadTime - timeVisible))
        {
            inputTraceTimes.remove (0);
            inputTracePitches.remove (0);
        }
    }

    void PitchCurveEditor::clearInputTrace()
    {
        inputTraceTimes.clear();
        inputTracePitches.clear();
    }

    // === Coordinate conversions ===
    // Note: the X axis covers only the curve editing area
    // (to the right of the piano keyboard). We subtract the piano width.
    double PitchCurveEditor::timeToX (double t) const
    {
        const int pianoW = pianoKeyboard.getWidth();
        const int plotW  = getWidth() - pianoW;
        const double viewT = t - scrollOffset;
        return pianoW + (viewT / timeVisible) * plotW;
    }
    double PitchCurveEditor::xToTime (float x) const
    {
        const int pianoW = pianoKeyboard.getWidth();
        const int plotW  = juce::jmax (1, getWidth() - pianoW);
        const double viewT = ((x - pianoW) / plotW) * timeVisible;
        return juce::jlimit (scrollOffset, scrollOffset + timeVisible,
                             viewT + scrollOffset);
    }
    float PitchCurveEditor::pitchToY (float p) const
    {
        const int rulerH = 24;
        if (pianoRollMode)
        {
            // Piano geometry (same as the vertical keyboard): notes
            // align on the piano keys and have a constant height,
            // regardless of position or zoom.
            const int lo = pianoKeyboard.getLowestMidi();
            const int hi = pianoKeyboard.getHighestMidi();
            const int midi = static_cast<int> (std::round (ovtdsp::hzToMidiFloat (p)));
            const float t = PianoKeyboard::midiToNorm (midi, lo, hi);
            return static_cast<float> (rulerH) + (getHeight() - rulerH) * (1.0f - t);
        }
        // Log scale (curve mode). NO clamping: a pitch outside the visible
    // window is projected outside the plotting area and thus clipped,
    // instead of being stacked at the top/bottom.
    const float lh = std::log (juce::jmax (p, 1.0f));
    const float lmin = std::log (minHz);
    const float lmax = std::log (maxHz);
    const float t = (lh - lmin) / (lmax - lmin);
    return rulerH + (getHeight() - rulerH) * (1.0f - t);
    }
    float PitchCurveEditor::yToPitch (float y) const
    {
        const int rulerH = 24;
        if (pianoRollMode)
        {
            // Inverse of the piano geometry: find the note whose
            // normalized position is closest to the requested Y.
            const int lo = pianoKeyboard.getLowestMidi();
            const int hi = pianoKeyboard.getHighestMidi();
            const float t = 1.0f - juce::jlimit (0.0f, 1.0f, (y - rulerH) / (getHeight() - rulerH));
            int best = lo;
            float bestD = 1.0e9f;
            for (int m = lo; m <= hi; ++m)
            {
                const float d = std::abs (PianoKeyboard::midiToNorm (m, lo, hi) - t);
                if (d < bestD) { bestD = d; best = m; }
            }
            return ovtdsp::midiToHz (static_cast<float> (best));
        }
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, (y - rulerH) / (getHeight() - rulerH));
        const float lmin = std::log (minHz);
        const float lmax = std::log (maxHz);
        return std::exp (lmin + t * (lmax - lmin));
    }

    float PitchCurveEditor::snapToNearestNote (float hz) const
    {
        if (hz <= 0.0f) return hz;
        const float midi = ovtdsp::hzToMidiFloat (hz);
        return ovtdsp::midiToHz (std::round (midi));
    }

    void PitchCurveEditor::drawPianoRoll (juce::Graphics& g)
    {
        const int pianoW = pianoKeyboard.getWidth();
        const int rulerH = 24;
        const int plotRight = getWidth();

        // Per-note horizontal rows, aligned with the left keyboard (same range and
        // same piano geometry: constant heights).
        const int lowestMidi = pianoKeyboard.getLowestMidi();
        const int highestMidi = pianoKeyboard.getHighestMidi();
        // Horizontal rows match the Curves-mode grid exactly: C notes use the
        // plain curve grid, in-scale notes use the scale line, and off-scale
        // notes draw no line. This keeps the piano-roll brightness/content
        // identical to the Curves grid (no faint, off-scale rows).
        for (int midi = lowestMidi; midi <= highestMidi; ++midi)
        {
            const int noteInOct = ovtdsp::midiToNoteInOctave (midi);
            const float y = pitchToY (ovtdsp::midiToHz (static_cast<float> (midi)));
            if (noteInOct == 0)
                g.setColour (ebs::curveGrid());
            else if (scaleIntervals.contains (noteInOct))
                g.setColour (ebs::scaleLine());
            else
                continue;   // off-scale: no line, matching Curves mode
            g.drawHorizontalLine (static_cast<int> (y),
                                  static_cast<float> (pianoW),
                                  static_cast<float> (plotRight));
        }

        // Note blocks: one per curve segment; the last one extends to the view end.
        const int n = curve.getNumPoints();
        if (n >= 1)
        {
            // Constant height: one white key height (identical for
            // all notes), aligned with the vertical keyboard.
            int numWhite = 0;
            for (int m = lowestMidi; m <= highestMidi; ++m)
                if (! PianoKeyboard::isBlackKey (m)) ++numWhite;
            const float plotH = static_cast<float> (getHeight() - rulerH);
            const float rowH = (numWhite > 0)
                ? juce::jmax (4.0f, plotH / static_cast<float> (numWhite))
                : 8.0f;
            for (int i = 0; i < n; ++i)
            {
                const auto& pt = curve.getPoint (i);
                const double startT = pt.time;
                const double endT = (i + 1 < n) ? curve.getPoint (i + 1).time : timeVisible;
                if (endT <= startT) continue;

                const int pitchMidi = static_cast<int> (std::round (ovtdsp::hzToMidiFloat (pt.pitch)));
                const float yCenter = pitchToY (ovtdsp::midiToHz (static_cast<float> (pitchMidi)));
                const float blockTop = yCenter - rowH / 2.0f + 1.0f;
                const float blockH = juce::jmax (3.0f, rowH - 2.0f);

                const float x1 = static_cast<float> (timeToX (startT));
                const float x2 = static_cast<float> (timeToX (endT));
                const float blockW = juce::jmax (2.0f, x2 - x1);

                const int noteInOct = ovtdsp::midiToNoteInOctave (pitchMidi);
                const bool inScale = scaleIntervals.contains (noteInOct);
                const juce::Colour fill = inScale ? kCurveColour.withAlpha (0.55f)
                                                  : juce::Colour (0x553322aa);

                g.setColour (fill);
                g.fillRoundedRectangle (x1, blockTop, blockW, blockH, 3.0f);
                g.setColour (inScale ? kCurveColour : juce::Colour (0xff6655cc));
                g.drawRoundedRectangle (x1, blockTop, blockW, blockH, 3.0f, 1.0f);

                const juce::String label = getNoteName (pt.pitch);
                if (blockW > 26.0f)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.9f));
                    g.setFont (ovt::fontOctaveLabel());
                    g.drawText (label, static_cast<int> (x1) + 3, static_cast<int> (blockTop),
                                static_cast<int> (blockW) - 6, static_cast<int> (blockH),
                                juce::Justification::centredLeft, false);
                }
            }
        }
    }

    juce::String PitchCurveEditor::getNoteName (float hz) const
    {
        if (hz <= 0.0f) return "";
        // Formula: midi = 69 + 12 * log2(f / 440)
        int midiNote = static_cast<int>(std::round(69.0f + 12.0f * std::log2(hz / 440.0f)));
        const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int octave = (midiNote / 12) - 1;
        int noteIndex = midiNote % 12;
        if (noteIndex < 0) noteIndex += 12; // Safety
        return juce::String(noteNames[noteIndex]) + juce::String(octave);
    }

    int PitchCurveEditor::findPointAtPixel (juce::Point<float> p, float maxDist) const
    {
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const auto& pt = curve.getPoint (i);
            const float dx = p.x - static_cast<float> (timeToX (pt.time));
            const float dy = p.y - pitchToY (pt.pitch);
            if (dx * dx + dy * dy <= maxDist * maxDist)
                return i;
        }
        return -1;
    }

    // === Mouse input ===
    void PitchCurveEditor::mouseDown (const juce::MouseEvent& e)
    {
        grabKeyboardFocus(); // for keyboard shortcuts

        // Horizontal scrolling with the mouse (middle button), only if
        // auto-scroll is disabled (avoids any conflict with auto-follow)
        // AND the playhead is not looping over the Measures window (the view
        // is then locked to the loop window).
        // Works in either editing mode (Curve or Live).
        if (e.mods.isMiddleButtonDown())
        {
            if (!autoScrollEnabled && !loopingPlayhead)
            {
                isMiddleScrolling = true;
                middleDragStartX = e.position.x;
                middleDragStartScroll = scrollOffset;
                setMouseCursor (juce::MouseCursor::DraggingHandCursor);
                repaint();
            }
            return;
        }

        // Left click in the ruler: instant playhead move
        // (snapped to the project grid). Transport action (not
        // editing): available in Curve and Live modes.
        if (e.mods.isLeftButtonDown() && e.position.y <= 24)
        {
            double t = juce::jmax (0.0, xToTime (e.position.x));
            t = snapTimeToGrid (t, snapToGridEnabled);
            if (onSeek) onSeek (t);
            setPlayheadTime (t, false); // immediate visual feedback (+ reveal if off screen)
            repaint();
            return;
        }

        // If the editor is disabled (Auto/Live mode), skip editing.
        if (!editorEnabled) return;

        // Snapshot for undo
        pendingUndoSnapshot = curve;

        const juce::Point<float> p (e.position.x, e.position.y);
        dragIndex = findPointAtPixel (p);
        isDragging = false;
        isDraggingSelection = false;
        isMarqueeSelecting = false;

        // Piano-roll metaphor: a left click on empty space creates a new note
        // (snapped to the keyboard row) and starts dragging it. Marquee (box)
        // selection is disabled in this mode.
        if (pianoRollMode && dragIndex < 0 && e.mods.isLeftButtonDown())
        {
            double t = xToTime (e.position.x);
            const double gridStep = 0.5;
            const double nearestGrid = std::round (t / gridStep) * gridStep;
            if (snapToGridEnabled) t = nearestGrid;
            else if (std::abs (t - nearestGrid) < 0.05) t = nearestGrid;
            float hz = snapToNearestNote (yToPitch (e.position.y));
            if (snapEnabled) hz = ovtdsp::PitchCurve::snapToIntervals (hz, scaleIntervals);
            curve.addOrUpdatePoint (t, hz);
            dragIndex = findPointAtPixel (p);
            isDragging = true;
            selectedIndices.clear();
            if (dragIndex >= 0) selectedIndices.add (dragIndex);
            repaint();
            return;
        }

        if (dragIndex >= 0)
        {
            // Drag start: check whether we should delete (right click or Alt+click).
            if (e.mods.isRightButtonDown() || e.mods.isAltDown())
            {
                curve.getPoint (dragIndex); // valid index
                curve.removePointNear (curve.getPoint (dragIndex).time);
                dragIndex = -1;
                selectedIndices.clear();
                notifyChanged();
                return;
            }

            if (! selectedIndices.contains (dragIndex))
            {
                selectedIndices.clear();
                selectedIndices.add (dragIndex);
            }

            if (selectedIndices.size() > 1)
            {
                selectionStartTimes.clear();
                selectionStartPitches.clear();
                selectionStartTimes.ensureStorageAllocated (selectedIndices.size());
                selectionStartPitches.ensureStorageAllocated (selectedIndices.size());

                for (int idx : selectedIndices)
                {
                    const auto& pt = curve.getPoint (idx);
                    selectionStartTimes.add (pt.time);
                    selectionStartPitches.add (pt.pitch);
                }

                selectionAnchorLocal = selectedIndices.indexOf (dragIndex);
                isDraggingSelection = selectionAnchorLocal >= 0;
            }
            else
            {
                isDragging = true;
            }
        }
        else
        {
            if (e.mods.isRightButtonDown())
            {
                if (onRightClick != nullptr)
                    onRightClick (e);
                return;
            }

            selectedIndices.clear();
            marqueeStart = e.position;
            marqueeRect = juce::Rectangle<float> (marqueeStart, marqueeStart);
            isMarqueeSelecting = true;
        }
    }

    void PitchCurveEditor::mouseDrag (const juce::MouseEvent& e)
    {
        // Horizontal scrolling (middle button): natural "grab and drag" movement.
        if (isMiddleScrolling)
        {
            const int pianoW = pianoKeyboard.getWidth();
            const double plotW = static_cast<double> (juce::jmax (1, getWidth() - pianoW));
            const double pxPerBeat = plotW / timeVisible;
            const double dxBeats = (e.position.x - middleDragStartX) / (pxPerBeat > 0.0 ? pxPerBeat : 1.0);
            scrollOffset = clampScrollOffset (middleDragStartScroll - dxBeats);
            repaint();
            return;
        }

        if (!editorEnabled) return;
        if (isMarqueeSelecting)
        {
            marqueeRect = juce::Rectangle<float> (marqueeStart, e.position).getSmallestIntegerContainer().toFloat();
            selectedIndices.clear();

            const int pianoW = pianoKeyboard.getWidth();
            const int rulerH = 24;
            const auto plotArea = juce::Rectangle<float> ((float) pianoW, (float) rulerH,
                                                          (float) (getWidth() - pianoW),
                                                          (float) (getHeight() - rulerH));
            const auto r = marqueeRect.getIntersection (plotArea);

            for (int i = 0; i < curve.getNumPoints(); ++i)
            {
                const auto& pt = curve.getPoint (i);
                const float x = static_cast<float> (timeToX (pt.time));
                const float y = pitchToY (pt.pitch);
                if (r.contains (x, y))
                    selectedIndices.add (i);
            }

            repaint();
            return;
        }

        if (isDraggingSelection && selectionAnchorLocal >= 0 && selectedIndices.size() >= 2)
        {
            const double anchorStartTime = selectionStartTimes.getUnchecked (selectionAnchorLocal);
            const float anchorStartHz = selectionStartPitches.getUnchecked (selectionAnchorLocal);

            double t = xToTime (e.position.x);
            float hz = yToPitch (e.position.y);

            double gridStep = 0.5;
            double nearestGrid = std::round (t / gridStep) * gridStep;
            if (snapToGridEnabled)
            {
                t = nearestGrid;
            }
            else if (std::abs (t - nearestGrid) < 0.05)
            {
                t = nearestGrid;
            }

            if (snapEnabled)
            {
                hz = ovtdsp::PitchCurve::snapToIntervals (hz, scaleIntervals);
            }
            else
            {
                float snappedHz = ovtdsp::PitchCurve::snapToIntervals (hz, getChromaticIntervals());
                float centsDiff = 1200.0f * std::log2 (hz / snappedHz);
                if (std::abs (centsDiff) < 15.0f)
                    hz = snappedHz;
            }

            double deltaT = t - anchorStartTime;
            double minT = selectionStartTimes.getUnchecked (0);
            double maxT = selectionStartTimes.getUnchecked (0);
            float minHzSel = selectionStartPitches.getUnchecked (0);
            float maxHzSel = selectionStartPitches.getUnchecked (0);
            for (int i = 1; i < selectionStartTimes.size(); ++i)
            {
                minT = juce::jmin (minT, selectionStartTimes.getUnchecked (i));
                maxT = juce::jmax (maxT, selectionStartTimes.getUnchecked (i));
                minHzSel = juce::jmin (minHzSel, selectionStartPitches.getUnchecked (i));
                maxHzSel = juce::jmax (maxHzSel, selectionStartPitches.getUnchecked (i));
            }

            deltaT = juce::jlimit (-minT, timeVisible - maxT, deltaT);

            float ratio = 1.0f;
            if (anchorStartHz > 0.0f && hz > 0.0f)
                ratio = hz / anchorStartHz;

            if (minHzSel > 0.0f && maxHzSel > 0.0f)
            {
                const float minRatio = minHz / minHzSel;
                const float maxRatio = maxHz / maxHzSel;
                ratio = juce::jlimit (minRatio, maxRatio, ratio);
            }

            juce::Array<double> newTimes;
            juce::Array<float> newPitches;
            newTimes.ensureStorageAllocated (selectedIndices.size());
            newPitches.ensureStorageAllocated (selectedIndices.size());

            for (int i = 0; i < selectedIndices.size(); ++i)
            {
                newTimes.add (selectionStartTimes.getUnchecked (i) + deltaT);
                newPitches.add (selectionStartPitches.getUnchecked (i) * ratio);
            }

            curve.setMultiplePointsTimeAndPitch (selectedIndices, newTimes, newPitches);
            dragIndex = selectedIndices.getUnchecked (selectionAnchorLocal);
            repaint();
            return;
        }

        if (!isDragging || dragIndex < 0) return;

        // Piano-roll metaphor: the pitch is snapped to the nearest keyboard row
        // (and to the scale if snapping is enabled). Time snapping is unchanged.
        if (pianoRollMode)
        {
            double t = xToTime (e.position.x);
            const double gridStep = 0.5;
            const double nearestGrid = std::round (t / gridStep) * gridStep;
            if (snapToGridEnabled) t = nearestGrid;
            else if (std::abs (t - nearestGrid) < 0.05) t = nearestGrid;
            float hz = snapToNearestNote (yToPitch (e.position.y));
            if (snapEnabled) hz = ovtdsp::PitchCurve::snapToIntervals (hz, scaleIntervals);
            curve.setPointTimeAndPitch (dragIndex, t, hz);
            repaint();
            return;
        }

        // Update the time and pitch.
        double t = xToTime (e.position.x);
        float hz = yToPitch (e.position.y);
        
        // Time snap to grid (0.5s by default = eighth note)
        double gridStep = 0.5;
        double nearestGrid = std::round(t / gridStep) * gridStep;
        if (snapToGridEnabled)
        {
            t = nearestGrid;
        }
        else if (std::abs(t - nearestGrid) < 0.05)
        {
            // Light magnetism at +/- 0.05s when strict snapping is off
            t = nearestGrid;
        }
        
        // Snap to note / Snap to scale
        if (snapEnabled)
        {
            hz = ovtdsp::PitchCurve::snapToIntervals (hz, scaleIntervals);
        }
        else
        {
            // Snap to note (implicit chromatic via magnetism)
            // The user wanted "snap-to-note on pitch change" even without a scale.
            float snappedHz = ovtdsp::PitchCurve::snapToIntervals (hz, getChromaticIntervals());
            // Magnetism when close to the exact note (e.g. less than 15 cents away)
            float centsDiff = 1200.0f * std::log2(hz / snappedHz);
            if (std::abs(centsDiff) < 15.0f) {
                hz = snappedHz;
            }
        }
        
        // Direct write via setPointTimeAndPitch to keep sorting and the drag index
        curve.setPointTimeAndPitch (dragIndex, t, hz);
        repaint();
    }

    void PitchCurveEditor::mouseUp (const juce::MouseEvent& /*e*/)
    {
        // End of horizontal scrolling (middle button).
        if (isMiddleScrolling)
        {
            isMiddleScrolling = false;
            setMouseCursor (juce::MouseCursor::NormalCursor);
            repaint();
            return;
        }

        // Check whether a modification occurred
        bool wasModified = isDragging || isDraggingSelection;
        bool hadPoints = (pendingUndoSnapshot.getNumPoints() > 0 || curve.getNumPoints() > 0);

        if (isMarqueeSelecting)
        {
            isMarqueeSelecting = false;
            marqueeRect = {};
            repaint();
            return;
        }

        if (isDraggingSelection)
        {
            isDraggingSelection = false;
            notifyChanged();
        }

        if (isDragging)
        {
            isDragging = false;
            notifyChanged();
        }

        // Register the undo if a change occurred
        if (wasModified && hadPoints)
        {
            // Compare the snapshots (avoids an empty undo when the drag changed nothing)
            // Uses a simple point-count comparison + serialization
            // as a fast heuristic to avoid empty undos
            auto beforeXml = pendingUndoSnapshot.toXml();
            auto afterXml = curve.toXml();
            bool changed = (beforeXml->toString() != afterXml->toString());
            if (changed)
            {
                auto* action = new CurveEditAction (this);
                action->before = pendingUndoSnapshot;
                action->after = curve;
                registerUndoableAction (action);
            }
        }
    }

    void PitchCurveEditor::mouseMove (const juce::MouseEvent& e)
    {
        const int rulerH = 24;
        const int pianoW = pianoKeyboard.getWidth() > 0 ? pianoKeyboard.getWidth() : 60;
        const auto plotArea = juce::Rectangle<int> (pianoW, rulerH,
                                                     getWidth() - pianoW, getHeight() - rulerH);

        auto pos = e.getPosition();
        if (plotArea.contains (pos))
        {
            isMouseOverPlot = true;
            hoverMouseX = static_cast<float> (pos.x);
            hoverMouseY = static_cast<float> (pos.y);
        }
        else
        {
            isMouseOverPlot = false;
        }

        if (! editorEnabled) return;

        const juce::Point<float> p (e.position.x, e.position.y);
        int newHover = findPointAtPixel (p);
        
        if (newHover != hoverIndex)
        {
            hoverIndex = newHover;
            repaint();
        }
    }

    void PitchCurveEditor::mouseExit (const juce::MouseEvent&)
    {
        isMouseOverPlot = false;
        repaint();
    }

    void PitchCurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
    {
        // If the editor is disabled (Auto mode), ignore.
        if (!editorEnabled) return;

        // Add a point at the cursor position.
        double t = xToTime (e.position.x);
        float hz = yToPitch (e.position.y);
        if (pianoRollMode) hz = snapToNearestNote (hz); // keyboard-row snap
        
        // Time snap to grid
        double gridStep = 0.5;
        double nearestGrid = std::round(t / gridStep) * gridStep;
        if (snapToGridEnabled)
        {
            t = nearestGrid;
        }
        else if (std::abs(t - nearestGrid) < 0.05)
        {
            t = nearestGrid;
        }
        
        if (snapEnabled)
        {
            hz = ovtdsp::PitchCurve::snapToIntervals (hz, scaleIntervals);
        }
        curve.addOrUpdatePoint (t, hz);
        notifyChanged();
    }

    void PitchCurveEditor::applyZoom (float anchorPitch, float factor)
    {
        // factor > 1 => zoom in (narrower pitch range), centered on
        // anchorPitch (the pitch under the cursor / finger). Clamped to 1..8 octaves.
        if (anchorPitch <= 0.0f) anchorPitch = std::sqrt (minHz * maxHz);
        float rangeCents = 1200.0f * std::log2 (maxHz / minHz) / factor;
        if (rangeCents < 1200.0f)           rangeCents = 1200.0f;
        if (rangeCents > 1200.0f * 8.0f)    rangeCents = 1200.0f * 8.0f;
        const float halfRangeLog = (rangeCents / 1200.0f) * std::log (2.0f) / 2.0f;
        const float centerLog = std::log (anchorPitch);
        minHz = std::exp (centerLog - halfRangeLog);
        maxHz = std::exp (centerLog + halfRangeLog);
        clampPitchRange();
    }

    void PitchCurveEditor::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
    {
        // macOS trackpad pinch: scaleFactor > 1 => pinch out => zoom in.
        // Zoom around the pitch under the finger (or the view center).
        const float anchor = (e.position.y >= 0 && e.position.y <= getHeight())
                                 ? yToPitch (e.position.y)
                                 : std::sqrt (minHz * maxHz);
        applyZoom (anchor, scaleFactor);
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (minHz)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (maxHz)));
        repaint();
    }

    void PitchCurveEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        // Trackpad pinch-to-zoom is delivered on macOS as a wheel event with the
        // Ctrl/Cmd modifier (and isSmooth == true); desktop Ctrl/Cmd + wheel also
        // lands here. Zoom is centred on the cursor's pitch (or the view centre).
        const bool pinchZoom = e.mods.isCtrlDown() || e.mods.isCommandDown();
        if (pinchZoom)
        {
            const float anchor = (e.position.y >= 0 && e.position.y <= getHeight())
                                     ? yToPitch (e.position.y)
                                     : std::sqrt (minHz * maxHz);
            // Multiplicative zoom, same direction as the Live visualizer: wheel up
            // (deltaY > 0) => zoom in (narrower pitch window).
            const float factor = juce::jlimit (0.1f, 10.0f, std::exp (wheel.deltaY * 4.0f));
            applyZoom (anchor, factor);
        }
        else
        {
            // Two-finger vertical scroll pans the pitch axis. Trackpads also feed
            // wheel.deltaX for two-finger horizontal swipes -> pan the time window
            // (same constraints as the middle-button drag: disabled while the
            // view auto-follows the playhead or is locked to the loop window).
            const float rangeLog = std::log (maxHz / minHz);
            const float pitchShift = wheel.deltaY * rangeLog * 0.5f;
            minHz = std::exp (std::log (minHz) + pitchShift);
            maxHz = std::exp (std::log (maxHz) + pitchShift);

            if (!autoScrollEnabled && !loopingPlayhead)
            {
                const float timeShift = wheel.deltaX * timeVisible * 0.5f;
                scrollOffset = clampScrollOffset (scrollOffset + timeShift);
            }
        }

        // Absolute limits (C0..C9).
        if (minHz < 16.35f) { const float r = maxHz / minHz; minHz = 16.35f; maxHz = minHz * r; }
        if (maxHz > 8372.0f) { const float r = maxHz / minHz; maxHz = 8372.0f; minHz = maxHz / r; }

        // Update the piano
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (minHz)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (maxHz)));
        repaint();
    }

    // === Public API ===
    void PitchCurveEditor::setCurve (const ovtdsp::PitchCurve& newCurve)
    {
        curve = newCurve; // copy (includes stepMode, snapEnabled, snapToGridEnabled)
        // In Piano Roll mode we always force Step Mode (see setPianoRollMode):
        // a preset may carry stepMode=false, but the keyboard-row layout must stay
        // discrete, otherwise the audio would glide between notes and disagree with
        // the displayed rows.
        if (pianoRollMode)
            curve.setStepMode (true);
        // Apply the editing options stored in the curve
        snapEnabled = curve.isSnapEnabled();
        snapToGridEnabled = curve.isSnapToGridEnabled();
        repaint();
        notifyChanged();
    }

    void PitchCurveEditor::importMidiCurve (const ovtdsp::PitchCurve& newCurve)
    {
        if (newCurve.getNumPoints() < 2)
            return;

        // Register undo state before modifying
        CurveEditAction* action = new CurveEditAction (this);

        curve = newCurve;

        // MIDI notes are discrete: force step mode
        curve.setStepMode (true);

        // Reset editing options for clean post-import state
        curve.setSnapEnabled (true);
        curve.setSnapToGridEnabled (true);
        snapEnabled = true;
        snapToGridEnabled = true;

        // Record the "after" state for undo
        action->setAfter();
        registerUndoableAction (action);

        // NOTE: the user's "Measures" setting is intentionally NOT modified
        // on import (the number of visible measures stays as configured).

        repaint();
        notifyChanged();
    }

    void PitchCurveEditor::resetEditState()
    {
        snapEnabled = true;
        snapToGridEnabled = true;
        curve.setStepMode (true);
    }

    void PitchCurveEditor::capturePitch (float hz, double currentTime)
    {
        if (hz <= 0.0f) return;
        if (snapEnabled)
        {
            hz = ovtdsp::PitchCurve::snapToIntervals (hz, scaleIntervals);
        }
        curve.addOrUpdatePoint (currentTime, hz);
        notifyChanged();
    }

    void PitchCurveEditor::setViewRange (double secVis, float minH, float maxH)
    {
        timeVisible = secVis;
        minHz = minH;
        maxHz = maxH;
        pianoKeyboard.setRange(static_cast<int>(ovtdsp::hzToMidiFloat(minHz)), 
                               static_cast<int>(ovtdsp::hzToMidiFloat(maxHz)));
        repaint();
    }

    void PitchCurveEditor::clampPitchRange()
    {
        // Keep the zoom within 1..8 octaves (mirrors mouseWheelMove limits).
        const float rangeCents = 1200.0f * std::log2 (maxHz / minHz);
        if (rangeCents < 1200.0f)
        {
            const float centerLog = 0.5f * (std::log (minHz) + std::log (maxHz));
            const float half = 0.5f * std::log (2.0f); // 1 octave half-range
            minHz = std::exp (centerLog - half);
            maxHz = std::exp (centerLog + half);
        }
        else if (rangeCents > 1200.0f * 8.0f)
        {
            const float centerLog = 0.5f * (std::log (minHz) + std::log (maxHz));
            const float half = 0.5f * 8.0f * std::log (2.0f); // 8 octaves half-range
            minHz = std::exp (centerLog - half);
            maxHz = std::exp (centerLog + half);
        }
        // Absolute limits (C0..C9).
        if (minHz < 16.35f) { const float r = maxHz / minHz; minHz = 16.35f; maxHz = minHz * r; }
        if (maxHz > 8372.0f) { const float r = maxHz / minHz; maxHz = 8372.0f; minHz = maxHz / r; }
    }

    void PitchCurveEditor::zoomIn()
    {
        const float centerLog = 0.5f * (std::log (minHz) + std::log (maxHz));
        const float half = 0.5f * std::log (maxHz / minHz) * 0.8f; // 20% narrower
        minHz = std::exp (centerLog - half);
        maxHz = std::exp (centerLog + half);
        clampPitchRange();
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (minHz)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (maxHz)));
        repaint();
    }

    void PitchCurveEditor::zoomOut()
    {
        const float centerLog = 0.5f * (std::log (minHz) + std::log (maxHz));
        const float half = 0.5f * std::log (maxHz / minHz) * 1.25f; // 25% wider
        minHz = std::exp (centerLog - half);
        maxHz = std::exp (centerLog + half);
        clampPitchRange();
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (minHz)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (maxHz)));
        repaint();
    }

    void PitchCurveEditor::scrollUp()
    {
        const float rangeLog = std::log (maxHz / minHz);
        const float shiftLog = rangeLog * 0.15f;
        minHz = std::exp (std::log (minHz) + shiftLog);
        maxHz = std::exp (std::log (maxHz) + shiftLog);
        clampPitchRange();
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (minHz)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (maxHz)));
        repaint();
    }

    void PitchCurveEditor::scrollDown()
    {
        const float rangeLog = std::log (maxHz / minHz);
        const float shiftLog = -rangeLog * 0.15f;
        minHz = std::exp (std::log (minHz) + shiftLog);
        maxHz = std::exp (std::log (maxHz) + shiftLog);
        clampPitchRange();
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (minHz)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (maxHz)));
        repaint();
    }

    void PitchCurveEditor::resetView()
    {
        minHz = 50.0f;
        maxHz = 1000.0f;
        scrollOffset = 0.0;
        pianoKeyboard.setRange (static_cast<int> (ovtdsp::hzToMidiFloat (minHz)),
                                static_cast<int> (ovtdsp::hzToMidiFloat (maxHz)));
        repaint();
    }

    bool PitchCurveEditor::exportAsImage (const juce::File& filePath)
    {
        // Render the editor to a high quality (2x) image, identical to the
        // Live visualizer (PitchVisualizer::exportAsImage).
        const int scale = 2;
        const int w = getWidth() * scale;
        const int h = getHeight() * scale;
        if (w <= 0 || h <= 0) return false;

        // Opaque image (no alpha) to avoid transparency issues.
        juce::Image image (juce::Image::RGB, w, h, true);
        {
            juce::Graphics g (image);
            g.addTransform (juce::AffineTransform::scale ((float) scale));
            paint (g);
        }

        juce::PNGImageFormat png;
        juce::FileOutputStream stream (filePath);
        if (stream.failedToOpen()) return false;
        return png.writeImageToStream (image, stream);
    }

    void PitchCurveEditor::setSnapEnabled (bool b)
    {
        snapEnabled = b;
    }

    void PitchCurveEditor::setSnapToGridEnabled (bool b)
    {
        snapToGridEnabled = b;
    }

    void PitchCurveEditor::setStepModeEnabled (bool b)
    {
        if (curve.isStepMode() != b)
        {
            curve.setStepMode (b);
            notifyChanged();
            repaint();
        }
    }

    void PitchCurveEditor::refreshTranslations()
    {
        undoButton.setTooltip (ovt::tr(ovt::Keys::kTooltipUndo));
        redoButton.setTooltip (ovt::tr(ovt::Keys::kTooltipRedo));
        pianoRollButton.setTooltip (ovt::tr(ovt::Keys::kTooltipPianoRoll));
        pianoRollButton.setButtonText (ovt::tr(ovt::Keys::kButtonPianoRoll));
        repaint();
    }

    void PitchCurveEditor::clearHarmonyTraces()
    {
        // Clear all harmony time/pitch buffers for every voice
        harmonyTimes.clear();
        harmonyPitches.clear();
        // Reinitialize with empty arrays (maxHarmonyVoices = 8)
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            harmonyTimes.add(juce::Array<double>());
            harmonyPitches.add(juce::Array<float>());
        }
        repaint(); // Force redraw without harmony traces
    }

    void PitchCurveEditor::setEditorEnabled (bool b)
    {
        editorEnabled = b;
        // Cancel any ongoing drag when disabling.
        if (!b) isDragging = false;
        repaint();
    }

    void PitchCurveEditor::setKeyAndScale (int key, ovtdsp::Scale scale)
    {
        keyIdx = key;
        currentScale = scale;
        // Re-snap all points when snapping is active.
        if (snapEnabled)
        {
            // Snap against the authoritative interval set (same as the on-screen
            // display) so re-snapping always matches the visible scale.
            for (int i = 0; i < curve.getNumPoints(); ++i)
            {
                const float hz = curve.getPoint (i).pitch;
                curve.getPoint (i).pitch =
                    ovtdsp::PitchCurve::snapToIntervals (hz, scaleIntervals);
            }
            notifyChanged();
        }
        repaint();
    }

    void PitchCurveEditor::setScaleIntervals (const juce::Array<int>& intervals)
    {
        scaleIntervals = intervals;
        pianoKeyboard.setScaleIntervals (intervals);
    }

    void PitchCurveEditor::notifyChanged()
    {
        if (listener != nullptr)
            listener->pitchCurveChanged();
    }

    // === Feature 1: Measures and time signature ===
    void PitchCurveEditor::setMeasuresVisible (int measures)
    {
        measuresVisible = juce::jlimit (1, 32, measures);
        recalculateTimeVisible();
        repaint();
    }

    void PitchCurveEditor::setTimeSignature (int numerator, int denominator)
    {
        timeSigNum = juce::jlimit (1, 32, numerator);
        timeSigDen = juce::jlimit (1, 32, denominator);
        recalculateTimeVisible();
        repaint();
    }

    void PitchCurveEditor::recalculateTimeVisible()
    {
        const double beatUnit = 4.0 / timeSigDen;
        const double ppqPerBar = timeSigNum * beatUnit;
        timeVisible = measuresVisible * ppqPerBar;
    }

    // === Feature 2: Auto-scroll ===
    void PitchCurveEditor::setAutoScroll (bool enabled)
    {
        autoScrollEnabled = enabled;
    }

    void PitchCurveEditor::setWaveformOverlay (const float* samples, int numSamples, double /*sampleRate*/)
    {
        if (samples == nullptr || numSamples <= 0)
        {
            hasWaveform = false;
            return;
        }
        // Keep the most recent audio block for the non-spectral display types
        // (unchanged behaviour).
        waveformBuffer.setSize (1, numSamples, false, false, true);
        waveformBuffer.copyFrom (0, 0, samples, numSamples);

        // Append into the ring buffer so the Spectral (FFT) view always has a
        // >= 512-sample window to compute a spectrum.
        const int cap = kWaveRingCapacity;
        const int n = juce::jmin (numSamples, cap);
        for (int i = 0; i < n; ++i)
        {
            waveformRing.setSample (0, waveformRingWritePos, samples[i]);
            waveformRingWritePos = (waveformRingWritePos + 1) % cap;
        }
        waveformTotalWritten += numSamples;
        hasWaveform = true;
        repaint();
    }

    void PitchCurveEditor::setPlayheadTime (double time, bool /*isHostPlaying*/, bool isLooping)
    {
        // A manual seek (Reset Playhead / DAW scrub) produces a large discontinuity
        // in the transport position, whereas normal playback only advances by a few
        // hundredths of a beat per frame. We use this to tell the two apart.
        const double delta = time - playheadTime;
        // When looping, the wrap (L -> 0) looks like a large backward jump. Treat a
        // near-full-window backward jump as normal advance (not a seek) so the
        // auto-scroll follow is not interrupted / snapped at every loop boundary.
        const bool isWrap = isLooping && (delta < 0.0) && (std::abs (delta) > timeVisible * 0.9);
        const bool isSeek = (std::abs (delta) > 0.5) && ! isWrap;

        if (autoScrollEnabled)
        {
            // Auto-scroll ON: keep the playhead centered while it advances (smooth follow).
            double targetScroll = time - timeVisible * 0.5;
            targetScroll = juce::jmax (0.0, targetScroll);
            scrollOffset = scrollOffset + (targetScroll - scrollOffset) * 0.15;
        }
        else if (isSeek && !isLooping)
        {
            // 2026-07-24 (Fix curve editor scroll bug): the previous
            // implementation treated every "seek" (large discontinuity in
            // the transport position) as a user-driven seek and centered
            // the view on the playhead. This worked for explicit seeks
            // (Reset Playhead / DAW scrub) but ALSO fired on loop
            // boundaries (the transport wraps from the end of the loop
            // back to 0, which is a large backward jump) when the loop
            // is shorter than 90% of the visible window, because the
            // `isWrap` detection (`|delta| > timeVisible * 0.9`) was
            // false. The result: every loop boundary caused a "jump" of
            // the playhead line, as if the view were trying to recenter
            // the playhead.
            //
            // The fix: when the Loop Playhead is enabled, NEVER
            // re-center the view on seek. The view stays where the user
            // put it, regardless of what the transport does. This is
            // the expected behaviour: "autoscroll OFF + Loop Playhead
            // ON" means the user wants the view to be static while the
            // playhead loops on a fixed window.
            //
            // Auto-scroll OFF: keep the view fixed during playback so the playhead
            // can run past the visible window. Only reveal the playhead on an
            // explicit seek so the user is not left staring at empty space.
            scrollOffset = juce::jmax (0.0, time - timeVisible * 0.5);
        }
        // else: auto-scroll OFF and continuous playback -> leave the view untouched.
        // ALSO: auto-scroll OFF + Loop Playhead ON + loop boundary seek
        // -> leave the view untouched (the loop is a deliberate user
        // setting, not a seek to reveal).

        playheadTime = time;
        loopingPlayhead = isLooping;
        repaint();
    }

    void PitchCurveEditor::returnToStart()
    {
        // Explicit "return to start": reveal time 0 by resetting the horizontal
        // scroll. Independent of setPlayheadTime's seek detector (which only fires
        // for large jumps), so it is reliable on the first click in every context.
        // Preserves zoom and pitch range.
        scrollOffset = 0.0;
        playheadTime = 0.0;
        repaint();
    }

    // === Copy/Paste ===
    void PitchCurveEditor::performCopy()
    {
        clipboard.clearQuick();
        if (selectedIndices.isEmpty())
        {
            for (int i = 0; i < curve.getNumPoints(); ++i)
                clipboard.add (curve.getPoint (i));
        }
        else
        {
            double refTime = curve.getPoint (selectedIndices.getFirst()).time;
            for (int idx : selectedIndices)
                clipboard.add ({ curve.getPoint (idx).time - refTime, curve.getPoint (idx).pitch });
        }
    }

    void PitchCurveEditor::performPaste()
    {
        if (clipboard.isEmpty()) return;

        // Local copy to avoid const issues on the static member
        auto localClip = clipboard;

        // Find the paste time: playhead if available, otherwise current view
        double pasteTime = playheadTime;
        if (pasteTime < scrollOffset) pasteTime = scrollOffset;
        // Use the minimum clipboard time as reference
        double minT = localClip.getFirst().time;
        for (int i = 0; i < localClip.size(); ++i)
            if (localClip[i].time < minT) minT = localClip[i].time;

        auto action = new CurveEditAction (this);
        selectedIndices.clear();
        for (int i = 0; i < localClip.size(); ++i)
        {
            double t = localClip[i].time - minT + pasteTime;
            int idx = curve.addOrUpdatePoint (t, localClip[i].pitch);
            selectedIndices.add (idx);
        }
        action->setAfter();
        registerUndoableAction (action);
        repaint();
        notifyChanged();
    }

    void PitchCurveEditor::performDeleteSelected()
    {
        if (selectedIndices.isEmpty()) return;
        auto action = new CurveEditAction (this);
        // Collect the times, then delete by time (avoids index shifts)
        juce::Array<double> timesToDelete;
        for (int idx : selectedIndices)
            timesToDelete.add (curve.getPoint (idx).time);
        for (double t : timesToDelete)
            curve.removePointNear (t, 0.001);
        selectedIndices.clear();
        action->setAfter();
        registerUndoableAction (action);
        repaint();
        notifyChanged();
    }

    // === UndoableAction ===
    bool PitchCurveEditor::CurveEditAction::perform()
    {
        editor->curve = after;
        editor->repaint();
        editor->notifyChanged();
        return true;
    }

    bool PitchCurveEditor::CurveEditAction::undo()
    {
        editor->curve = before;
        editor->repaint();
        editor->notifyChanged();
        return true;
    }

    // === Keyboard ===
    bool PitchCurveEditor::keyPressed (const juce::KeyPress& key)
    {
        // Ctrl/Cmd + Z = Undo
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Z')
        {
            performUndo();
            return true;
        }
        // Ctrl/Cmd + Shift + Z or Ctrl/Cmd + Y = Redo
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'Y')
        {
            performRedo();
            return true;
        }
        if (key.getModifiers().isCommandDown() && key.getModifiers().isShiftDown() && key.getKeyCode() == 'Z')
        {
            performRedo();
            return true;
        }
        // Ctrl/Cmd + C = Copy
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'C')
        {
            performCopy();
            return true;
        }
        // Ctrl/Cmd + V = Paste
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'V')
        {
            performPaste();
            return true;
        }
        // Delete/Backspace = Delete selected points
        if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
        {
            performDeleteSelected();
            return true;
        }
        // Ctrl/Cmd + A = Select all
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'A')
        {
            selectedIndices.clear();
            for (int i = 0; i < curve.getNumPoints(); ++i)
                selectedIndices.add (i);
            repaint();
            return true;
        }
        return false;
    }
}



