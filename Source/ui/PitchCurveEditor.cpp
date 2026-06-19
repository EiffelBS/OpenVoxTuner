// PitchCurveEditor.cpp
// Implementation de l'editeur de pitch curve.

#include "PitchCurveEditor.h"

namespace ui
{
    const juce::Colour PitchCurveEditor::kCurveColour = juce::Colour (0xff4caf50); // vert
    const juce::Colour PitchCurveEditor::kPointColour = juce::Colour (0xffe91e63); // rose
    const juce::Colour PitchCurveEditor::kGridColour  = juce::Colour (0x40ffffff);

    PitchCurveEditor::PitchCurveEditor()
    {
        // Initialise avec un preset par defaut pour que l'editeur ne soit pas vide.
        curve.loadPreset ("default");
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

        // init harmony buffers
        harmonyTimes.clear();
        harmonyPitches.clear();
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            harmonyTimes.add (juce::Array<double>());
            harmonyPitches.add (juce::Array<float>());
        }
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
        g.setColour (juce::Colour (0xff1a1a1a));
        g.fillRect (pianoW, 0, b.getWidth() - pianoW, rulerH);
        
        // Bordure inferieure de la regle
        g.setColour (kGridColour);
        g.drawHorizontalLine (rulerH, static_cast<float> (pianoW), static_cast<float> (b.getWidth()));

        // === Grille : lignes horizontales pour les octaves C2, C3, C4, C5, C6 ===
        g.setColour (kGridColour);
        const float refFreqs[] = { 65.4f, 130.8f, 261.6f, 523.3f, 1046.5f };
        const char* labels[]    = { "C2",   "C3",   "C4",   "C5",   "C6" };
        for (int i = 0; i < 5; ++i)
        {
            const float y = pitchToY (refFreqs[i]);
            g.drawHorizontalLine (static_cast<int> (y), static_cast<float> (pianoW), static_cast<float> (b.getWidth()));
            // On decale le texte vers la droite pour eviter la superposition
            // g.setColour (kGridColour.withAlpha (0.7f));
            // g.drawText (labels[i], pianoW + 4, static_cast<int> (y) - 7, 28, 14, juce::Justification::left);
            g.setColour (kGridColour);
        }

        // === Lignes verticales (repere par Beat et Mesure) et Ruler ===
        for (double t = 0.0; t <= timeVisible; t += 0.5) // Sous-divisions
        {
            const double x = timeToX (t);
            bool isBeat = (std::fmod(t, 1.0) == 0.0);
            bool isMeasure = (std::fmod(t, 4.0) == 0.0);
            
            // Ligne verticale dans la grille
            g.setColour (kGridColour.withAlpha (isMeasure ? 0.6f : (isBeat ? 0.3f : 0.1f)));
            g.drawVerticalLine (static_cast<int> (x), rulerH, static_cast<float> (b.getHeight()));
            
            // Graduations dans le ruler
            if (isBeat)
            {
                g.setColour (juce::Colours::white.withAlpha(0.8f));
                g.drawVerticalLine (static_cast<int> (x), rulerH - 4.0f, rulerH);
                
                // Texte du ruler
                juce::String text;
                int measure = static_cast<int>(t / 4.0) + 1;
                int beat = static_cast<int>(std::fmod(t, 4.0)) + 1;
                if (beat == 1) text = juce::String(measure);
                else text = juce::String(measure) + "." + juce::String(beat);
                
                g.setFont (11.0f);
                g.drawText (text, static_cast<int>(x) + 4, 0, 40, rulerH, juce::Justification::centredLeft);
            }
            else
            {
                // Sous-division
                g.setColour (juce::Colours::white.withAlpha(0.4f));
                g.drawVerticalLine (static_cast<int> (x), rulerH - 2.0f, rulerH);
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
                g.setFont (12.0f);
                g.drawText (tooltipText, tooltipBounds, juce::Justification::centred, false);
            }
        }

        // === Playhead (Barre verticale de lecture) ===
        // On boucle le playhead visuellement aussi sur 16.0 beats
        double displayPlayhead = std::fmod(playheadTime, 16.0);
        if (displayPlayhead >= 0.0 && displayPlayhead <= timeVisible)
        {
            const float x = static_cast<float> (timeToX (displayPlayhead));
            g.setColour (juce::Colours::red.withAlpha (0.8f));
            g.drawVerticalLine (static_cast<int> (x), rulerH, static_cast<float> (b.getHeight()));
        }

        // === Label aide (coin bas-droit) ===
        g.setColour (juce::Colours::grey.withAlpha(0.6f));
        g.setFont (11.0f);
        const juce::String modifierName =
#if JUCE_MAC
            "Cmd";
#else
            "Ctrl";
#endif
        g.drawText ("MouseWheel: Scroll | " + modifierName + "+MouseWheel: Zoom",
                    b.getWidth() - 280, b.getHeight() - 36, 270, 14, juce::Justification::bottomRight);
        g.drawText ("Double-click: Add point | Right-click: Menu",
                    b.getWidth() - 280, b.getHeight() - 20, 270, 14, juce::Justification::bottomRight);

        g.restoreState();

        // Separateur vertical entre le piano et la zone d'edition.
        g.setColour (juce::Colour (0xff2a2a36));
        g.drawVerticalLine (pianoW, rulerH, static_cast<float> (b.getHeight()));

        // === Overlay gris si l'editeur est desactive (mode Live) ===
        if (!editorEnabled)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRect (plotArea);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.setFont (14.0f);
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
        return pianoW + (t / timeVisible) * plotW;
    }
    double PitchCurveEditor::xToTime (float x) const
    {
        const int pianoW = pianoKeyboard.getWidth();
        const int plotW  = juce::jmax (1, getWidth() - pianoW);
        return juce::jlimit (0.0, timeVisible,
                             ((x - pianoW) / plotW) * timeVisible);
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
        // Si l'editeur est desactive (mode Auto), on ignore tout.
        if (!editorEnabled) return;

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
            return;
        }

        if (isDragging)
        {
            isDragging = false;
            notifyChanged();
        }
    }

    void PitchCurveEditor::mouseMove (const juce::MouseEvent& e)
    {
        if (!editorEnabled) return;

        const juce::Point<float> p (e.position.x, e.position.y);
        int newHover = findPointAtPixel (p);
        
        if (newHover != hoverIndex)
        {
            hoverIndex = newHover;
            repaint();
        }
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
        curve = newCurve; // copie
        repaint();
        notifyChanged();
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
        pianoKeyboard.setScaleIntervals (intervals);
    }

    void PitchCurveEditor::notifyChanged()
    {
        if (listener != nullptr)
            listener->pitchCurveChanged();
    }
}
