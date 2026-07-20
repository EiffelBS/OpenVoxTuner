// OVTFonts.h
// Centralized font definitions for OpenVoxTuner.
// All fonts in the plugin MUST be created through these helpers
// to guarantee consistent typeface and prevent random bold/plain switching.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovt
{
    // Default typeface family used across the entire plugin.
    // Changing this single constant updates all fonts at once.
    static const juce::String kTypefaceFamily = "Segoe UI";

    /** Create a font with the plugin's standard typeface.
        Using the (family, height, style) constructor guarantees
        the same typeface is used for bold and plain variants.
        @param size    font size in points
        @param bold    true for bold, false for regular
        @return        juce::Font with consistent typeface */
    inline juce::Font createFont (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (kTypefaceFamily, size,
                           bold ? juce::Font::bold : juce::Font::plain));
    }

    // === Named font constants for common sizes ===
    // Use these throughout the plugin for maximum consistency.

    inline juce::Font fontTitle()       { return createFont (26.0f, true);  }
    inline juce::Font fontVersion()     { return createFont (12.0f, false); }
    inline juce::Font fontNoteLarge()   { return createFont (24.0f, true);  }
    inline juce::Font fontTarget()      { return createFont (13.0f, true);  }
    inline juce::Font fontCents()       { return createFont (18.0f, true);  }
    inline juce::Font fontMeter0()      { return createFont (8.0f,  false); }
    inline juce::Font fontOctaveLabel() { return createFont (9.0f,  false); }
    inline juce::Font fontYAxis()       { return createFont (8.0f,  false); }
    inline juce::Font fontLegend()      { return createFont (11.0f, false); }
    inline juce::Font fontLegendHint()  { return createFont (11.0f, false); }
    inline juce::Font fontReadout()     { return createFont (10.0f, true);  }
    inline juce::Font fontLabel()       { return createFont (13.0f, true);  }
    inline juce::Font fontLabelSmall()  { return createFont (11.0f, true);  }
    inline juce::Font fontComboBox()    { return createFont (14.0f, false); }
    inline juce::Font fontTooltip()     { return createFont (13.0f, false); }
    inline juce::Font fontToggleButton(){ return createFont (14.0f, false); }
    inline juce::Font fontPianoKey()    { return createFont (11.0f, true);  }
    inline juce::Font fontRuler()       { return createFont (11.0f, false); }
    inline juce::Font fontCurveHelp()   { return createFont (11.0f, false); }
    inline juce::Font fontMeasuresLabel(){ return createFont (11.0f, true); }
}
