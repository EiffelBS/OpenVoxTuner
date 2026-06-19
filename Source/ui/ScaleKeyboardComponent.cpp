// ScaleKeyboardComponent.cpp
#include "ScaleKeyboardComponent.h"

namespace ui
{
    // === PianoKeyButton ===

    void PianoKeyButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
    {
        auto bounds = getLocalBounds().toFloat().reduced(0.5f);
        
        // Determine si la note est active (soit par toggle state en mode custom, soit par activeInScale en mode preset)
        bool isActive = getToggleState();
        
        // Couleurs
        juce::Colour baseColour;
        if (isBlack) {
            baseColour = isActive ? juce::Colour(0xff0088ff) : juce::Colour(0xff333333); // Bleu si actif, Gris fonce sinon
        } else {
            baseColour = isActive ? juce::Colour(0xff00aaff) : juce::Colour(0xff777777); // Bleu clair si actif, Gris moyen sinon
        }
        
        if (shouldDrawButtonAsDown) {
            baseColour = baseColour.brighter(0.2f);
        } else if (shouldDrawButtonAsHighlighted) {
            baseColour = baseColour.brighter(0.1f);
        }

        // Dessin de la touche
        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, 2.0f);
        
        // Bordure
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds, 2.0f, 1.0f);
        
        // Effet 3D leger
        if (!isBlack) {
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.2f), 0, 0,
                                                   juce::Colours::transparentWhite, 0, bounds.getHeight() * 0.2f, false));
            g.fillRoundedRectangle(bounds, 2.0f);
        } else {
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.1f), 0, 0,
                                                   juce::Colours::transparentWhite, bounds.getWidth(), 0, false));
            g.fillRoundedRectangle(bounds, 2.0f);
        }
    }

    // === ScaleKeyboardComponent ===
    
    ScaleKeyboardComponent::ScaleKeyboardComponent()
    {
        // Initialisation des notes
        // 0:C, 1:C#, 2:D, 3:D#, 4:E, 5:F, 6:F#, 7:G, 8:G#, 9:A, 10:A#, 11:B
        bool isBlackKey[12] = { false, true, false, true, false, false, true, false, true, false, true, false };
        
        for (int i = 0; i < 12; ++i)
        {
            keys[i].setNoteIndex(i, isBlackKey[i]);
            addAndMakeVisible(keys[i]);
        }
    }

    void ScaleKeyboardComponent::paint (juce::Graphics& g)
    {
        // Fond autour du clavier
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
        g.setColour(juce::Colours::black.withAlpha(0.8f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 4.0f, 1.0f);
    }

    void ScaleKeyboardComponent::resized()
    {
        auto bounds = getLocalBounds().reduced(2); // Padding
        float whiteKeyWidth = bounds.getWidth() / 7.0f;
        float blackKeyWidth = whiteKeyWidth * 0.65f;
        float blackKeyHeight = bounds.getHeight() * 0.6f;
        
        int whiteIndex = 0;
        
        // D'abord on place les touches blanches
        for (int i = 0; i < 12; ++i)
        {
            if (!keys[i].getIsBlack())
            {
                keys[i].setBounds(bounds.getX() + static_cast<int>(whiteIndex * whiteKeyWidth),
                                  bounds.getY(),
                                  static_cast<int>(whiteKeyWidth) + 1, // +1 pour eviter les gaps
                                  bounds.getHeight());
                whiteIndex++;
            }
        }
        
        // Ensuite les touches noires par-dessus
        whiteIndex = 0;
        for (int i = 0; i < 12; ++i)
        {
            if (keys[i].getIsBlack())
            {
                // La touche noire est entre la touche blanche precedente et la suivante
                keys[i].setBounds(bounds.getX() + static_cast<int>(whiteIndex * whiteKeyWidth - blackKeyWidth / 2.0f),
                                  bounds.getY(),
                                  static_cast<int>(blackKeyWidth),
                                  static_cast<int>(blackKeyHeight));
            }
            else
            {
                whiteIndex++;
            }
            
            // On s'assure que les touches noires sont au premier plan
            if (keys[i].getIsBlack()) {
                keys[i].toFront(false);
            }
        }
    }
}
