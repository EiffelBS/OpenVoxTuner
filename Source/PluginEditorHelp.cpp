// PluginEditorHelp.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Out-of-line definitions of OpenVoxTunerAudioProcessorEditor::HelpOverlayComponent
// (the class declaration itself stays in PluginEditor.h): the keyboard-shortcuts
// overlay's constructor, its click handler and its painting. The overlay is owned
// by the editor, so toggleHelpOverlay() - which touches the editor state - stays in
// PluginEditor.cpp. Extracted verbatim (step 1 of the source split).

#include "PluginEditor.h"

#include "ui/OVTFonts.h"
#include "ui/OVTLanguages.h"

// === HelpOverlayComponent ===
OpenVoxTunerAudioProcessorEditor::HelpOverlayComponent::HelpOverlayComponent (
    OpenVoxTunerAudioProcessorEditor& o)
    : owner (o)
{
    setInterceptsMouseClicks (true, false);
    setVisible (false);
}

void OpenVoxTunerAudioProcessorEditor::HelpOverlayComponent::mouseDown (const juce::MouseEvent&)
{
    owner.helpOverlayVisible = false;
    setVisible (false);
}

void OpenVoxTunerAudioProcessorEditor::HelpOverlayComponent::paint (juce::Graphics& g)
{
    // Semi-transparent dark backdrop
    g.setColour (juce::Colour (0xcc000000));
    g.fillRect (getLocalBounds());

    const int centerX = getWidth() / 2;
    const int centerY = getHeight() / 2;

    // Shortcuts list
    struct Shortcut { const char* key; const char* descKey; };
    const Shortcut shortcuts[] = {
        { "Mouse Wheel",            ovt::Keys::kHelpMouseWheel },
        { "Ctrl / Cmd + Wheel",     ovt::Keys::kHelpCtrlWheel },
        { "Click + Drag",           ovt::Keys::kHelpClickDrag },
        { "Double-click",           ovt::Keys::kHelpDoubleClick },
        { "Right-click / Alt+Click",ovt::Keys::kHelpRightClick },
        { "Ctrl / Cmd + C",         ovt::Keys::kHelpCopy },
        { "Ctrl / Cmd + V",         ovt::Keys::kHelpPaste },
        { "Delete / Backspace",     ovt::Keys::kHelpDelete },
        { "Ctrl / Cmd + Z",         ovt::Keys::kHelpUndo },
        { "Ctrl / Cmd + Y",         ovt::Keys::kHelpRedo },
        { "Ctrl / Cmd + Shift + Z", ovt::Keys::kHelpRedo },
        { "?",                      ovt::Keys::kHelpToggleHelp },
    };
    const int numShortcuts = 12;
    const int rowsPerCol = 6;
    const int numCols = (numShortcuts + rowsPerCol - 1) / rowsPerCol; // 2 columns

    // Measure the widest key / description so the box is sized to fit the
    // current language and nothing is ever truncated (long keys like
    // "Right-click / Alt+Click" previously overflowed the fixed 94px column).
    const juce::Font legendFont = ovt::fontLegend();
    const juce::Font titleFont  = ovt::fontLabel();
    const int padX = 24;           // inner horizontal padding
    const int colGap = 24;         // gap between the key and its description
    const int colGapBetween = 32;  // gap between the two columns
    int maxKeyW = 0, maxDescW = 0;
    for (int i = 0; i < numShortcuts; ++i)
    {
        maxKeyW  = juce::jmax (maxKeyW,  static_cast<int> (juce::GlyphArrangement::getStringWidth (legendFont, shortcuts[i].key)));
        maxDescW = juce::jmax (maxDescW, static_cast<int> (juce::GlyphArrangement::getStringWidth (legendFont, ovt::tr (shortcuts[i].descKey))));
    }
    const int colW = maxKeyW + colGap + maxDescW;
    const int contentW = numCols * colW + (numCols - 1) * colGapBetween;

    // Also account for the title / close-hint width so long translations do
    // not get clipped either.
    const int titleW = static_cast<int> (juce::GlyphArrangement::getStringWidth (titleFont, ovt::tr (ovt::Keys::kHelpTitle)));
    const int closeW = static_cast<int> (juce::GlyphArrangement::getStringWidth (ovt::fontLegendHint(), ovt::tr (ovt::Keys::kHelpCloseHint)));

    // Final panel dimensions: leave room for inner padding, the header band,
    // the divider and the close hint.
    const int boxW = juce::jmax (contentW + padX * 2,
                                 juce::jmax (titleW + 64, juce::jmax (closeW + 48, 360)));
    const int boxH = 380;
    const int boxX = centerX - boxW / 2;
    const int boxY = centerY - boxH / 2;

    // Panel background
    g.setColour (ebs::bgPanel());
    g.fillRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 8.0f);
    g.setColour (ebs::accentSoft());
    g.drawRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 8.0f, 1.5f);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (titleFont);
    g.drawText (ovt::tr(ovt::Keys::kHelpTitle), boxX + 16, boxY + 12, boxW - 32, 24,
                juce::Justification::centred);

    // Scope subtitle: the listed shortcuts are Curve-Editor scoped (they only
    // fire when the Curve Editor has keyboard focus), so state that explicitly
    // instead of implying they are global.
    g.setColour (ebs::textDim());
    g.setFont (ovt::fontLegendHint());
    g.drawText (ovt::tr(ovt::Keys::kHelpScope), boxX + 16, boxY + 38, boxW - 32, 16,
                juce::Justification::centred);

    // Divider
    g.setColour (ebs::accentSoft());
    g.fillRect ((float) (boxX + 16), (float) (boxY + 58), (float) (boxW - 32), 1.0f);

    g.setFont (legendFont);
    int sy = boxY + 70;
    for (int i = 0; i < numShortcuts; ++i)
    {
        const int col = i / rowsPerCol;
        const int row = i % rowsPerCol;
        const int x = boxX + padX + col * (colW + colGapBetween);
        const int y = sy + row * 22;

        g.setColour (ebs::accent());
        g.drawText (shortcuts[i].key, x, y, maxKeyW, 18, juce::Justification::centredLeft);
        g.setColour (ebs::text());
        g.drawText (ovt::tr(shortcuts[i].descKey), x + maxKeyW + colGap, y, maxDescW, 18,
                    juce::Justification::centredLeft);
    }

    // Close hint
    g.setColour (ebs::textDim());
    g.setFont (ovt::fontLegendHint());
    g.drawText (ovt::tr(ovt::Keys::kHelpCloseHint), boxX, boxY + boxH - 24, boxW, 20,
                juce::Justification::centred);
}
