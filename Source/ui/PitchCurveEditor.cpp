// PitchCurveEditor.cpp
// Implementation de l'editeur de pitch curve.

#include "PitchCurveEditor.h"
#include "OVTFonts.h"
#include "OVTTheme.h"
#include "../dsp/NoteUtils.h"

namespace ui
{
    const juce::Colour PitchCurveEditor::kCurveColour = juce::Colour (0xff4caf50); // vert
    const juce::Colour PitchCurveEditor::kPointColour = juce::Colour (0xffe91e63); // rose
    const juce::Colour PitchCurveEditor::kGridColour  = juce::Colour (0x40ffffff);

    // Static clipboard for copy/paste across instances
    juce::Array<atdsp::PitchPoint> PitchCurveEditor::clipboard;

    PitchCurveEditor::PitchCurveEditor()
    {
        // Do NOT load the "default" preset here — the parent editor will
        // call setCurve() from the processor's pitchCurve on the first
        // timer tick (pendingCurveRestore flag). Loading "default" here
        // would flash the default preset before the real curve is synced.
        // We keep curve as an empty/zero-state object until synced.
        startTimerHz (30);
        // S'assurer que l'editeur intercepte bien les clics meme s'il est desactive
        // (l'etat editorEnabled ne bloque que la logique interne, pas les events).
        setInterceptsMouseClicks (true, true);
        setEnabled (true);

        // Le piano keyboard est place a gauche, dans la zone reservee
        // (resizee dans resized()). On l'ajoute comme enfant.
        addAndMakeVisible (pianoKeyboard);
        // Le piano keyboard n'est qu'un affichage : il ne repond pas aux
        // clics. On desactive l'interception des clics pour eviter qu'il
        // ne mange les events souris destines au curve editor (le parent),
        // ce qui empechait le drag des points (bug identifie le 2026-06-10).
        pianoKeyboard.setInterceptsMouseClicks (false, false);
        // Plage par defaut : C2 -> C7 (suffit pour les voix).
        pianoKeyboard.setRange (36, 96);

        // Embedded controls: Measures combo + Auto-Scroll toggle.
        // These are children of PitchCurveEditor, not PluginEditor, so they
        // cannot be blocked by the tabbedComponent's tab buttons.
        measuresLabel.setText ("Measures", juce::dontSendNotification);
        measuresLabel.setJustificationType (juce::Justification::left);
        measuresLabel.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
        measuresLabel.setFont (ovt::fontMeasuresLabel());
        addAndMakeVisible (measuresLabel);

        measuresBox.addItemList ({ "1", "2", "4", "8", "16", "32" }, 1);
        measuresBox.setSelectedItemIndex (2, juce::dontSendNotification);
        measuresBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a36));
        measuresBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xffcccccc));
        measuresBox.setColour (juce::ComboBox::outlineColourId, juce::Colour (0x441A9AF0));
        measuresBox.setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff1A9AF0));
        measuresBox.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff191b1e));
        measuresBox.setColour (juce::PopupMenu::textColourId, juce::Colour (0xffcccccc));
        measuresBox.onChange = [this] { setMeasuresVisible (measuresBox.getText().getIntValue()); };
        addAndMakeVisible (measuresBox);

        autoScrollToggle.setButtonText ("Auto-Scroll");
        // Force dark mode colours (curve editor is always dark regardless of theme)
        autoScrollToggle.setColour (juce::ToggleButton::textColourId, juce::Colour (0xffcccccc));
        autoScrollToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour (0xff1A9AF0));
        autoScrollToggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colour (0xff555555));
        autoScrollToggle.setTooltip ("Automatically scroll the editor view during playback");
        autoScrollToggle.onClick = [this] { autoScrollEnabled = autoScrollToggle.getToggleState(); };
        addAndMakeVisible (autoScrollToggle);

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
            btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0x331A9AF0));
            btn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcccccc));
            btn.setTooltip (tip);
            addAndMakeVisible (btn);
        };
        setupUndoBtn (undoButton, "Undo (Ctrl+Z)");
        setupUndoBtn (redoButton, "Redo (Ctrl+Y)");

        undoButton.onClick = [this] { performUndo(); };
        redoButton.onClick = [this] { performRedo(); };
    }

    PitchCurveEditor::~PitchCurveEditor() { stopTimer(); }

    void PitchCurveEditor::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds();

        // Fond transparent pour laisser voir le gradient de PluginEditor.
        g.fillAll (juce::Colours::transparentBlack);

        // La zone d'edition de courbe commence apres le piano keyboard (a gauche).
        const int pianoW = pianoKeyboard.getWidth();
        const int rulerH = 24;
        const auto plotArea = juce::Rectangle<int> (pianoW, rulerH,
                                                    b.getWidth() - pianoW,
                                                    b.getHeight() - rulerH);

        // Decoupe pour ne pas dessiner sur le piano keyboard.
        g.saveState();
        g.reduceClipRegion (juce::Rectangle<int>(pianoW, 0, b.getWidth() - pianoW, b.getHeight()));

        // === Fond de la regle (Ruler) ===
        g.setColour (ovt::rulerBg());
        g.fillRect (pianoW, 0, b.getWidth() - pianoW, rulerH);
        
        // Bordure inferieure de la regle
        g.setColour (ovt::curveGrid());
        g.drawHorizontalLine (rulerH, static_cast<float> (pianoW), static_cast<float> (b.getWidth()));

        // === Waveform overlay ===
        if (hasWaveform && waveformBuffer.getNumSamples() > 0)
        {
            const auto waveformArea = juce::Rectangle<int> (pianoW, rulerH,
                                                             b.getWidth() - pianoW, b.getHeight() - rulerH);
            ovt::drawWaveformOverlay (g, waveformBuffer.getReadPointer (0),
                                       waveformBuffer.getNumSamples(), waveformArea,
                                       static_cast<ovt::WaveformDisplayType> (currentDisplayType));
        }

        // === Grille : lignes horizontales pour les octaves C2, C3, C4, C5, C6 ===
        g.setColour (ovt::curveGrid());
        const float refFreqs[] = { 65.4f, 130.8f, 261.6f, 523.3f, 1046.5f };
        const char* labels[]    = { "C2",   "C3",   "C4",   "C5",   "C6" };
        for (int i = 0; i < 5; ++i)
        {
            const float y = pitchToY (refFreqs[i]);
            g.drawHorizontalLine (static_cast<int> (y), static_cast<float> (pianoW), static_cast<float> (b.getWidth()));
            // On decale le texte vers la droite pour eviter la superposition
            // g.setColour (ovt::curveGrid().withAlpha (0.7f));
            // g.drawText (labels[i], pianoW + 4, static_cast<int> (y) - 7, 28, 14, juce::Justification::left);
            g.setColour (ovt::curveGrid());
        }

        // === Scale note lines (horizontal lines for notes in the current scale) ===
        if (! scaleIntervals.isEmpty())
        {
            g.setColour (ovt::scaleLine());
            const float lowestHz = minHz;
            const float highestHz = maxHz;
            const int lowestMidi = static_cast<int> (std::ceil (atdsp::hzToMidiFloat (lowestHz)));
            const int highestMidi = static_cast<int> (std::floor (atdsp::hzToMidiFloat (highestHz)));
            for (int midi = lowestMidi; midi <= highestMidi; ++midi)
            {
                const int noteInOct = atdsp::midiToNoteInOctave (midi);
                if (noteInOct == 0) continue; // skip C (already drawn as octave grid)
                if (! scaleIntervals.contains (noteInOct)) continue;
                const float hz = atdsp::midiToHz (static_cast<float> (midi));
                const float y = pitchToY (hz);
                g.drawHorizontalLine (static_cast<int> (y),
                                      static_cast<float> (pianoW),
                                      static_cast<float> (b.getWidth()));
            }
        }

        // === Lignes verticales (repere par Beat et Mesure) et Ruler ===
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

            // Ligne verticale dans la grille
            g.setColour (ovt::curveGrid().withAlpha (isBarStart ? 0.6f : (isBeat ? 0.3f : 0.1f)));
            g.drawVerticalLine (static_cast<int> (x), rulerH, static_cast<float> (b.getHeight()));

            // Graduations dans le ruler
            if (isBarStart)
            {
                g.setColour (juce::Colours::white.withAlpha (0.8f));
                g.drawVerticalLine (static_cast<int> (x), rulerH - 4.0f, rulerH);

                // Texte du ruler : bar number (1, 2, 3...)
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

        // === Courbe interpolee ===
        if (curve.getNumPoints() >= 2)
        {
            juce::Path p;
            
            if (curve.isStepMode())
            {
                // Trace en "escalier" (Step mode)
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
                
                // Extrapolation vers la droite
                const auto& last = curve.getPoint (curve.getNumPoints() - 1);
                p.lineTo (static_cast<float> (timeToX (timeVisible)), pitchToY (last.pitch));
            }
            else
            {
                // Trace lineaire
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

        // Draw harmony traces (dashed blue lines) aligned with timeline
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
                for (int i = 0; i < times.size(); ++i)
                {
                    double t = times[i];
                    float pHz = pitches[i];
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

        // === Points (cercles) et Tooltip dynamique ===
        int activePointIndex = (isDragging && dragIndex >= 0) ? dragIndex : hoverIndex;

        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            const auto& pt = curve.getPoint (i);
            const float x = static_cast<float> (timeToX (pt.time));
            const float y = pitchToY (pt.pitch);
            
            // Met en surbrillance le point survole ou en cours de drag
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

            // Affichage du tooltip pour le point actif
            if (isActive)
            {
                juce::String noteStr = getNoteName(pt.pitch);
                
                // Formatage du temps pour correspondre au ruler (Mesure.Beat.Decimale)
                int measure = static_cast<int>(pt.time / 4.0) + 1;
                int beat = static_cast<int>(std::fmod(pt.time, 4.0)) + 1;
                int decimal = static_cast<int>(std::round(std::fmod(pt.time, 1.0) * 100.0));
                juce::String timeStr = juce::String(measure) + "." + juce::String(beat);
                if (decimal > 0 && decimal < 100)
                    timeStr += "." + juce::String(decimal).paddedLeft('0', 2);
                
                juce::String tooltipText = noteStr + " | " + timeStr;
                
                // Fond du tooltip
                int textW = 85;
                int textH = 20;
                juce::Rectangle<float> tooltipBounds (x - textW / 2.0f, y - radius - textH - 5.0f, static_cast<float>(textW), static_cast<float>(textH));
                
                // Empeche le tooltip de sortir du cadre superieur
                if (tooltipBounds.getY() < 0) tooltipBounds.setY(y + radius + 5.0f);
                
                g.setColour (juce::Colours::black.withAlpha(0.8f));
                g.fillRoundedRectangle(tooltipBounds, 4.0f);
                
                g.setColour (juce::Colours::white);
                g.setFont (ovt::fontVersion());
                g.drawText (noteStr, tooltipBounds, juce::Justification::centred, false);
            }
        }

        // === Playhead (Barre verticale de lecture) ===
        double displayPlayhead = playheadTime;
        if (displayPlayhead >= scrollOffset && displayPlayhead <= scrollOffset + timeVisible)
        {
            const float x = static_cast<float> (timeToX (displayPlayhead));
            g.setColour (juce::Colours::red.withAlpha (0.8f));
            g.drawVerticalLine (static_cast<int> (x), rulerH, static_cast<float> (b.getHeight()));
        }

        // === Label aide (coin bas-droit) ===
        g.setColour (juce::Colours::grey.withAlpha(0.6f));
        g.setFont (ovt::fontRuler());
        const juce::String modifierName =
#if JUCE_MAC
            "Cmd";
#else
            "Ctrl";
#endif
        g.drawText ("MouseWheel: Scroll | " + modifierName + "+MouseWheel: Zoom",
                    b.getWidth() - 280, b.getHeight() - 36, 270, 14, juce::Justification::bottomRight);
        g.drawText ("Double-click: Add point | Right-click: Curve presets",
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
                const int textW = g.getCurrentFont().getStringWidth (readout);
                const int boxW = textW + 10;
                const int boxH = 16;
                int boxX = b.getWidth() - boxW - 8;
                int boxY = static_cast<int> (hoverMouseY) - boxH - 4;
                if (boxY < rulerH) boxY = static_cast<int> (hoverMouseY) + 4;
                g.setColour (juce::Colour (0xcc15151e));
                g.fillRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 3.0f);
                g.setColour (juce::Colour (0x661A9AF0));
                g.drawRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 3.0f, 0.5f);
                g.setColour (juce::Colours::white);
                g.drawText (readout, boxX + 5, boxY, boxW - 10, boxH,
                            juce::Justification::centredLeft);
            }
        }

        // Separateur vertical entre le piano et la zone d'edition.
        g.setColour (ovt::isDark() ? juce::Colour (0xff2a2a36) : juce::Colour (0xff8a8a96));
        g.drawVerticalLine (pianoW, rulerH, static_cast<float> (b.getHeight()));

        // === Overlay gris si l'editeur est desactive (mode Live) ===
        if (!editorEnabled)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRect (plotArea);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (ovt::fontComboBox());
            g.drawText ("Live Mode : switch to Curve Editor to edit",
                        plotArea.getX(), plotArea.getY(), plotArea.getWidth(), plotArea.getHeight(),
                        juce::Justification::centred);
        }
    }

    void PitchCurveEditor::resized()
    {
        // Le piano keyboard prend la bande verticale a gauche (largeur 60 px).
        const int pianoW = 60;
        const int rulerH = 24;
        pianoKeyboard.setBounds (0, rulerH, pianoW, getHeight() - rulerH);

        // Embedded controls: top-right corner of the editor
        const int controlY = 2;
        const int controlH = 20;
        const int rightEdge = getWidth() - 4;

        // Auto-Scroll toggle at the far right (hidden when !autoScrollVisible)
        if (autoScrollVisible)
        {
            autoScrollToggle.setBounds (rightEdge - 88, controlY, 88, controlH);
        }
        else
        {
            autoScrollToggle.setBounds (0, 0, 0, 0); // off-screen when hidden
        }

        // Measures combo right before the toggle (or at far right if hidden)
        const int comboRight = autoScrollVisible ? (rightEdge - 88 - 4) : rightEdge;

        // Measures combo right before the toggle
        measuresBox.setBounds (comboRight - 54, controlY, 54, controlH);

        // Measures label right before the combo
        measuresLabel.setBounds (comboRight - 54 - 4 - 64, controlY, 64, controlH);

        // Undo/Redo buttons (bottom-left, below piano keyboard)
        const int btnSize = 22;
        const int btnGap = 4;
        const int btnY = pianoKeyboard.getBottom() + 4;
        undoButton.setBounds (2, btnY, btnSize, btnSize);
        redoButton.setBounds (2 + btnSize + btnGap, btnY, btnSize, btnSize);
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

    // === Conversions coordonnees ===
    // Note : l'axe X couvre uniquement la zone d'edition de la courbe
    // (a droite du piano keyboard). On soustrait la largeur du piano.
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
        // Echelle log.
        const float lh = std::log (p);
        const float lmin = std::log (minHz);
        const float lmax = std::log (maxHz);
        const float t = (lh - lmin) / (lmax - lmin);
        return rulerH + (getHeight() - rulerH) * (1.0f - juce::jlimit (0.0f, 1.0f, t));
    }
    float PitchCurveEditor::yToPitch (float y) const
    {
        const int rulerH = 24;
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, (y - rulerH) / (getHeight() - rulerH));
        const float lmin = std::log (minHz);
        const float lmax = std::log (maxHz);
        return std::exp (lmin + t * (lmax - lmin));
    }

    juce::String PitchCurveEditor::getNoteName (float hz) const
    {
        if (hz <= 0.0f) return "";
        // Formule midi = 69 + 12 * log2(f / 440)
        int midiNote = static_cast<int>(std::round(69.0f + 12.0f * std::log2(hz / 440.0f)));
        const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int octave = (midiNote / 12) - 1;
        int noteIndex = midiNote % 12;
        if (noteIndex < 0) noteIndex += 12; // Securite
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

    // === Saisie souris ===
    void PitchCurveEditor::mouseDown (const juce::MouseEvent& e)
    {
        grabKeyboardFocus(); // pour les raccourcis clavier
        // Si l'editeur est desactive (mode Auto), on ignore tout.
        if (!editorEnabled) return;

        // Snapshot pour undo
        pendingUndoSnapshot = curve;

        const juce::Point<float> p (e.position.x, e.position.y);
        dragIndex = findPointAtPixel (p);
        isDragging = false;
        isDraggingSelection = false;
        isMarqueeSelecting = false;

        if (dragIndex >= 0)
        {
            // Debut du drag : verifier si on doit supprimer (clic droit ou Alt+clic).
            if (e.mods.isRightButtonDown() || e.mods.isAltDown())
            {
                curve.getPoint (dragIndex); // index valide
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
                if (currentScale == atdsp::Scale::Custom && ! customIntervalsCache.isEmpty())
                    hz = atdsp::PitchCurve::snapToScaleCustom (hz, customIntervalsCache);
                else
                    hz = atdsp::PitchCurve::snapToScale (hz, keyIdx, currentScale);
            }
            else
            {
                float snappedHz = atdsp::PitchCurve::snapToScale (hz, 0, atdsp::Scale::Chromatic);
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

        // Mise a jour du temps et du pitch.
        double t = xToTime (e.position.x);
        float hz = yToPitch (e.position.y);
        
        // Snap to grid temporel (0.5s par defaut = 8eme note)
        double gridStep = 0.5;
        double nearestGrid = std::round(t / gridStep) * gridStep;
        if (snapToGridEnabled)
        {
            t = nearestGrid;
        }
        else if (std::abs(t - nearestGrid) < 0.05)
        {
            // Magnetisme leger a +/- 0.05s si pas de snap strict
            t = nearestGrid;
        }
        
        // Snap to note / Snap to scale
        if (snapEnabled)
        {
            if (currentScale == atdsp::Scale::Custom && ! customIntervalsCache.isEmpty())
                hz = atdsp::PitchCurve::snapToScaleCustom (hz, customIntervalsCache);
            else
                hz = atdsp::PitchCurve::snapToScale (hz, keyIdx, currentScale);
        }
        else
        {
            // Snap to note (Chromatique implicite par magnetisme)
            // L'utilisateur voulait un "snap-to-note lors du changement de pitch" meme sans la gamme.
            float snappedHz = atdsp::PitchCurve::snapToScale(hz, 0, atdsp::Scale::Chromatic);
            // Magnetisme si on est proche de la note exacte (ex: ecart de moins de 15 cents)
            float centsDiff = 1200.0f * std::log2(hz / snappedHz);
            if (std::abs(centsDiff) < 15.0f) {
                hz = snappedHz;
            }
        }
        
        // Ecriture directe via setPointTimeAndPitch pour maintenir le tri et l'index de drag
        curve.setPointTimeAndPitch (dragIndex, t, hz);
        repaint();
    }

    void PitchCurveEditor::mouseUp (const juce::MouseEvent& /*e*/)
    {
        // Verifier si une modification a eu lieu
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

        // Enregistrer l'undo si un changement a eu lieu
        if (wasModified && hadPoints)
        {
            // On compare les snapshots (evite un undo vide si le drag n'a rien change)
            // Utilisation d'une simple comparaison de nombre de points + serialisation
            // comme heuristique rapide pour eviter les undo vides
            auto beforeXml = pendingUndoSnapshot.toXml();
            auto afterXml = curve.toXml();
            bool changed = (beforeXml->createDocument ("") != afterXml->createDocument (""));
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
        // Si l'editeur est desactive (mode Auto), on ignore.
        if (!editorEnabled) return;

        // Ajoute un point a la position du curseur.
        double t = xToTime (e.position.x);
        float hz = yToPitch (e.position.y);
        
        // Snap to grid temporel
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
            if (currentScale == atdsp::Scale::Custom && ! customIntervalsCache.isEmpty())
                hz = atdsp::PitchCurve::snapToScaleCustom (hz, customIntervalsCache);
            else
                hz = atdsp::PitchCurve::snapToScale (hz, keyIdx, currentScale);
        }
        curve.addOrUpdatePoint (t, hz);
        notifyChanged();
    }

    void PitchCurveEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        // Facteur de zoom/scroll
        float scrollAmount = wheel.deltaY;
        
        // Si Ctrl est enfonce, on zoome/dezoome
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            // Zoom base sur le centre de l'ecran ou la position de la souris
            float zoomFactor = 1.0f - scrollAmount * 2.0f;
            if (zoomFactor < 0.1f) zoomFactor = 0.1f;
            if (zoomFactor > 10.0f) zoomFactor = 10.0f;
            
            float centerPitch = yToPitch(e.position.y);
            float currentRangeCents = 1200.0f * std::log2(maxHz / minHz);
            float newRangeCents = currentRangeCents * zoomFactor;
            
            // Limite le zoom max (1 octave) et min (8 octaves)
            if (newRangeCents < 1200.0f) newRangeCents = 1200.0f;
            if (newRangeCents > 1200.0f * 8.0f) newRangeCents = 1200.0f * 8.0f;
            
            float halfRangeLog = (newRangeCents / 1200.0f) * std::log(2.0f) / 2.0f;
            float centerLog = std::log(centerPitch);
            
            minHz = std::exp(centerLog - halfRangeLog);
            maxHz = std::exp(centerLog + halfRangeLog);
        }
        else
        {
            // Sinon on scroll (pan) vers le haut/bas
            // deltaY > 0 (scroll up) -> on veut voir plus haut sur le piano -> fMin/fMax augmentent
            float currentRangeLog = std::log(maxHz / minHz);
            float shiftLog = scrollAmount * currentRangeLog * 0.5f;
            minHz = std::exp(std::log(minHz) + shiftLog);
            maxHz = std::exp(std::log(maxHz) + shiftLog);
        }
        
        // Limites absolues (C0 a C9)
        if (minHz < 16.35f) { float r = maxHz/minHz; minHz = 16.35f; maxHz = minHz * r; }
        if (maxHz > 8372.0f) { float r = maxHz/minHz; maxHz = 8372.0f; minHz = maxHz / r; }
        
        // Mise a jour du piano
        pianoKeyboard.setRange(static_cast<int>(atdsp::hzToMidiFloat(minHz)), 
                               static_cast<int>(atdsp::hzToMidiFloat(maxHz)));
        repaint();
    }

    // === API publique ===
    void PitchCurveEditor::setCurve (const atdsp::PitchCurve& newCurve)
    {
        curve = newCurve; // copie (inclut stepMode, snapEnabled, snapToGridEnabled)
        // Applique les parametres d'edition stockes dans la courbe
        snapEnabled = curve.isSnapEnabled();
        snapToGridEnabled = curve.isSnapToGridEnabled();
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
            if (currentScale == atdsp::Scale::Custom && ! customIntervalsCache.isEmpty())
                hz = atdsp::PitchCurve::snapToScaleCustom (hz, customIntervalsCache);
            else
                hz = atdsp::PitchCurve::snapToScale (hz, keyIdx, currentScale);
        }
        curve.addOrUpdatePoint (currentTime, hz);
        notifyChanged();
    }

    void PitchCurveEditor::setViewRange (double secVis, float minH, float maxH)
    {
        timeVisible = secVis;
        minHz = minH;
        maxHz = maxH;
        pianoKeyboard.setRange(static_cast<int>(atdsp::hzToMidiFloat(minHz)), 
                               static_cast<int>(atdsp::hzToMidiFloat(maxHz)));
        repaint();
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
        // Annule un drag en cours si on desactive.
        if (!b) isDragging = false;
        repaint();
    }

    void PitchCurveEditor::setKeyAndScale (int key, atdsp::Scale scale)
    {
        keyIdx = key;
        currentScale = scale;
        // Re-snap tous les points si le snap est actif.
        if (snapEnabled)
        {
            for (int i = 0; i < curve.getNumPoints(); ++i)
            {
                const float hz = curve.getPoint (i).pitch;
                if (scale == atdsp::Scale::Custom && ! customIntervalsCache.isEmpty())
                    curve.getPoint (i).pitch =
                        atdsp::PitchCurve::snapToScaleCustom (hz, customIntervalsCache);
                else
                    curve.getPoint (i).pitch =
                        atdsp::PitchCurve::snapToScale (hz, keyIdx, currentScale);
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

    void PitchCurveEditor::setAutoScrollVisible (bool visible)
    {
        autoScrollVisible = visible;
        autoScrollToggle.setVisible (visible);
        if (!visible)
        {
            // Force disable auto-scroll when the control is hidden
            autoScrollEnabled = false;
            autoScrollToggle.setToggleState (false, juce::dontSendNotification);
        }
        resized();
    }

    void PitchCurveEditor::setWaveformOverlay (const float* samples, int numSamples, double /*sampleRate*/)
    {
        if (samples == nullptr || numSamples <= 0)
        {
            hasWaveform = false;
            return;
        }
        waveformBuffer.setSize (1, numSamples, false, false, true);
        waveformBuffer.copyFrom (0, 0, samples, numSamples);
        hasWaveform = true;
        repaint();
    }

    void PitchCurveEditor::setPlayheadTime (double time, bool /*isHostPlaying*/)
    {
        // === Detect playing transitions for trace cleanup ===
        const bool playing = (time != playheadTime); // transport is advancing
        if (playing && ! wasPlayingLastFrame)
        {
            for (int v = 0; v < maxHarmonyVoices; ++v)
            {
                harmonyTimes.getReference(v).clear();
                harmonyPitches.getReference(v).clear();
            }
        }
        wasPlayingLastFrame = playing;

        if (autoScrollEnabled && autoScrollVisible)
        {
            // Auto-scroll: LERP fluide, playhead reste au centre
            double targetScroll = time - timeVisible * 0.5;
            targetScroll = juce::jmax (0.0, targetScroll);
            scrollOffset = scrollOffset + (targetScroll - scrollOffset) * 0.15;
        }
        else if (autoScrollVisible)
        {
            // Stopped: snap instantane si seek manuel
            if (std::abs (time - stoppedPlayheadTime) > 0.01)
            {
                scrollOffset = juce::jmax (0.0, time - timeVisible * 0.5);
                stoppedPlayheadTime = time;
            }
        }

        playheadTime = time;
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

        // Copie locale pour eviter les problemes de const sur membre static
        auto localClip = clipboard;

        // Trouver le temps de collage : playhead si disponible, sinon vue courante
        double pasteTime = playheadTime;
        if (pasteTime < scrollOffset) pasteTime = scrollOffset;
        // Utiliser le temps minimum du clipboard comme reference
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
        // Collecter les temps, puis supprimer par temps (evite les decalages)
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
        // Ctrl/Cmd + Shift + Z ou Ctrl/Cmd + Y = Redo
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
