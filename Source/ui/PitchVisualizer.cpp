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

            // ---- LED-grid VU meter (professional DAW style) ----
            {
                const float cents = noteInfo.valid ? noteInfo.cents : 0.0f;

                // LED grid configuration
                // Segments per side (excluding center), total = 2*segmentsPerSide + 1
                constexpr int segmentsPerSide = 8;
                constexpr int totalSegments = 2 * segmentsPerSide + 1;
                constexpr float centsPerSegment = 50.0f / (float)segmentsPerSide; // ~6.25

                // Fixed meter width for readability, centered in available space
                constexpr int meterFixedW = 300;
                const int availW = W - 285; // space from note+cents area
                const int meterW = juce::jmin (meterFixedW, juce::jmax (160, availW));
                const int meterLeft = (W - meterW) / 2 + 20; // centered
                const int meterY = badgeY + 3;
                const int meterH = badgeH - 6;

                if (meterW > 80 && noteInfo.valid)
                {
                    // Segment dimensions
                    constexpr int segGap = 2;
                    constexpr int segCount = totalSegments;
                    const int totalGaps = (segCount - 1) * segGap;
                    const int segW = (meterW - totalGaps) / segCount;
                    const int segWClamped = juce::jmax (6, segW);
                    g.setColour (juce::Colour (0x2215151b));
                    g.fillRoundedRectangle ((float)meterLeft - 4, (float)meterY - 2,
                                            (float)(meterW + 8), (float)(meterH + 4), 4.0f);

                    // Static color gradient: index from center (0 = center, +1..8 right, -1..-8 left)
                    // Colors mapped by absolute position from center
                    static const juce::Colour segmentColors[segmentsPerSide + 1] = {
                        juce::Colour (0xff4caf50), // 0: center (will be overridden by distinctive style)
                        juce::Colour (0xff66bb6a), // 1: green
                        juce::Colour (0xff8bc34a), // 2: light green
                        juce::Colour (0xffcddc39), // 3: yellow-green
                        juce::Colour (0xffffeb3b), // 4: yellow
                        juce::Colour (0xffff9800), // 5: orange
                        juce::Colour (0xffff5722), // 6: deep orange
                        juce::Colour (0xfff44336), // 7: red
                        juce::Colour (0xffd32f2f)  // 8: dark red
                    };

                    for (int i = 0; i < segCount; ++i)
                    {
                        // Map segment index to offset from center
                        int offset = i - segmentsPerSide; // -8..0..+8
                        int absOffset = std::abs (offset);
                        float threshold = (float)absOffset * centsPerSegment;

                        bool isActive = false;

                        if (offset == 0) {
                            // Center segment: only active when very close to 0 cents
                            isActive = (std::abs (cents) < centsPerSegment * 0.5f && std::abs (cents) >= 0.001f);
                        } else if (offset > 0) {
                            // Right side: only active when cents is POSITIVE
                            // Segment at position 'offset' lights up when
                            // cents >= its threshold
                            isActive = (cents >= threshold);
                        } else {
                            // Left side (offset < 0): only active when cents is NEGATIVE
                            isActive = (cents <= -threshold);
                        }

                        // Compute segment X position
                        int segX = meterLeft + i * (segWClamped + segGap);
                        int segY = meterY;
                        int sH = meterH;

                        // Center segment gets extra height (10% taller)
                        if (absOffset == 0) {
                            segY -= 1;
                            sH += 2;
                        }

                        juce::Colour baseCol;
                        if (absOffset == 0) {
                            // Center segment: distinctive bright cyan/blue
                            baseCol = juce::Colour (0xff00bcd4);
                        } else {
                            int colorIdx = juce::jmin (absOffset, segmentsPerSide);
                            baseCol = segmentColors[colorIdx];
                        }

                        if (isActive) {
                            // Active: full brightness, slight glow via brighter variant
                            g.setColour (baseCol);
                            g.fillRoundedRectangle ((float)segX, (float)segY,
                                                    (float)segWClamped, (float)sH, 2.5f);
                            // Inner bright highlight
                            g.setColour (baseCol.brighter (0.3f).withAlpha (0.5f));
                            g.fillRoundedRectangle ((float)(segX + 1), (float)(segY + 1),
                                                    (float)(segWClamped - 2), (float)(sH - 3), 1.5f);

                            // Center segment extra glow
                            if (absOffset == 0) {
                                g.setColour (juce::Colour (0xffffffff).withAlpha (0.3f));
                                g.fillRoundedRectangle ((float)(segX + 1), (float)(segY + 1),
                                                        (float)(segWClamped - 2), (float)(sH - 2), 2.0f);
                            }
                        } else {
                            // Inactive: dim with low opacity
                            g.setColour (baseCol.withAlpha (0.12f));
                            g.fillRoundedRectangle ((float)segX, (float)segY,
                                                    (float)segWClamped, (float)sH, 2.5f);
                        }

                        // Segment border for definition
                        g.setColour (juce::Colour (0x22ffffff));
                        g.drawRoundedRectangle ((float)segX, (float)segY,
                                                (float)segWClamped, (float)sH, 2.5f, 0.5f);

                        // Center segment: distinctive border
                        if (absOffset == 0) {
                            g.setColour (juce::Colour (0x8800bcd4));
                            g.drawRoundedRectangle ((float)(segX - 1), (float)(segY - 1),
                                                    (float)(segWClamped + 2), (float)(sH + 2), 3.0f, 1.5f);
                        }
                    }

                    // "0" label centered below the grid
                    const int centerSegX = meterLeft + segmentsPerSide * (segWClamped + segGap);
                    g.setFont (juce::Font (8.0f));
                    g.setColour (juce::Colour (0xaa00bcd4));
                    g.drawText ("0", centerSegX - 12, meterY + meterH + 2, 24, 10,
                                juce::Justification::centred);
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
