// PianoKeyboard.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PianoKeyboard.h"
#include "OVTFonts.h"
#include "OVTTheme.h"

namespace ui
{
    const juce::Colour PianoKeyboard::kWhiteKey      = juce::Colour (0xffffffff);
    const juce::Colour PianoKeyboard::kBlackKey      = juce::Colour (0xff1a1a1a);
    const juce::Colour PianoKeyboard::kWhiteKeyScale = juce::Colour (0xffffffff); // Background color no longer used
    const juce::Colour PianoKeyboard::kBlackKeyScale = juce::Colour (0xff1a1a1a); // Background color no longer used
    const juce::Colour PianoKeyboard::kBorder        = juce::Colour (0xff333333);
    const juce::Colour PianoKeyboard::kText          = juce::Colour (0xff000000);
    const juce::Colour kScaleIndicator               = juce::Colour (0xff3399ff); // Light blue from the screenshot

    PianoKeyboard::PianoKeyboard() = default;
    PianoKeyboard::~PianoKeyboard() = default;

    bool PianoKeyboard::isBlackKey (int midi) noexcept
    {
        const int n = ovtdsp::midiToNoteInOctave (midi);
        // 1=C#, 3=D#, 6=F#, 8=G#, 10=A# (the others are white keys).
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    bool PianoKeyboard::isInScale (int midi) const noexcept
    {
        if (scaleIntervals.isEmpty()) return false;
        const int n = ovtdsp::midiToNoteInOctave (midi);
        for (int s : scaleIntervals)
            if (s == n) return true;
        return false;
    }

    juce::Colour PianoKeyboard::getKeyColour (int midi, bool black) const
    {
        const bool inScale = isInScale (midi);
        if (black)
            return inScale ? kBlackKeyScale : kBlackKey;
        return inScale ? kWhiteKeyScale : kWhiteKey;
    }

    float PianoKeyboard::midiToNorm (int midi, int lowestMidi, int highestMidi)
    {
        lowestMidi = juce::jmax (0, juce::jmin (127, lowestMidi));
        highestMidi = juce::jmax (0, juce::jmin (127, highestMidi));
        if (highestMidi < lowestMidi) std::swap (lowestMidi, highestMidi);
        if (highestMidi <= lowestMidi) return 0.0f;

        // Number of white keys in the visible range.
        int numWhite = 0;
        for (int m = lowestMidi; m <= highestMidi; ++m)
            if (! isBlackKey (m)) ++numWhite;
        if (numWhite == 0) return 0.0f;

        // Index (possibly < 0 or > numWhite-1) of the white key closest BELOW `midi`,
        // counted from `lowestMidi`. Counting is done WITHOUT clamping to allow
        // out-of-range extrapolation: notes outside the visible window are then
        // projected outside the drawing area (and clipped afterwards), instead of
        // being stacked on the edges.
        int belowIndex = 0;
        if (midi >= lowestMidi)
        {
            for (int m = lowestMidi; m < midi; ++m)
                if (! isBlackKey (m)) ++belowIndex;
        }
        else
        {
            for (int m = midi; m < lowestMidi; ++m)
                if (! isBlackKey (m)) --belowIndex;
        }

        if (isBlackKey (midi))
        {
            // Black key: boundary between white keys belowIndex-1 and belowIndex
            // (same formula as for in-range notes, but extrapolated).
            return static_cast<float> (belowIndex) / static_cast<float> (numWhite);
        }

        // White key: center of the cell = (index + 0.5) / numWhite.
        return (static_cast<float> (belowIndex) + 0.5f) / static_cast<float> (numWhite);
    }

    float PianoKeyboard::midiToY (int midi) const
    {
        // Piano geometry: same height for all white keys, consistent black keys.
        // t = 0 -> bottom (low notes), t = 1 -> top (high notes).
        const float t = midiToNorm (midi, lowestMidi, highestMidi);
        return getHeight() * (1.0f - t);
    }

    int PianoKeyboard::yToMidi (float y) const
    {
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, y / static_cast<float> (getHeight()));
        // Inverse by scanning: note whose normalized position is closest to t.
        int best = lowestMidi;
        float bestD = 1.0e9f;
        for (int m = lowestMidi; m <= highestMidi; ++m)
        {
            const float d = std::abs (midiToNorm (m, lowestMidi, highestMidi) - t);
            if (d < bestD) { bestD = d; best = m; }
        }
        return best;
    }

    float PianoKeyboard::yToHz (float y) const
    {
        return ovtdsp::midiToHz (static_cast<float> (yToMidi (y)));
    }

    void PianoKeyboard::setRange (int lowest, int highest)
    {
        lowestMidi  = juce::jlimit (0, 127, lowest);
        highestMidi = juce::jlimit (0, 127, highest);
        if (highestMidi < lowestMidi) std::swap (lowestMidi, highestMidi);
        repaint();
    }

    void PianoKeyboard::setScaleIntervals (const juce::Array<int>& intervals)
    {
        scaleIntervals = intervals;
        repaint();
    }

    void PianoKeyboard::setCurrentPitches (float inHz, float outHz)
    {
        currentInputHz = inHz;
        currentOutputHz = outHz;
        repaint();
    }

    void PianoKeyboard::paint (juce::Graphics& g)
    {
        const int W = getWidth();
        const int H = getHeight();
        if (W <= 0 || H <= 0) return;

        // Transparent background.
        g.fillAll (juce::Colours::transparentBlack);

        const float whiteW = static_cast<float> (W);
        const float blackW = whiteW * 0.65f;

        int inMidi = currentInputHz > 0.0f ? static_cast<int>(std::round(ovtdsp::hzToMidiFloat(currentInputHz))) : -1;
        int outMidi = currentOutputHz > 0.0f ? static_cast<int>(std::round(ovtdsp::hzToMidiFloat(currentOutputHz))) : -1;

        juce::Array<int> whiteMidis;
        for (int midi = highestMidi; midi >= lowestMidi; --midi)
            if (! isBlackKey (midi))
                whiteMidis.add (midi);

        juce::Array<float> whiteCenters;
        whiteCenters.ensureStorageAllocated (whiteMidis.size());
        for (int i = 0; i < whiteMidis.size(); ++i)
            whiteCenters.add (midiToY (whiteMidis.getUnchecked (i)));

        juce::Array<juce::Rectangle<float>> whiteRects;
        whiteRects.ensureStorageAllocated (whiteMidis.size());
        juce::HashMap<int, int> whiteIndex;

        // === Step 1: draw the WHITE keys (with text) ===
        for (int i = 0; i < whiteMidis.size(); ++i)
        {
            const int midi = whiteMidis.getUnchecked (i);
            const float center = whiteCenters.getUnchecked (i);
            const float top = (i == 0) ? 0.0f : 0.5f * (whiteCenters.getUnchecked (i - 1) + center);
            const float bottom = (i == whiteMidis.size() - 1) ? static_cast<float> (H)
                                                              : 0.5f * (center + whiteCenters.getUnchecked (i + 1));

            juce::Rectangle<float> keyRect (0.0f, top, whiteW, juce::jmax (1.0f, bottom - top));
            whiteIndex.set (midi, i);
            whiteRects.add (keyRect);

            // Standard white key
            g.setColour (kWhiteKey);
            g.fillRect (keyRect);
            
            // Input/Output highlighting
            if (midi == inMidi && midi == outMidi)
            {
                juce::ColourGradient mixGrad(juce::Colour(0xffe91e63).withAlpha(0.5f), keyRect.getX(), keyRect.getY(),
                                             juce::Colour(0xff00e676).withAlpha(0.5f), keyRect.getRight(), keyRect.getY(), false);
                g.setGradientFill(mixGrad);
                g.fillRect(keyRect);
            }
            else if (midi == inMidi)
            {
                g.setColour (juce::Colour(0xffe91e63).withAlpha(0.5f));
                g.fillRect(keyRect);
            }
            else if (midi == outMidi)
            {
                g.setColour (juce::Colour(0xff00e676).withAlpha(0.5f));
                g.fillRect(keyRect);
            }

            // Borders (gray to separate white keys)
            g.setColour (kBorder);
            g.drawRect (keyRect, 1.0f);

            // Scale indicator on the right
            if (isInScale(midi))
            {
                g.setColour (kScaleIndicator);
                g.fillRect (whiteW - 4.0f, keyRect.getY(), 4.0f, keyRect.getHeight());
            }

            // Label: note name (C, D, E, F, G, A, B) + octave.
            // Show labels only when keys are tall enough (>= 20px) to avoid clutter.
            const int note = ovtdsp::midiToNoteInOctave (midi);
            const int oct  = ovtdsp::midiToOctave (midi);
            if (keyRect.getHeight() >= 20.0f)
            {
                g.setColour (kText.withAlpha(0.8f));
                g.setFont (ovt::fontPianoKey());
                const juce::String noteName = juce::String (ovtdsp::noteInOctaveName (note));
                const bool isC = (note == 0);
                const juce::String label = isC ? (noteName + " " + juce::String (oct))
                                               : noteName;
                g.drawText (label,
                            1.0f, keyRect.getY() + keyRect.getHeight() * 0.5f - 7.0f, whiteW - 8.0f, 14.0f,
                            juce::Justification::centredRight);
            }
        }

        // === Step 2: draw the BLACK keys on top ===
        for (int midi = highestMidi; midi >= lowestMidi; --midi)
        {
            if (! isBlackKey (midi)) continue;

            int aboveWhite = midi + 1;
            while (aboveWhite <= highestMidi && isBlackKey (aboveWhite))
                ++aboveWhite;

            int belowWhite = midi - 1;
            while (belowWhite >= lowestMidi && isBlackKey (belowWhite))
                --belowWhite;

            if (! whiteIndex.contains (aboveWhite) || ! whiteIndex.contains (belowWhite)) continue;

            const int aboveIdx = whiteIndex [aboveWhite];
            const int belowIdx = whiteIndex [belowWhite];
            const auto& aboveRect = whiteRects.getUnchecked (aboveIdx);
            const auto& belowRect = whiteRects.getUnchecked (belowIdx);

            const float boundary = belowRect.getY();
            float keyH = juce::jmin (aboveRect.getHeight(), belowRect.getHeight()) * 0.65f;
            keyH = juce::jmax (2.0f, keyH);
            float keyTop = boundary - keyH * 0.5f;
            keyTop = juce::jlimit (0.0f, static_cast<float> (H) - keyH, keyTop);

            juce::Rectangle<float> keyRect (0.0f, keyTop, blackW, keyH);

            // Drop shadow
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRect(keyRect.translated(2.0f, 2.0f));

            // Black key
            juce::ColourGradient grad (kBlackKey.brighter(0.2f), 0.0f, keyTop,
                                       kBlackKey.darker(0.3f), blackW, keyTop, false);
            g.setGradientFill (grad);
            
            // Rounded corners on the right
            g.fillRoundedRectangle (keyRect, 2.0f);
            
            // Input/Output highlighting
            if (midi == inMidi && midi == outMidi)
            {
                juce::ColourGradient mixGrad(juce::Colour(0xffe91e63).withAlpha(0.6f), keyRect.getX(), keyRect.getY(),
                                             juce::Colour(0xff00e676).withAlpha(0.6f), keyRect.getRight(), keyRect.getY(), false);
                g.setGradientFill(mixGrad);
                g.fillRoundedRectangle(keyRect, 2.0f);
            }
            else if (midi == inMidi)
            {
                g.setColour (juce::Colour(0xffe91e63).withAlpha(0.6f));
                g.fillRoundedRectangle(keyRect, 2.0f);
            }
            else if (midi == outMidi)
            {
                g.setColour (juce::Colour(0xff00e676).withAlpha(0.6f));
                g.fillRoundedRectangle(keyRect, 2.0f);
            }

            // Border
            g.setColour (kBorder.darker());
            g.drawRoundedRectangle (keyRect, 2.0f, 1.0f);

            // Scale indicator to the right of the black key
            if (isInScale(midi))
            {
                g.setColour (kScaleIndicator);
                g.fillRect (blackW - 4.0f, keyRect.getY() + 1.0f, 4.0f, keyRect.getHeight() - 2.0f);
            }
        }

        // === Step 3: current pitch cursor (optional) ===
        // Removed because the user prefers highlighting the keys themselves.
    }

    void PianoKeyboard::resized() {}
}



