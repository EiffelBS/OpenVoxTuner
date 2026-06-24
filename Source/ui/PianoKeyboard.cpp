// PianoKeyboard.cpp
// Implementation du clavier de piano vertical.
//
// Convention : axe Y vertical, notes graves en BAS, notes aigues en HAUT.
// - Touches blanches : C, D, E, F, G, A, B (largeur 100% de la largeur).
// - Touches noires  : C#, D#, F#, G#, A# (largeur 60%, hauteur 60%).
// Les touches de la gamme courante sont mises en surbrillance.

#include "PianoKeyboard.h"

namespace ui
{
    const juce::Colour PianoKeyboard::kWhiteKey      = juce::Colour (0xffffffff);
    const juce::Colour PianoKeyboard::kBlackKey      = juce::Colour (0xff1a1a1a);
    const juce::Colour PianoKeyboard::kWhiteKeyScale = juce::Colour (0xffffffff); // On n'utilise plus la couleur de fond
    const juce::Colour PianoKeyboard::kBlackKeyScale = juce::Colour (0xff1a1a1a); // On n'utilise plus la couleur de fond
    const juce::Colour PianoKeyboard::kBorder        = juce::Colour (0xff333333);
    const juce::Colour PianoKeyboard::kText          = juce::Colour (0xff000000);
    const juce::Colour kScaleIndicator               = juce::Colour (0xff3399ff); // Bleu clair de la capture

    PianoKeyboard::PianoKeyboard() = default;
    PianoKeyboard::~PianoKeyboard() = default;

    bool PianoKeyboard::isBlackKey (int midi) noexcept
    {
        const int n = atdsp::midiToNoteInOctave (midi);
        // 1=C#, 3=D#, 6=F#, 8=G#, 10=A# (les autres sont blanches).
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    bool PianoKeyboard::isInScale (int midi) const noexcept
    {
        if (scaleIntervals.isEmpty()) return false;
        const int n = atdsp::midiToNoteInOctave (midi);
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

    float PianoKeyboard::midiToY (int midi) const
    {
        // On reserve un petit padding en haut et en bas.
        const int range = juce::jmax (1, highestMidi - lowestMidi);
        const float t = static_cast<float> (midi - lowestMidi) / static_cast<float> (range);
        // Inverser : bas = graves, haut = aigus.
        return getHeight() - t * getHeight();
    }

    int PianoKeyboard::yToMidi (float y) const
    {
        const int range = juce::jmax (1, highestMidi - lowestMidi);
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, y / static_cast<float> (getHeight()));
        return lowestMidi + static_cast<int> (std::round (t * range));
    }

    float PianoKeyboard::yToHz (float y) const
    {
        return atdsp::midiToHz (static_cast<float> (yToMidi (y)));
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

        // Fond transparent.
        g.fillAll (juce::Colours::transparentBlack);

        const float whiteW = static_cast<float> (W);
        const float blackW = whiteW * 0.65f;

        int inMidi = currentInputHz > 0.0f ? static_cast<int>(std::round(atdsp::hzToMidiFloat(currentInputHz))) : -1;
        int outMidi = currentOutputHz > 0.0f ? static_cast<int>(std::round(atdsp::hzToMidiFloat(currentOutputHz))) : -1;

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

        // === Etape 1 : dessine les touches BLANCHES (avec texte) ===
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

            // Touche blanche standard
            g.setColour (kWhiteKey);
            g.fillRect (keyRect);
            
            // Surbrillance Input/Output
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

            // Bordures (grise pour separer les touches blanches)
            g.setColour (kBorder);
            g.drawRect (keyRect, 1.0f);

            // Indicateur de gamme (Scale Indicator) a droite
            if (isInScale(midi))
            {
                g.setColour (kScaleIndicator);
                g.fillRect (whiteW - 4.0f, keyRect.getY(), 4.0f, keyRect.getHeight());
            }

            // Label : nom de note (C, D, E, F, G, A, B) + octave.
            const int note = atdsp::midiToNoteInOctave (midi);
            const int oct  = atdsp::midiToOctave (midi);
            // On n'affiche le label que pour les C (plus lisible).
            if (note == 0)
            {
                g.setColour (kText.withAlpha(0.8f));
                g.setFont (juce::Font (11.0f, juce::Font::bold));
                // On decale legerement a gauche pour ne pas ecraser l'indicateur bleu
                g.drawText ("C " + juce::String (oct),
                            1.0f, keyRect.getY() + keyRect.getHeight() * 0.5f - 7.0f, whiteW - 8.0f, 14.0f,
                            juce::Justification::centredRight);
            }
        }

        // === Etape 2 : dessine les touches NOIRES par-dessus ===
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

            // Ombre portee
            g.setColour(juce::Colours::black.withAlpha(0.6f));
            g.fillRect(keyRect.translated(2.0f, 2.0f));

            // Touche noire
            juce::ColourGradient grad (kBlackKey.brighter(0.2f), 0.0f, keyTop,
                                       kBlackKey.darker(0.3f), blackW, keyTop, false);
            g.setGradientFill (grad);
            
            // Coins arrondis a droite
            g.fillRoundedRectangle (keyRect, 2.0f);
            
            // Surbrillance Input/Output
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

            // Bordure
            g.setColour (kBorder.darker());
            g.drawRoundedRectangle (keyRect, 2.0f, 1.0f);

            // Indicateur de gamme (Scale Indicator) a droite de la touche noire
            if (isInScale(midi))
            {
                g.setColour (kScaleIndicator);
                g.fillRect (blackW - 4.0f, keyRect.getY() + 1.0f, 4.0f, keyRect.getHeight() - 2.0f);
            }
        }

        // === Etape 3 : curseur du pitch courant (optionnel) ===
        // Supprime car l'utilisateur prefere la surbrillance des touches elles-memes.
    }

    void PianoKeyboard::resized() {}
}
