// OVTFonts.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ovt
{
// Use the system default sans-serif on macOS so the plugin renders
    // with native typography (San Francisco / Helvetica Neue).
    // On Windows, hard-code "Segoe UI" because JUCE's
    // getDefaultSansSerifFontName() resolves to "Verdana" (a much wider
    // face), which would make all text look visibly larger than intended.
    // On macOS, apply a 0.85 scale because San Francisco has a larger
    // x-height than Segoe UI at the same nominal point size.
#if JUCE_MAC
    static const juce::String kTypefaceFamily =
        juce::Font::getDefaultSansSerifFontName();
    static constexpr float kPlatformFontScale = 0.85f;
#else
    static const juce::String kTypefaceFamily = "Segoe UI";
    static constexpr float kPlatformFontScale = 1.0f;
#endif

    /** Create a font with the plugin's standard typeface.
        Using the (family, height, style) constructor guarantees
        the same typeface is used for bold and plain variants.
        A platform-specific scale is applied so that the rendered size
        matches the visual size used on Windows (the reference platform).
        @param size    font size in points
        @param bold    true for bold, false for regular
        @return        juce::Font with consistent typeface */
    inline juce::Font createFont (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (kTypefaceFamily,
                           size * kPlatformFontScale,
                           bold ? juce::Font::bold : juce::Font::plain));
    }

    /** Create a font with the plugin's typeface family at the EXACT given
        point size (no platform scale). Reserved for critical navigation UI
        (the Live/Curve Editor pill switch) that must visually match the
        other custom-painted pill switch (Modern/Transparent), which uses a
        raw point size and therefore renders consistently across platforms.
        Using this for body text, labels or tooltips would re-introduce the
        too-large-macOS-text problem. */
    inline juce::Font createFontRaw (float size, bool bold = false)
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
    inline juce::Font fontPopupMenu()
    {
        // Popup menus (wrench, Curve Editor options, combo dropdowns) bypass
        // the platform scale. JUCE's default menu font is 17pt and uses the
        // system font family directly; on macOS the menu items sit in a
        // dedicated vertical panel that already leaves enough room, and
        // shrinking them with the 0.85 scale used elsewhere makes the items
        // feel cramped relative to the surrounding UI. We still pick the
        // same typeface family as the rest of the plugin for consistency.
        return createFontRaw (17.0f, false);
    }
    inline juce::Font fontToggleButton(){ return createFont (14.0f, false); }
    inline juce::Font fontPianoKey()    { return createFont (11.0f, true);  }
    inline juce::Font fontRuler()       { return createFont (11.0f, false); }
    inline juce::Font fontCurveHelp()   { return createFont (11.0f, false); }
    inline juce::Font fontMeasuresLabel(){ return createFont (11.0f, true); }
}



