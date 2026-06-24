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

        // === Layout ===
        // Modern header strip (50px): integrated note display + tuning meter
        // Piano on the left (60px)
        // Plot area fills the rest
        const int headerH = juce::jmin (50, H / 4);
        const int pianoW  = pianoKeyboard.getWidth() > 0 ? pianoKeyboard.getWidth() : 60;

        const auto plotArea = juce::Rectangle<int> (pianoW, headerH, W - pianoW, H - headerH);

        // === Background ===
        g.fillAll (kBg);

        // === Modern header strip ===
        {
            // Dark glass-panel background
            g.setColour (juce::Colour (0xff15151e));
            g.fillRect (0, 0, W, headerH);

            // Bottom border accent line
            g.setColour (juce::Colour (0x331A9AF0));
            g.fillRect (0, headerH - 2, W, 2);

            const int badgeH = headerH - 12;
            const int badgeY = (headerH - badgeH) / 2;

            // ---- Note badge (center-aligned text) ----
            const juce::String noteDisplay = noteInfo.valid ? noteInfo.name : "--";

            // Note name on the left side, centered in its area
            const int noteAreaW = 90;
            const int noteAreaX = 14;

            // Subtle background glow for the note area
            juce::Colour badgeCol = noteInfo.valid
                ? juce::Colour (0x221A9AF0)
                : juce::Colour (0x11ffffff);
            g.setColour (badgeCol);
            g.fillRoundedRectangle ((float)noteAreaX, (float)badgeY, (float)noteAreaW, (float)badgeH, 6.0f);
            g.setColour (juce::Colour (0x441A9AF0));
            g.drawRoundedRectangle ((float)noteAreaX, (float)badgeY, (float)noteAreaW, (float)badgeH, 6.0f, 1.0f);

            // Current sung note, CENTERED in its badge zone
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (24.0f, juce::Font::bold));
            g.drawText (noteDisplay,
                        noteAreaX, badgeY, noteAreaW, badgeH,
                        juce::Justification::centred);

            // Target note arrow + name (right of the note badge), centered vertically
            const juce::String targetDisplay = (noteInfo.valid && noteInfo.targetName != noteInfo.name)
                                                 ? noteInfo.targetName : juce::String();
            if (targetDisplay.isNotEmpty())
            {
                g.setColour (juce::Colour (0xff8bc34a));
                g.setFont (juce::Font (13.0f, juce::Font::bold));
                g.drawText ("> " + targetDisplay,
                            noteAreaX + noteAreaW + 4, badgeY, 80, badgeH,
                            juce::Justification::centredLeft);
            }

            // ---- Cents value (inline, with color) ----
            if (noteInfo.valid)
            {
                const float cents = noteInfo.cents;
                juce::Colour centsCol;
                if (std::abs (cents) < 5.0f)      centsCol = juce::Colour (0xff4caf50);
                else if (std::abs (cents) < 15.0f) centsCol = juce::Colour (0xffcddc39);
                else if (std::abs (cents) < 35.0f) centsCol = juce::Colour (0xffff9800);
                else                                centsCol = juce::Colour (0xffe57373);

                g.setColour (centsCol);
                g.setFont (juce::Font (18.0f, juce::Font::bold));
                const juce::String centsStr = (cents >= 0.0f ? "+" : "")
                    + juce::String (static_cast<int> (std::round (cents))) + "\xc2\xa2";
                g.drawText (centsStr,
                            185, badgeY, 72, badgeH,
                            juce::Justification::centred);
            }

            // ---- Horizontal VU-style tuning meter ----
            {
                const float cents = noteInfo.valid ? noteInfo.cents : 0.0f;
                const int meterLeft  = 275;
                const int meterRight = W - 14;
                const int meterW     = juce::jmax (60, meterRight - meterLeft);
                const int meterY    = badgeY + 2;
                const int meterH    = badgeH - 4;

                if (meterW > 40 && noteInfo.valid)
                {
                    // Meter background: dark rounded track
                    g.setColour (juce::Colour (0x3322222a));
                    g.fillRoundedRectangle ((float)meterLeft, (float)meterY, (float)meterW, (float)meterH, 4.0f);
                    g.setColour (juce::Colour (0x44444466));
                    g.drawRoundedRectangle ((float)meterLeft, (float)meterY, (float)meterW, (float)meterH, 4.0f, 1.0f);

                    const int centerX = meterLeft + meterW / 2;

                    // Center "0" mark — thicker green marker
                    g.setColour (juce::Colour (0xaa4caf50));
                    g.fillRect (centerX - 1, meterY + 2, 2, meterH - 4);

                    // Tick marks at ±25 cents (subtle)
                    g.setColour (juce::Colour (0x44ffffff));
                    const int tick25 = meterW / 4; // 25 cents = 1/4 of the meter when range is ±50
                    g.fillRect (centerX - tick25, meterY + meterH - 8, 1, 6);
                    g.fillRect (centerX + tick25, meterY + meterH - 8, 1, 6);

                    // "0" label centered below meter
                    g.setFont (juce::Font (8.0f));
                    g.setColour (juce::Colour (0xaa4caf50));
                    g.drawText ("0", centerX - 10, meterY + meterH + 1, 20, 10,
                                juce::Justification::centred);

                    // Needle position: mapped to ±50 cents range (full meter width)
                    const float clampedCents = juce::jlimit (-50.0f, 50.0f, cents);
                    const float centsNorm = clampedCents / 50.0f; // normalized to [-1, 1]
                    int needleTarget = centerX + (int)(centsNorm * meterW / 2.0f);
                    needleTarget = juce::jlimit (meterLeft + 3, meterLeft + meterW - 3, needleTarget);

                    // Smooth interpolation for slow, fluid animation
                    const float animAlpha = 0.12f; // slower for a polished feel
                    lastNeedleX = lastNeedleX + (needleTarget - lastNeedleX) * animAlpha;

                    // Needle color based on absolute cents value
                    juce::Colour needleCol;
                    if (std::abs (cents) < 5.0f)      needleCol = juce::Colour (0xff4caf50);
                    else if (std::abs (cents) < 15.0f) needleCol = juce::Colour (0xffcddc39);
                    else if (std::abs (cents) < 30.0f) needleCol = juce::Colour (0xffff9800);
                    else                                needleCol = juce::Colour (0xffe57373);

                    // Draw the VU needle as a small diamond marker
                    const int nX = (int)juce::jlimit ((float)meterLeft + 3, (float)(meterLeft + meterW - 3), lastNeedleX);
                    juce::Path needle;
                    needle.addTriangle ((float)nX, (float)(meterY + 2),
                                        (float)(nX - 4), (float)(meterY + meterH - 2),
                                        (float)(nX + 4), (float)(meterY + meterH - 2));
                    g.setColour (needleCol);
                    g.fillPath (needle);

                    // Small white dot at the needle tip
                    g.setColour (juce::Colours::white);
                    g.fillEllipse ((float)nX - 2.0f, (float)(meterY + meterH / 2) - 2.0f, 4.0f, 4.0f);
                }
            }
        }

        // === Plot area: pitch curves ===
        g.saveState();
        g.reduceClipRegion (plotArea);

        // Base grid (C2..C6)
        g.setColour (kGrid);
        const float refFreqs[] = { 65.4f, 130.8f, 261.6f, 523.3f, 1046.5f };
        for (int i = 0; i < 5; ++i)
        {
            const float y = plotArea.getY() + hzToY (refFreqs[i], plotArea.getHeight());
            g.drawHorizontalLine (static_cast<int> (y),
                                  static_cast<float> (plotArea.getX()),
                                  static_cast<float> (plotArea.getRight()));
        }

        // Scale note lines
        g.setColour (kScaleLineColour);
        for (int oct = 2; oct <= 5; ++oct)
        {
            for (int n = 0; n < scaleIntervals.size(); ++n)
            {
                const int semi = scaleIntervals[n];
                if (semi == 0) continue;
                const int midi = (oct + 1) * 12 + semi;
                const float hz = atdsp::midiToHz (static_cast<float> (midi));
                const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                g.drawHorizontalLine (static_cast<int> (y),
                                      static_cast<float> (plotArea.getX()),
                                      static_cast<float> (plotArea.getRight()));
            }
        }

        // Input pitch curve (red)
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

        // Output pitch curve (green)
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
            g.setColour (kOutputColour);
            g.strokePath (p, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Harmony voices (blue)
        {
            const float dx = static_cast<float> (plotArea.getWidth()) / static_cast<float> (historySize - 1);
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
                    if (hz <= 0.0f) { segmentOpen = false; continue; }
                    const float y = plotArea.getY() + hzToY (hz, plotArea.getHeight());
                    if (!segmentOpen) { p.startNewSubPath (x, y); segmentOpen = true; hasAny = true; }
                    else p.lineTo (x, y);
                }
                if (hasAny)
                {
                    g.setColour (kHarmonyColour);
                    g.strokePath (p, juce::PathStrokeType (0.8f));
                }
            }
        }

        g.restoreState();

        // Legend at bottom-right of plot area
        g.setFont (10.0f);
        g.setColour (kInputColour);
        g.drawText ("Input", plotArea.getRight() - 110, plotArea.getBottom() - 18, 50, 14, juce::Justification::centredRight);
        g.setColour (kOutputColour);
        g.drawText ("Output", plotArea.getRight() - 60, plotArea.getBottom() - 18, 50, 14, juce::Justification::centredRight);

        g.setColour (juce::Colours::grey.withAlpha(0.6f));
        g.setFont (11.0f);
        const juce::String modifierName =
#if JUCE_MAC
            "Cmd";
#else
            "Ctrl";
#endif
        g.drawText ("MouseWheel: Scroll | " + modifierName + "+MouseWheel: Zoom",
                    plotArea.getRight() - 210, plotArea.getBottom() - 32, 200, 14, juce::Justification::bottomRight);
    }

    void PitchVisualizer::resized()
    {
        const int headerH = juce::jmin (50, getHeight() / 4);
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
