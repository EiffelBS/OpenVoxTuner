// PitchVisualizer.cpp
// Composant GUI : visualisation en temps reel des courbes de pitch.
// Affiche la pitch curve d'entree (rose) et la pitch curve corrigee (vert)
// sur une echelle semi-logarithmique (Hz), avec en overlay :
//   - le nom de la note chantee
//   - l'offset en cents (couleur selon le signe)
//   - un meter de tuning vertical
//   - les lignes horizontales des notes de la gamme courante

#include "PitchVisualizer.h"

namespace ui
{
    // === Couleurs du theme ===
    // Fond plus moderne et transparent.
    const juce::Colour PitchVisualizer::kBg              = juce::Colour (0x4015151b);
    const juce::Colour PitchVisualizer::kGrid            = juce::Colour (0x20ffffff);
    const juce::Colour PitchVisualizer::kInputColour     = juce::Colour (0xffe91e63).withAlpha (0.4f);
    const juce::Colour PitchVisualizer::kOutputColour    = juce::Colour (0xff00e676);
    const juce::Colour PitchVisualizer::kScaleLineColour = juce::Colour (0x10ffffff);
    const juce::Colour PitchVisualizer::kHarmonyColour   = juce::Colour (0xff1A9AF0).withAlpha (0.7f); // bleu spec

    PitchVisualizer::PitchVisualizer()
    {
        inputHistory.clear();
        outputHistory.clear();
        for (int i = 0; i < historySize; ++i)
        {
            inputHistory.add (0.0f);
            outputHistory.add (0.0f);
        }
        // initialize harmony history buffers (maxHarmonyVoices x historySize)
        harmonyHistory.clear();
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            juce::Array<float> h;
            for (int i = 0; i < historySize; ++i) h.add (0.0f);
            harmonyHistory.add (h);
        }
        
        addAndMakeVisible(pianoKeyboard);
        pianoKeyboard.setRange(static_cast<int>(atdsp::hzToMidiFloat(fMin)), 
                               static_cast<int>(atdsp::hzToMidiFloat(fMax)));
        
        startTimerHz (30);
    }

    PitchVisualizer::~PitchVisualizer() { stopTimer(); }

    void PitchVisualizer::pushInputPitch (float hz)
    {
        if (inputHistory.size() >= historySize) inputHistory.remove (0);
        inputHistory.add (hz);
        latestInputHz = hz;
    }

    void PitchVisualizer::pushOutputPitch (float hz)
    {
        if (outputHistory.size() >= historySize) outputHistory.remove (0);
        outputHistory.add (hz);
        latestOutputHz = hz;
    }

    void PitchVisualizer::setNoteInfo (const atdsp::NoteInfo& info)
    {
        noteInfo = info;
    }

    void PitchVisualizer::setScaleIntervals (const juce::Array<int>& intervals)
    {
        scaleIntervals = intervals;
        pianoKeyboard.setScaleIntervals (intervals);
    }

    void PitchVisualizer::setHarmonyFrequencies (const juce::Array<float>& freqs)
    {
        // Push latest harmony frequencies into per-voice history buffers.
        for (int v = 0; v < maxHarmonyVoices; ++v)
        {
            float value = 0.0f;
            if (v < freqs.size()) value = freqs[v];

            auto& h = harmonyHistory.getReference(v);
            if (h.size() >= historySize) h.remove (0);
            h.add (value);
        }
    }

    float PitchVisualizer::hzToY (float hz, int height) const
    {
        if (hz <= 0.0f) return static_cast<float> (height);
        // Utilise la meme echelle que le piano keyboard
        const float midiF = atdsp::hzToMidiFloat(hz);
        const int lowestMidi = pianoKeyboard.getLowestMidi();
        const int highestMidi = pianoKeyboard.getHighestMidi();
        const int range = juce::jmax(1, highestMidi - lowestMidi);
        const float t = (midiF - static_cast<float>(lowestMidi)) / static_cast<float>(range);
        return height * (1.0f - juce::jlimit(0.0f, 1.0f, t));
    }

    void PitchVisualizer::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds();
        const int W = b.getWidth();
        const int H = b.getHeight();

        // Reserve la zone du haut pour le bandeau note / cents (60 px)
        // et la zone droite pour le meter de tuning (50 px).
        const int headerH   = juce::jmin (60, H / 3);
        const int meterW    = juce::jmin (60, W / 6);
        const int pianoW    = pianoKeyboard.getWidth();

        const auto plotArea = juce::Rectangle<int> (pianoW, headerH, W - meterW - pianoW, H - headerH);

        // === Fond ===
        g.fillAll (kBg);

        // === Bandeau du haut : note chantee + offset cents ===
        g.setColour (juce::Colour (0xff181820));
        g.fillRect (0, 0, W, headerH);

        // Note chantee (gros texte).
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (28.0f, juce::Font::bold));
        g.drawText (noteInfo.name,
                    12, 6, 130, 38,
                    juce::Justification::centredLeft);

        // Nom de la note cible.
        if (noteInfo.valid && noteInfo.targetName != noteInfo.name)
        {
            g.setColour (juce::Colour (0xff8bc34a));
            g.setFont (12.0f);
            g.drawText ("-> " + noteInfo.targetName,
                        12, 42, 130, 16,
                        juce::Justification::centredLeft);
        }

        // Cents (texte a droite de la note).
        if (noteInfo.valid)
        {
            const float cents = noteInfo.cents;
            juce::Colour centsCol = juce::Colours::white;
            if (std::abs (cents) < 5.0f)         centsCol = juce::Colour (0xff4caf50);
            else if (std::abs (cents) < 25.0f)   centsCol = juce::Colour (0xffcddc39);
            else                                  centsCol = juce::Colour (0xffe57373);

            g.setColour (centsCol);
            g.setFont (juce::Font (18.0f, juce::Font::bold));
            const juce::String centsStr =
                (cents >= 0.0f ? "+" : "") + juce::String (static_cast<int> (std::round (cents))) + " c";
            g.drawText (centsStr,
                        145, 12, 130, 28,
                        juce::Justification::centredLeft);
        }

        // === Meter de tuning vertical (a droite) ===
        if (meterW > 0)
        {
            const auto meterArea = juce::Rectangle<int> (W - meterW, headerH, meterW, H - headerH);
            g.setColour (juce::Colour (0xff181820));
            g.fillRect (meterArea);

            // Barre centrale (=0 cents).
            const int midY = meterArea.getCentreY();
            g.setColour (juce::Colour (0xff4caf50));
            g.fillRect (meterArea.getX() + meterW / 2 - 1, meterArea.getY() + 4, 2, meterArea.getHeight() - 8);

            // Graduations : +/- 50, +/- 100 cents.
            g.setColour (kGrid);
            for (int c = -100; c <= 100; c += 50)
            {
                if (c == 0) continue;
                const float ratio = static_cast<float> (c) / 100.0f; // +/- 50% de la hauteur
                const int y = midY - static_cast<int> (ratio * (meterArea.getHeight() - 8) * 0.5f);
                g.drawHorizontalLine (y, static_cast<float> (meterArea.getX() + 4),
                                            static_cast<float> (meterArea.getRight() - 4));
            }

            // Aiguille : position selon cents.
            if (noteInfo.valid)
            {
                const float cents = juce::jlimit (-100.0f, 100.0f, noteInfo.cents);
                const float ratio = cents / 100.0f;
                const int ay = midY - static_cast<int> (ratio * (meterArea.getHeight() - 8) * 0.5f);
                juce::Colour needleCol = juce::Colour (0xff4caf50);
                if (std::abs (cents) > 25.0f) needleCol = juce::Colour (0xffe57373);
                else if (std::abs (cents) > 10.0f) needleCol = juce::Colour (0xffcddc39);
                g.setColour (needleCol);
                g.fillRect (meterArea.getX() + 4, ay - 1, meterW - 8, 2);
            }
        }

        // === Zone du trace (pitch curves) ===
        g.saveState();
        g.reduceClipRegion (plotArea);

        // Grille de base (C2..C6) en gris clair.
        g.setColour (kGrid);
        const float refFreqs[] = { 65.4f, 130.8f, 261.6f, 523.3f, 1046.5f };
        for (int i = 0; i < 5; ++i)
        {
            const float y = plotArea.getY() + hzToY (refFreqs[i], plotArea.getHeight());
            g.drawHorizontalLine (static_cast<int> (y),
                                  static_cast<float> (plotArea.getX()),
                                  static_cast<float> (plotArea.getRight()));
        }

        // Lignes des notes de la gamme (toutes les notes, sur 2 octaves).
        g.setColour (kScaleLineColour);
        for (int oct = 2; oct <= 5; ++oct)
        {
            for (int n = 0; n < scaleIntervals.size(); ++n)
            {
                const int semi = scaleIntervals[n];
                if (semi == 0) continue; // C est deja sur la grille de base
                const int midi = (oct + 1) * 12 + semi;
                const float hz = atdsp::midiToHz (static_cast<float> (midi));
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                g.drawHorizontalLine (static_cast<int> (y),
                                      static_cast<float> (plotArea.getX()),
                                      static_cast<float> (plotArea.getRight()));
            }
        }

        // Trace la pitch curve d'entree (rose/rouge).
        if (inputHistory.size() > 1)
        {
            juce::Path p;
            const float dx = static_cast<float> (plotArea.getWidth()) / static_cast<float> (historySize - 1);
            for (int i = 0; i < inputHistory.size(); ++i)
            {
                const float hz = inputHistory[i];
                if (hz <= 0.0f) continue;
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                const float x = plotArea.getX() + dx * (historySize - inputHistory.size() + i);
                if (i == 0 || inputHistory[i - 1] <= 0.0f) p.startNewSubPath (x, y);
                else p.lineTo (x, y);
            }
            g.setColour (kInputColour);
            g.strokePath (p, juce::PathStrokeType (1.5f));
        }

        // Trace la pitch curve corrigee (vert).
        if (outputHistory.size() > 1)
        {
            juce::Path p;
            const float dx = static_cast<float> (plotArea.getWidth()) / static_cast<float> (historySize - 1);
            for (int i = 0; i < outputHistory.size(); ++i)
            {
                const float hz = outputHistory[i];
                if (hz <= 0.0f) continue;
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                const float x = plotArea.getX() + dx * (historySize - outputHistory.size() + i);
                if (i == 0 || outputHistory[i - 1] <= 0.0f) p.startNewSubPath (x, y);
                else p.lineTo (x, y);
            }

            // Draw line
            g.setColour (kOutputColour);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draw harmony voice traces (one history per voice) in soft blue
        {
            const float dx = static_cast<float> (plotArea.getWidth()) / static_cast<float> (historySize - 1);
            juce::Colour harmonyLine = kHarmonyColour;
            for (int v = 0; v < maxHarmonyVoices; ++v)
            {
                const auto& h = harmonyHistory[v];
                if (h.size() <= 1) continue;
                juce::Path p;
                bool hasAny = false;
                bool segmentOpen = false;
                for (int i = 0; i < h.size(); ++i)
                {
                    const float hz = h[i];
                    const float x = plotArea.getX() + dx * (historySize - h.size() + i);

                    if (hz <= 0.0f)
                    {
                        segmentOpen = false;
                        continue;
                    }

                    const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                    if (!segmentOpen)
                    {
                        p.startNewSubPath (x, y);
                        segmentOpen = true;
                        hasAny = true;
                    }
                    else
                    {
                        p.lineTo (x, y);
                    }
                }
                if (hasAny)
                {
                    g.setColour (harmonyLine);
                    // Draw thin solid line (dashed not available on this JUCE version)
                    g.strokePath (p, juce::PathStrokeType (0.8f));
                }
            }
        }

        g.restoreState();

        // Legende en bas a droite de la zone de trace.
        g.setFont (10.0f);
        g.setColour (kInputColour);
        g.drawText ("Input",
                    plotArea.getRight() - 110, plotArea.getBottom() - 18, 50, 14,
                    juce::Justification::centredRight);
        g.setColour (kOutputColour);
        g.drawText ("Output",
                    plotArea.getRight() - 60, plotArea.getBottom() - 18, 50, 14,
                    juce::Justification::centredRight);
                    
        g.setColour (juce::Colours::grey.withAlpha(0.6f));
        g.setFont (11.0f);
        const juce::String modifierName =
#if JUCE_MAC
            "Cmd";
#else
            "Ctrl";
#endif
        g.drawText ("MouseWheel: Scroll | " + modifierName + "+MouseWheel: Zoom",
                    plotArea.getRight() - 210, plotArea.getBottom() - 32, 200, 14,
                    juce::Justification::bottomRight);
    }

    void PitchVisualizer::resized()
    {
        const int headerH = juce::jmin (60, getHeight() / 3);
        pianoKeyboard.setBounds (0, headerH, 60, getHeight() - headerH);
    }

    void PitchVisualizer::timerCallback()
    {
        pianoKeyboard.setCurrentPitches(latestInputHz, latestOutputHz);
        repaint();
    }

    void PitchVisualizer::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        // Facteur de zoom/scroll
        float scrollAmount = wheel.deltaY;
        
        // Si Ctrl est enfonce, on zoome/dezoome
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            float zoomFactor = 1.0f - scrollAmount * 2.0f;
            if (zoomFactor < 0.1f) zoomFactor = 0.1f;
            if (zoomFactor > 10.0f) zoomFactor = 10.0f;
            
            float centerPitch = std::exp(std::log(fMin) + (std::log(fMax) - std::log(fMin)) * 0.5f);
            float currentRangeCents = 1200.0f * std::log2(fMax / fMin);
            float newRangeCents = currentRangeCents * zoomFactor;
            
            if (newRangeCents < 1200.0f) newRangeCents = 1200.0f;
            if (newRangeCents > 1200.0f * 8.0f) newRangeCents = 1200.0f * 8.0f;
            
            float halfRangeLog = (newRangeCents / 1200.0f) * std::log(2.0f) / 2.0f;
            float centerLog = std::log(centerPitch);
            
            fMin = std::exp(centerLog - halfRangeLog);
            fMax = std::exp(centerLog + halfRangeLog);
        }
        else
        {
            // Scroll (pan) vers le haut/bas
            float currentRangeLog = std::log(fMax / fMin);
            float shiftLog = scrollAmount * currentRangeLog * 0.5f;
            fMin = std::exp(std::log(fMin) + shiftLog);
            fMax = std::exp(std::log(fMax) + shiftLog);
        }
        
        // Limites absolues
        if (fMin < 16.35f) { float r = fMax/fMin; fMin = 16.35f; fMax = fMin * r; }
        if (fMax > 8372.0f) { float r = fMax/fMin; fMax = 8372.0f; fMin = fMax / r; }
        
        pianoKeyboard.setRange(static_cast<int>(atdsp::hzToMidiFloat(fMin)), 
                               static_cast<int>(atdsp::hzToMidiFloat(fMax)));
        repaint();
    }
}
