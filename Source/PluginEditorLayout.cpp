// PluginEditorLayout.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// The editor's layout arithmetic: resized() and the bottom-band width
// helpers it is built on. Split out of PluginEditor.cpp (step 2 of the
// source split) verbatim; the declarations stay in PluginEditor.h, because
// a member function may be defined in any translation unit.
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "ui/CorrectionModeSwitch.h"
#include "ui/OVTHarmonyLayout.h"
#include "ui/OVTSpineCaption.h"

// How much width a room name needs inside its power toggle, measured in the language currently loaded.
// ebs LookAndFeel draws a ToggleButton as a 16 px tick well, a 6 px gap and centred-left text in
// fontToggleButton() (eiffelbs/LookAndFeel.h drawToggleButton), so the strip is 18 px before the first glyph;
// +2 keeps the last glyph off the frame edge. Measuring here, not hard-coding per locale, is what lets one
// layout serve EN "Lead" / "Harmony" / "Ducking", FR "Lead" / "Harmonie", DE "Lead" / "Harmonie", JA
// "リード" / "ハーモニー" / "ダッキンィ" and ZH "主唱" / "和声" at once. Short names make the folded Reverb room
// narrower than the old long ones were, which is what the measurement reports - no constant is tuned per language.
int OpenVoxTunerAudioProcessorEditor::reverbHeaderNeed (const juce::String& text) const
{
    return 18 + ebs::fontToggleButton().getStringWidth (text) + 2;
}

// The Harmony frame's chrome: the chevron strip on its right edge, its own interior padding on both
// sides and its title spine on the left. One expression, so the three width helpers below differ
// only in the ZONES they carry.
int OpenVoxTunerAudioProcessorEditor::harmonyChromeW() const
{
    return ebs::ExpansibleFrame::chevronWidth() + ebs::ExpansibleFrame::chevronGap()
         + 2 * kHarmonyPad
         + ebs::FrameTitleBand::bandWidth();
}

int OpenVoxTunerAudioProcessorEditor::harmonyFoldedW() const
{
    return harmonyChromeW() + ovt::harmonyMainZoneW();
}

int OpenVoxTunerAudioProcessorEditor::harmonyOpenW() const
{
    return harmonyChromeW() + ovt::harmonyMainZoneW() + kHarmonyZoneGap + ovt::kHarmonyKnobZoneW;
}

int OpenVoxTunerAudioProcessorEditor::harmonyOpenFloorW() const
{
    return harmonyChromeW() + ovt::harmonyMainZoneW() + kHarmonyZoneGap + ovt::kHarmonyKnobZoneFloorW;
}

// The single source of truth for every bottom-row block width. Each frame states the
// width its content asks for and the floor below which its widgets collide. The band
// lays every frame at that width: spare room is left empty and nothing is stretched.
void OpenVoxTunerAudioProcessorEditor::updateBottomBandWidths()
{
    const int chevron = ebs::ExpansibleFrame::chevronWidth() + ebs::ExpansibleFrame::chevronGap();
    // Every frame carries a title spine on its left edge, so the spine width is part of what each block asks for.
    // It is one fixed number (ebs::FrameTitleBand::bandWidth()) for every locale: a long translated name pays in
    // length along the spine, never in width, so adding it here keeps the row model exact in all six languages.
    const int bandW = ebs::FrameTitleBand::bandWidth();
    const int corrBaseW   = kKnobBlockW - 22 - 2 * kCorrPad;
    const int corrClosedW = corrBaseW + 2 * kCorrPad + chevron + bandW;

    // Effects: its width IS the cell count, so it is fixed in both directions. Open shows five
    // columns: Gate/Compressor, Doubler/Formant, Output Limit + meter over De-Esser, Saturation, and Delay. Folded it keeps
    // the first two columns, i.e. Gate, Compressor, Doubler and Formant - the controls you dial while singing. The
    // Output Limit control is in the open zone, so a folded frame does not show the ceiling; that is the accepted
    // cost of swapping Limit and Formant, and it changes no width: the count stays 2 folded / 5 open. The two rooms,
    // their sends and Duck are NOT here; they are the Reverb frame's own columns. The row grows by column only, so
    // each new column costs one kEffectsColW plus gap.
    const int effectW = chevron + 2 * kEffectsPad + bandW
                      + (effectsFrame.expanded() ? 5 : 2) * kEffectsColW
                      + (effectsFrame.expanded() ? 4 : 1) * kEffectsColGap;

    // Reverb: the fold shows the LEAD column only (its switch and the two amounts it sends, Room + Hall), so folded
    // the frame asks for one measured column - the Lead switch name - and open it asks for three: Lead, Harmony,
    // Ducking. Open it is NOT growable: a fixed width keeps every knob under its label. The width comes from the
    // MEASURED switch names through `reverbHeaderNeed` at the real 14 pt toggle font. Measured maxima over all six
    // shipped locales: lead <= 43 px, harmony <= 67 px (Spanish "Armonías"; French and German "Harmonie" 66), so both
    // ride the fixed kEffectsColW = 82 px cell floor and a rename cannot widen the frame in any language: the same
    // locale stays compact and a fully folded row fits the minimum-width client without scrolling.
    const int leadNeed = juce::jmax (kEffectsColW, reverbHeaderNeed (leadReverbEnableButton.getButtonText()));
    const int harmNeed = juce::jmax (kEffectsColW, reverbHeaderNeed (harmonyReverbEnableButton.getButtonText()));
    const int duckNeed = juce::jmax (kEffectsColW, reverbHeaderNeed (duckDepthLabel.getText()));
    const int reverbW = chevron + 2 * kEffectsPad + bandW
                      + (reverbFrame.expanded()
                         ? leadNeed + harmNeed + duckNeed + 2 * kEffectsColGap
                         : leadNeed);

    // Correction re-states its width when the advanced zone opens. The width comes from
    // the content, so the closed block is never squeezed and an open block never shrinks:
    // a squeezed block is exactly why the advanced row looked like it never opened.
    bottomBand.setFrameWidths (&correctionFrame,
                             corrClosedW + (correctionFrame.expanded() ? kAdvancedW : 0),
                             corrClosedW, false);
    // Effects: its width IS the cell count, so it is fixed in both directions.
    bottomBand.setFrameWidths (&effectsFrame, effectW, effectW, false);
    // Reverb: same rule as Effects - pref and floor are the cell count in the current fold state.
    bottomBand.setFrameWidths (&reverbFrame, reverbW, reverbW, false);
    // Root/Scale: pref is the keyboard strip plus padding, the floor is the strip itself. Both carry the spine.
    bottomBand.setFrameWidths (&scaleFrame, kScalePrefW + bandW, kScaleMinW + bandW, false);
    // Harmony folds, and it is the band's only GROWABLE block. Folded, its content is the main zone
    // (enable, type, use-voice, voices, tone, two toggles) and nothing else, so it asks for exactly
    // the MEASURED width of that zone: the frame is never wider than its content, and a long
    // translated type entry widens it instead of truncating. Open, it adds the knob zone the four
    // dials need; spare room lands here (growable) so the dials get the air they need, and the
    // floor keeps two readable dials before the row starts scrolling.
    if (harmonyFrame.expanded())
        bottomBand.setFrameWidths (&harmonyFrame, harmonyOpenW(), harmonyOpenFloorW(), true);
    else
        bottomBand.setFrameWidths (&harmonyFrame, harmonyFoldedW(), harmonyFoldedW(), false);
}

void OpenVoxTunerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // === Top banner (title + hamburger menu) ===
    auto titleArea = bounds.removeFromTop (50);

    // Hamburger menu button in the top-right corner.
    const int menuW = 48;
    auto menuArea = titleArea.removeFromRight (menuW).reduced (6, 10);
    menuButton.setBounds (menuArea);

    // Layout: [buttonA] [morphSlider] [buttonB] [undo] [redo] [menuButton] in
    // the top-right. The global Undo/Redo icon buttons sit between button B
    // and the Options gear.
    const int btnSize = 28;
    const int btnGap = 4;
    const int morphW = 80;

    // Position from right to left: menu, redo, undo, B, morph, A
    int x = menuArea.getX() - btnSize - btnGap; // redo
    redoButton.setBounds (x, 11, btnSize, btnSize);
    x -= btnSize + btnGap;                       // undo
    undoButton.setBounds (x, 11, btnSize, btnSize);
    x -= btnSize + btnGap;                       // buttonB
    buttonB.setBounds (x, 11, btnSize, btnSize);
    x -= morphW + btnGap;
    morphSlider.setBounds (x, 13, morphW, btnSize - 4);
    morphSliderLabel.setBounds (x, 2, morphW, 10);
    x -= btnSize + btnGap;
    buttonA.setBounds (x, 11, btnSize, btnSize);

    // === Plugin Preset selector (separate from Curve Presets) ===
    // Centred in the top banner, between the left edge of the title area and
    // the right-hand control cluster (button A / morph / B / undo / redo /
    // gear). Layout: [<] [Combo: name] [>] [save].
    {
        const int selBtn = 24;
        const int selGap = 4;
        const int comboW = 180;
        const int totalW = selBtn + selGap + comboW + selGap + selBtn + selGap + selBtn;
        const int bannerLeft  = titleArea.getX();
        const int bannerRight = x; // right edge of buttonA cluster
        const int availW = bannerRight - bannerLeft;
        int selX = bannerLeft + juce::jmax (0, (availW - totalW) / 2);
        const int selY = 13;

        presetPrevButton.setBounds (selX, selY, selBtn, selBtn);
        selX += selBtn + selGap;
        presetComboBox.setBounds (selX, selY, comboW, selBtn);
        selX += comboW + selGap;
        presetNextButton.setBounds (selX, selY, selBtn, selBtn);
        selX += selBtn + selGap;
        presetSaveButton.setBounds (selX, selY, selBtn, selBtn);
    }

    // Hide all old top-bar controls (they still work via attachments/handlers).
    latencyModeLabel.setBounds (0, 0, 0, 0);
    latencyModeBox.setBounds (0, 0, 0, 0);
    detectorLabel.setBounds (0, 0, 0, 0);
    detectorBox.setBounds (0, 0, 0, 0);
    bypassButton.setBounds (0, 0, 0, 0);
    midiOutButton.setBounds (0, 0, 0, 0);
    updateButton.setBounds (0, 0, 0, 0);
    debugWindowButton.setBounds (0, 0, 0, 0);

    // === Visualizer (top) and Graphic Editor (middle) ===
    const int pad = 10;
    // Reserve the bottom area for the control blocks (Speed/Amount + Effects +
    // Scale/Keyboard + Harmony). The Correction block no longer needs extra
    // height: its advanced knobs (Flex / Humanize / Vibrato / Attack-Aware) are
    // revealed to the SIDE via the "Advanced" banner, so we keep the bottom
    // strip compact. It no longer pays for any indicator: every activity dot lives inside the knob
    // of the component it reports (ebs::ActivityKnob), so the 12 px a strip used to cost went back
    // to the window - the reserve dropped from 208 to 196 px, and every knob in the row still
    // reaches its full 54 px because the cells no longer share their height with a strip.
    auto centerArea = bounds.removeFromTop (bounds.getHeight() - 196);

    // Toolbar band: the pill and the icon row sit ABOVE the tab viewport, not inside it. The
    // hidden tab bar used to reserve 30 px at the top of the tabbed component, so a taller
    // pill overlapped the canvas; the depth is zeroed in the constructor, so the content fills
    // the whole bounds and the band can be a strip taken off centerArea.
    const int bandH   = 44;   // pill row: a taller pill gives the labels their air
    const int bandGap = 6;    // minimal spacing between the pill and the viewport
    auto bandArea = centerArea.removeFromTop (bandH + bandGap);
    auto tabArea  = centerArea.reduced (pad);
    tabArea.setY (bandArea.getBottom());
    // The viewport sits as close to the top of the frames as the frames sit to the window
    // edge: the same pad gap below the frame row.
    tabArea.setBottom (centerArea.getBottom());
    tabbedComponent.setBounds (tabArea);
    
    // Graphic Mode specific tools aligned over the toolbar band.
    auto toolsArea = bandArea.reduced (pad, 4); // pill centred in the band, icons stay 22 px

    // Live / Curve Editor tab switch, placed on the left of the toolbar row:
    // the shared eiffelbs-ui pill. It measures its own labels (the tab names
    // are already translated), so it asks for the width they need instead of
    // editor guessing it. The pill band is TALLER than the icon row, so the labels
    // get their air and the fixed 22 px icons stay centred inside it.
    if (tabSwitch != nullptr)
    {
        const int switchH = 40;
        const int switchW = tabSwitch->preferredWidth();
        auto switchRect = toolsArea.removeFromLeft(switchW);
        switchRect = switchRect.withSizeKeepingCentre(switchW, switchH);
        tabSwitch->setBounds (switchRect);
        toolsArea.removeFromLeft(6); // gap
    }

    const int iconSize = 22;   // square icon slots: a taller pill band must not stretch them
    auto placeIcon = [&] (juce::Component& btn, bool fromRight)
    {
        auto r = fromRight ? toolsArea.removeFromRight (iconSize)
                           : toolsArea.removeFromLeft  (iconSize);
        btn.setBounds (r.withSizeKeepingCentre (iconSize, iconSize));
    };

    // Toolbar left group (curve-editor mode): standalone transport + measures.
    const bool isStandalone = processorRef.isStandaloneWrapper();
    const int leftGap = 8;
    if (isStandalone)
    {
        // Standalone transport: a "Return to start" (rewind) button placed
        // before the Play/Pause toggle, matching the classic DAW order.
        placeIcon (rewindButton, false);
        toolsArea.removeFromLeft(4);
        placeIcon (playButton, false);
        toolsArea.removeFromLeft(leftGap);
    }
    // "Measures" control (number of measures shown in the curve editor time window).
    // Kept on the toolbar row so it no longer covers the ruler.
    const int measuresLabelW = static_cast<int> (juce::GlyphArrangement::getStringWidth (measuresLabel.getFont(), measuresLabel.getText())) + 10;
    measuresLabel.setBounds (toolsArea.removeFromLeft (measuresLabelW));
    toolsArea.removeFromLeft(4);
    measuresComboBox.setBounds (toolsArea.removeFromLeft (56).withSizeKeepingCentre (56, 24));
    toolsArea.removeFromLeft(leftGap);

    // Standalone tempo field, right after Measures: the two controls the Curve Editor's grid
    // depends on sit together, and the tempo-synced Delay reads the same number. Hidden in a DAW,
    // where the host owns the tempo.
    bpmLabel.setVisible (isStandalone);
    if (isStandalone)
        bpmLabel.setBounds (toolsArea.removeFromLeft (76).withSizeKeepingCentre (76, 24));

    // Toolbar (right to left): Options | Gallery | zoom / scroll / reset | snap / grid / step
    // Options and Gallery are icon-only buttons; leave a clear gap before
    // the reset (X) button so they don't visually touch.
    placeIcon (optionsButton, true);
    toolsArea.removeFromRight(8);
    placeIcon (presetGalleryButton, true);
    toolsArea.removeFromRight(8);

    placeIcon (resetViewButton, true);
    toolsArea.removeFromRight(8);
    placeIcon (scrollDownButton, true);
    toolsArea.removeFromRight(8);
    placeIcon (scrollUpButton, true);
    toolsArea.removeFromRight(8);
    placeIcon (zoomOutButton, true);
    toolsArea.removeFromRight(8);
    placeIcon (zoomInButton, true);
    toolsArea.removeFromRight(8);

    placeIcon (stepModeButton, true);
    toolsArea.removeFromRight(8);
    placeIcon (snapGridButton, true);
    toolsArea.removeFromRight(8);
    placeIcon (snapButton, true);

    // === Bottom bar: blocks ===
    auto bottomArea = bounds.reduced (pad);

    // The bottom row is one ebs::FrameBand. Each block states the width its content asks
    // for and the floor below which its widgets collide; spare room goes only to blocks
    // marked growable, and below the hard floor the row scrolls instead of stretching
    // the last block. The band sets every frame bounds, so no block is placed by hand.
    const int corrBaseW   = kKnobBlockW - 22 - 2 * kCorrPad;   // Speed + Amount content strip

    updateBottomBandWidths();
    bottomBand.setBounds (bottomArea);

    const bool correctionOpen = correctionFrame.expanded();
    // --- Block 2 : Correction - an ebs::ExpansibleFrame; Speed + Amount always visible ---
    // The spine sits at the left edge of the frame body, inside its rounded corner; every content cell starts right of it.
    auto corrArea = correctionFrame.interior();
    correctionTitleBand.setBounds (corrArea.getX(), corrArea.getY(), ebs::FrameTitleBand::bandWidth(), corrArea.getHeight());
    corrArea.removeFromLeft (ebs::FrameTitleBand::bandWidth());
    auto b2 = corrArea.reduced (kCorrPad);   // frame-local coordinates

    // The chevron strip is excluded by interior(), so the content area IS the interior:
    // the base knobs on the left, the advanced zone in what is left over.
    auto content = b2;

    // The folded zone is hidden, not painted over: an invisible child of the frame takes
    // no clicks and leaves its cell empty - the Effects frame third column rule.
    vibratoPreserveSlider.setVisible (correctionOpen);
    vibratoPreserveLabel.setVisible (correctionOpen);
    humanizeSlider.setVisible (correctionOpen);
    humanizeLabel.setVisible (correctionOpen);
    voiceTypeBox.setVisible (correctionOpen);
    voiceTypeLabel.setVisible (correctionOpen);
    formantStrategyBox.setVisible (correctionOpen);
    formantStrategyLabel.setVisible (correctionOpen);

    // Speed + Amount: the two prominent big knobs on the left. A strip is
    // reserved at the bottom of this area for the Modern/Transparent graphic
    // switch, which shrinks the two knobs vertically.
    const int baseContentW = juce::jmin (corrBaseW, content.getWidth());  // width reserved for Speed + Amount
    auto baseArea = content.removeFromLeft (baseContentW);
    auto advancedArea = content;                              // Flex / Humanize / Vibrato / Attack

    // Reserve the switch strip at the bottom of the base area, then let the
    // knobs fill the remaining top space (so they get smaller).
    const int switchH   = 30;
    const int switchGap = 8;
    auto switchRect = baseArea.removeFromBottom (switchH);
    baseArea.removeFromBottom (switchGap);

    const int knobPadding = 6;
    const int bigHalf = (baseArea.getWidth() - knobPadding) / 2;

    auto bSpeed = baseArea.removeFromLeft (bigHalf);
    speedLabel.setBounds (bSpeed.removeFromTop (18));
    speedSlider.setBounds (bSpeed);
    baseArea.removeFromLeft (knobPadding);

    auto bAmount = baseArea;
    amountLabel.setBounds (bAmount.removeFromTop (18));
    amountSlider.setBounds (bAmount);

    // Modern / Transparent studio-console switch, spanning the base area width.
    if (modeSwitch != nullptr)
        modeSwitch->setBounds (switchRect);

    // Advanced correction knobs (revealed when the block is expanded). They are
    // compact, laid out in a 2x2 grid that FILLS the advanced area width (no empty
    // side margins, no overlap), and have NO value textbox (value shows in the
    // popup while dragging), so the "Advanced" zone stays small.
    const int advLabelH = 13;
    const int advGapX   = 8;    // gap between the two columns
    const int advGapY   = 6;    // gap between the rows
    const int knobMax   = 52;   // cap so advanced knobs stay smaller than Speed/Amount
    const int advInsetR = 5;    // pixels to shift advanced content away from main block

    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> cell)
    {
        l.setBounds (cell.removeFromTop (advLabelH));
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setPopupDisplayEnabled (true, false, this);
        const int d = juce::jmin (cell.getWidth() - 4, cell.getHeight() - 4, knobMax);
        const int kx = cell.getX() + (cell.getWidth() - d) / 2;
        const int ky = cell.getY() + (cell.getHeight() - d) / 2;
        s.setBounds (kx, ky, d, d);
    };

    if (correctionOpen && advancedArea.getWidth() > 4)
    {
        // 2026-07-27 (Layout update with Voice Type selector): the advanced
        // area now uses 2 rows: row 1 = Vibrato + Humanize side-by-side
        // (2 small knobs centered), row 2 = Voice Type combo box (full width).
        // This matches the proposal in docs/voice-type-feasibility-report.md.

        // Shift all content 5px right to create breathing room from the main block.
        advancedArea.removeFromLeft (advInsetR);

        // Row 1: Vibrato + Humanize (2 columns).
        const int advRow1H = (int)((advancedArea.getHeight() - 2 * advGapY) * 0.42f);
        auto row1Area = advancedArea.removeFromTop (advRow1H);
        advancedArea.removeFromTop (advGapY);

        const int colW = (row1Area.getWidth() - advGapX) / 2;
        auto vibCell   = row1Area.removeFromLeft (colW);
        row1Area.removeFromLeft (advGapX);
        auto humanCell = row1Area;

        placeKnob (vibratoPreserveSlider, vibratoPreserveLabel, vibCell);
        placeKnob (humanizeSlider,         humanizeLabel,        humanCell);
        // 2026-07-24 (Deprecation): FlexTune and Attack-Aware knobs
        // are no longer placed in the visible UI. Their slider/label
        // members are still kept (in PluginEditor.h) for future
        // re-implementation, but the placeKnob calls are commented
        // out so the two visible knobs (Vibrato, Humanize) take the
        // full advanced area in a 1x2 column.

        // Row 2: Voice Type label + combo. Narrower combos centered in the
        // advanced area so they stay away from the main (Speed/Amount) block.
        const int comboW  = 130;   // narrower than full area width (180)
        const int comboL  = (int)((advancedArea.getWidth() - comboW) / 2); // centered
        advancedArea.removeFromLeft (comboL);
        voiceTypeLabel.setBounds (advancedArea.removeFromTop (advLabelH));
        voiceTypeBox.setBounds (advancedArea.removeFromTop (22).withWidth (comboW));
        // Adjust remaining area width to keep row 3 aligned.
        advancedArea.removeFromLeft (advancedArea.getWidth() - comboW - comboL);
        advancedArea.removeFromTop (advGapY);

        // Row 3: Formant Strategy label + combo.
        formantStrategyLabel.setBounds (advancedArea.removeFromTop (advLabelH));
        formantStrategyBox.setBounds (advancedArea.removeFromTop (22).withWidth (comboW));
    }
    else
    {
        // Collapsed: hide the advanced knobs so they don't overlap other blocks.
        // 2026-07-24 (Deprecation): FlexTune bounds set to (0,0,0,0) is
        // no longer needed (not visible), but kept for safety.
        flexTuneSlider.setBounds (0, 0, 0, 0);   flexTuneLabel.setBounds (0, 0, 0, 0);
        humanizeSlider.setBounds (0, 0, 0, 0);   humanizeLabel.setBounds (0, 0, 0, 0);
        vibratoPreserveSlider.setBounds (0, 0, 0, 0); vibratoPreserveLabel.setBounds (0, 0, 0, 0);
        // 2026-07-24 (Deprecation): Attack-Aware bounds no longer needed
        // (not visible), but kept for safety.
        attackReleaseSlider.setBounds (0, 0, 0, 0);       attackAwareButton.setBounds (0, 0, 0, 0);
        // Voice Type is hidden when Advanced is collapsed
        voiceTypeBox.setBounds (0, 0, 0, 0);
        voiceTypeLabel.setBounds (0, 0, 0, 0);
        // Formant Strategy is hidden when Advanced is collapsed
        formantStrategyBox.setBounds (0, 0, 0, 0);
        formantStrategyLabel.setBounds (0, 0, 0, 0);
    }

    // --- Block 4 : Effects - an ebs::ExpansibleFrame of two columns, growing by column ---
    // Column 1 shows Noise Gate over Upward Compressor, column 2 Doubler over Formant. Those four cells are what a
    // folded frame keeps. The chevron on the frame's right edge opens the extra columns (Output Limit + meter over
    // De-Esser, then Saturation): the frame WIDENS and pushes the Reverb frame registered right of it, which is
    // what the host re-layout on onExpandedChanged is for. Every cell is a power toggle carrying its name on top and
    // a knob below; the knob size comes from the row height, so it stays at its full 54 px: adding a COLUMN is how
    // this block grows, never a row (a third row would shrink every knob). The value shows in the popup display
    // while dragging (no textbox). Limit and Formant exchanged places by design: the dial you shape a voice with
    // stays on screen folded, the ceiling control goes to the open zone.
    // The two rooms and their send room are NOT in this block - they live in the Reverb frame.

    const bool effectsOpen = effectsFrame.expanded();

    // The folded columns' widgets are hidden, not painted over: an invisible child of
    // the frame takes no clicks and leaves its cell empty. Only the Effects zone folds here -
    // the two rooms and their send room are gated by reverbOpen in the Reverb block below.
    // Formant is a folded-visible cell now that it sits under the Doubler in column 2: dialling the vowel shape is
    // lead-side work you do while singing, so its toggle and knob stay visible in BOTH states.
    formantEnableButton.setVisible (true);
    formantSlider.setVisible (true);
    // The Output Limit + meter control lives in the open-only column 3, so a folded frame does not show the ceiling.
    // That is the accepted consequence of swapping it with Formant: folding hides the CONTROL, never the processing -
    // limiter_enable keeps guarding every sample and outputMeter keeps reading the same state; an invisible meter
    // bar updates exactly as a visible one does.
    limiterEnableButton.setVisible (effectsOpen);
    outputTrimSlider.setVisible (effectsOpen);
    outputMeter.setVisible (effectsOpen);
    deessEnableButton.setVisible (effectsOpen);
    deessAmountSlider.setVisible (effectsOpen);
    saturationEnableButton.setVisible (effectsOpen);
    saturationDriveSlider.setVisible (effectsOpen);
    saturationDriveLabel.setVisible (effectsOpen);
    saturationToneSlider.setVisible (effectsOpen);
    saturationToneLabel.setVisible (effectsOpen);
    // The Delay column lives in the open zone like Saturation: folding hides its
    // controls, never the processing - delay_enable keeps gating the ping-pong bus.
    delayEnableButton.setVisible (effectsOpen);
    delayDivisionLabel.setVisible (effectsOpen);
    delayDivisionKnob.setVisible (effectsOpen);
    delayMixSlider.setVisible (effectsOpen);
    delayMixLabel.setVisible (effectsOpen);
    delayFeedbackSlider.setVisible (effectsOpen);
    delayFeedbackLabel.setVisible (effectsOpen);

    auto effArea = effectsFrame.interior();
    effectsTitleBand.setBounds (effArea.getX(), effArea.getY(), ebs::FrameTitleBand::bandWidth(), effArea.getHeight());
    effArea.removeFromLeft (ebs::FrameTitleBand::bandWidth());
    auto b4 = effArea.reduced (kEffectsPad);   // frame-local coordinates
    const int effectPowerH  = 18;
    const int rowGap        = 8;
    const int numEffectRows = 2;
    // Five columns when open (Gate/Compress, Doubler/Formant, Output Limit+meter over De-Esser, Saturation, Delay)
    // and two when folded. This count drives effectColW, so it has to match the number of columns
    // placed below: asking for 4 widths while placing 5 columns leaves the last column a
    // negative-width rectangle, and that cell vanishes without any warning in the layout log.
    const int numEffectCols = effectsOpen ? 5 : 2;
    // No activity strip any more: every dot in this block lives inside the knob of the component
    // it reports, so the column area is pure cell area and the freed height goes back to the
    // window (see the bottom reserve in resized()).
    auto rowsArea = b4;
    const int rowH          = (rowsArea.getHeight() - (numEffectRows - 1) * rowGap) / numEffectRows;
    const int effKnobD      = juce::jlimit (30, 54, rowH - effectPowerH - 2);
    const int effectColW    = (rowsArea.getWidth() - (numEffectCols - 1) * kEffectsColGap) / numEffectCols;

    // A caption in a HALF column (36 px) cannot hold every shipped word at the shared 11 pt label
    // font: measured, the widest pair needs 27 px (EN/DE), 23 (FR), 31 (JA), 13 (ZH) and 44 (ES,
    // "Realimentación") at the 8 pt floor. So a caption shrinks its own glyphs to fit, the frame
    // spine's rule, from fontLabelSmallPlain() down to that floor - and the Spanish pair is the reason
    // the ES wording uses the audio jargon "Feedback", which this file already keeps untranslated
    // for Lead, Ducking and Gate. Weight is REGULAR here like every non-caps caption in a frame.
    auto fitLabelToWidth = [&] (juce::Label& label, int width)
    {
        const juce::String text = label.getText();

        for (int size = 11; size >= 8; --size)
        {
            const juce::Font font = ovt::createFont (static_cast<float> (size), false);
            if (juce::GlyphArrangement::getStringWidth (font, text) <= static_cast<float> (width))
            {
                label.setFont (font);
                return;
            }
        }

        label.setFont (ovt::createFont (8.0f, false));
    };

    // Cell with a power toggle: toggle on top, knob centred below.
    auto placeToggledKnob = [&] (juce::Rectangle<int> cell, auto& toggle, auto& knob)
    {
        toggle.setBounds (cell.removeFromTop (effectPowerH));
        const int kx = cell.getX() + juce::jmax (0, (cell.getWidth() - effKnobD) / 2);
        knob.setBounds (kx, cell.getY() + 2, effKnobD, effKnobD);
    };

    // Cell with a label on top and a knob below. The shared send room has no power switch,
    // so its name sits in the same strip a toggle occupies in every other cell: the column
    // reads as three cells of the same shape, not as a strip of text bolted onto knobs.
    const int sendLabelH = 14;
    auto placeLabelledKnob = [&] (juce::Rectangle<int> cell, juce::Label& label, juce::Slider& knob)
    {
        label.setBounds (cell.removeFromTop (sendLabelH));
        // The diameter is bounded by the CELL, height and width both: a narrow zone (Harmony's knob
        // columns at the frame's floor) must shrink the dial rather than let it overlap the one
        // beside it - the same "no control is ever dropped" rule the Delay column follows.
        const int widest = juce::jmax (30, juce::jmin (effKnobD, cell.getWidth()));
        const int d = juce::jlimit (30, widest, cell.getHeight() - 2);
        const int kx = cell.getX() + juce::jmax (0, (cell.getWidth() - d) / 2);
        knob.setBounds (kx, cell.getY() + 2, d, d);
    };

    // Column 1 : Noise Gate on top, Upward Compressor below. Both stay visible when folded - they
    // are the two input-stage processors you dial while singing, and this is what the frame keeps
    // when its chevron folds everything to the right away.
    auto col1 = rowsArea.removeFromLeft (effectColW);
    rowsArea.removeFromLeft (kEffectsColGap);
    placeToggledKnob (col1.removeFromTop (rowH), noiseGateEnableButton, noiseGateThresholdSlider);
    col1.removeFromTop (rowGap);
    placeToggledKnob (col1, upwardCompEnableButton, upwardCompAmountSlider);

    // Column 2 : Doubler on top, Formant below. The Doubler cell is built exactly like
    // its siblings (power toggle carrying the name, knob below): double_enable is a real parameter,
    // so switching off greys the knob and stops the copy without clearing double_amount. It lives
    // here because it is an effect of the processed signal, not a harmony voice; nothing Harmony
    // does touches this cell. Formant goes under it: shaping the vowel is lead-side work you dial while singing, so
    // both cells stay visible in BOTH states and the folded frame stays at two columns.
    auto col2 = rowsArea.removeFromLeft (effectColW);
    rowsArea.removeFromLeft (kEffectsColGap);
    placeToggledKnob (col2.removeFromTop (rowH), doubleEnableButton, doubleAmountSlider);
    col2.removeFromTop (rowGap);
    placeToggledKnob (col2, formantEnableButton, formantSlider);

    if (effectsOpen)
    {
        // Column 3 : Output Limit + meter on top, De-Esser below. This cell is the half of the swapped pair that
        // moved out of column 2: folding hides its CONTROL (see the visibility block above), never the processing -
        // limiter_enable keeps guarding every sample behind the fold. It is the only cell in this block that carries
        // something under its knob: the output meter. Rows are frozen at two and the frame grows by column only, so
        // the strip is paid for by THIS cell's knob - the one knob in the block that is smaller than its siblings. A
        // 5 px bar plus one 8 pt dB line needs 16 px; the knob takes the rest of the cell. output_trim reads in dB
        // (-12 to +12), not percent, and limiter_enable switches the peak guard on.
        auto col3 = rowsArea.removeFromLeft (effectColW);
        rowsArea.removeFromLeft (kEffectsColGap);
        auto limitCell = col3.removeFromTop (rowH);
        limiterEnableButton.setBounds (limitCell.removeFromTop (effectPowerH));
        const int meterStripH = 16;
        const int limitKnobD  = juce::jmax (30, limitCell.getHeight() - 2 - meterStripH);
        const int kx = limitCell.getX() + juce::jmax (0, (limitCell.getWidth() - limitKnobD) / 2);
        outputTrimSlider.setBounds (kx, limitCell.getY() + 2, limitKnobD, limitKnobD);

        const int meterTop = outputTrimSlider.getBounds().getBottom() + 1;
        outputMeter.setBounds (limitCell.getX(), meterTop, limitCell.getWidth(),
                               juce::jmax (0, limitCell.getBottom() - meterTop));

        // The De-Esser cell is one switch (deess_enable) and one Intensity knob: that knob drives the detector
        // threshold and the depth cap together, and the band centre follows voice_type, so there is nothing else to
        // place in this cell.
        col3.removeFromTop (rowGap);
        placeToggledKnob (col3, deessEnableButton, deessAmountSlider);
        // The one column that holds two voices: the ceiling it contains is the master's, the
        // De-Esser under it is the lead's, so its caption names both and its dot reads the stage
        // that LEAVES the column (the master blend) - one dot, one truth.

        // Column 4 : Saturation. One power switch carrying the name and two labelled
        // knobs (Drive, Warmth) stacked in one cell - the same shape the send-room column uses in
        // the Reverb frame. It pays for its second knob inside its own column, so rows stay frozen
        // at two everywhere in the block.
        auto col4 = rowsArea.removeFromLeft (effectColW);   // fixed width: a fifth column follows
        rowsArea.removeFromLeft (kEffectsColGap);
        saturationEnableButton.setBounds (col4.removeFromTop (effectPowerH));
        const int satCellH = (col4.getHeight() - rowGap) / 2;
        placeLabelledKnob (col4.removeFromTop (satCellH), saturationDriveLabel, saturationDriveSlider);
        col4.removeFromTop (rowGap);
        placeLabelledKnob (col4, saturationToneLabel, saturationToneSlider);

        // Column 5, the rightmost : Delay. One power switch carrying the name, then the MIX knob at
        // the block's standard size - it carries the component's activity dot, so it is the dial the
        // eye lands on - and, on ONE line under it, the two secondary controls as small knobs:
        // Division (detented, the ping length synced to the host tempo) and Feedback. The detented
        // knob replaced a combo strip, which is what frees the height the Mix knob needed: the combo
        // plus its caption cost 36 px, the pair of small knobs costs 50 and gives Mix its full 54.
        auto col5 = rowsArea;   // the last column takes whatever is left
        delayEnableButton.setBounds (col5.removeFromTop (effectPowerH));

        // The Mix cell is exactly the standard cell (caption + a full-size dial + its 2 px of air),
        // and the two small dials take the space that is left, CENTRED in it: a row flush against the
        // frame's bottom edge reads as a footnote, while the same row at mid-distance from the Mix
        // knob reads as the second half of one component.
        const int smallLabelH = 12;   // one fitted caption: 8-11 pt, so 12 px carries every locale
        const int smallKnobD  = 36;   // the biggest a half-column dial can be (36 px wide cells)
        const int smallRowH   = smallLabelH + smallKnobD + 2;

        auto mixCell = col5.removeFromTop (sendLabelH + 54 + 2);
        placeLabelledKnob (mixCell, delayMixLabel, delayMixSlider);

        auto smallRow = col5.withSizeKeepingCentre (col5.getWidth(), smallRowH);

        // The two small cells: a caption each, fitted to the half column they are given (the shared
        // 11 pt label does not fit 36 px in every language), then the dial centred under it.
        auto placeSmallLabelledKnob = [&] (juce::Rectangle<int> cell, juce::Label& label, juce::Slider& knob)
        {
            auto labelStrip = cell.removeFromTop (smallLabelH);
            fitLabelToWidth (label, labelStrip.getWidth());
            label.setBounds (labelStrip);

            const int d = juce::jlimit (24, effKnobD,
                                        juce::jmin (cell.getWidth(), cell.getHeight()));
            const int kx = cell.getX() + juce::jmax (0, (cell.getWidth() - d) / 2);
            knob.setBounds (kx, cell.getY() + 1, d, d);
        };

        auto divCell = smallRow.removeFromLeft ((smallRow.getWidth() - 2) / 2);
        smallRow.removeFromLeft (2);
        placeSmallLabelledKnob (divCell, delayDivisionLabel, delayDivisionKnob);
        placeSmallLabelledKnob (smallRow, delayFeedbackLabel, delayFeedbackSlider);
    }

    // --- Block 5 : Reverb - an ebs::ExpansibleFrame right of Effects ---
    // Two SHARED ROOMS and two voice columns. Folded, the frame shows the LEAD column only: the Lead switch carrying
    // its name, then the two amounts that voice sends - Room (short) above Hall (long), each room named VERTICALLY at
    // its dial's left in the frame spines' typography (ovt::VerticalCaption). Open, the HARMONY column joins it (same
    // shape) and Ducking stands alone in column 3 with its caption ABOVE the dial, because the ducker is a return
    // ramp rather than an amount a power switch sends. Each column is one voice, each row inside it is one room: the
    // knob IS the send, so there is no separate send control and the dry voice is never attenuated.
    // The height works at the shipped band height: the band row gives every frame 156 px of content, and header (18)
    // + label (14) + knob (<= 49) + gap (8) + label (14) + knob (<= 49) fits inside it.
    // The columns are sized by the MEASURED switch name: a ToggleButton draws its tick well then centred-left text
    // with no wrap, so a long voice name inside one 74 px cell would clip. Measured at the real 14 pt toggle font
    // every shipped locale rides that floor: lead <= 43 px, harmony <= 67 px (Spanish "Armonías"), so the folded
    // frame measures the SAME width in all six languages. reverbHeaderNeed() + updateBottomBandWidths() keep
    // proving that offline instead of trusting an eyeball.
    // The ducker lives here because it is a property of the two rooms' return, not of a voice: one depth for both
    // rooms, keyed by the corrected lead in the processor (never by the noise gate). The frame WIDENS when its
    // chevron opens, pushing Scale and Harmony right - which is what updateBottomBandWidths() and resized() on
    // onExpandedChanged are for.
    const bool reverbOpen = reverbFrame.expanded();

    // Same rule as Effects: the Harmony column and the ducker are hidden when folded, not painted over.
    harmonyReverbEnableButton.setVisible (reverbOpen);
    harmonyRoomSlider.setVisible (reverbOpen);
    harmonyRoomLabel.setVisible (reverbOpen);
    harmonyHallSlider.setVisible (reverbOpen);
    harmonyHallLabel.setVisible (reverbOpen);
    duckDepthSlider.setVisible (reverbOpen);
    duckDepthLabel.setVisible (reverbOpen);

    auto rvArea = reverbFrame.interior();
    reverbTitleBand.setBounds (rvArea.getX(), rvArea.getY(), ebs::FrameTitleBand::bandWidth(), rvArea.getHeight());
    rvArea.removeFromLeft (ebs::FrameTitleBand::bandWidth());
    auto rv = rvArea.reduced (kEffectsPad);   // frame-local coordinates

    const int headerH = effectPowerH;   // the switch strip, same height as every other cell's

    // A knob whose caption runs VERTICALLY along its left edge, in the frame spines' typography (tracked
    // capitals, 50 % alpha, glyphs shrinking to fit). The caption spends width the cell already has and
    // hands the 14 px it used to occupy above the dial back to the dial: 156 - header - gap leaves 65 px
    // per amount cell, so the dial reaches the shared 54 px instead of the 49 px a top caption allowed.
    auto placeSpineKnob = [&] (juce::Rectangle<int> cell, ovt::VerticalCaption& caption, juce::Slider& knob)
    {
        caption.setBounds (cell.removeFromLeft (ovt::VerticalCaption::captionWidth()));
        cell.removeFromLeft (2);   // clear space between the rotated glyphs and the dial
        const int widest = juce::jmax (30, juce::jmin (effKnobD, cell.getWidth()));
        const int d = juce::jlimit (30, widest, cell.getHeight() - 2);
        const int kx = cell.getX() + juce::jmax (0, (cell.getWidth() - d) / 2);
        knob.setBounds (kx, cell.getY() + juce::jmax (0, (cell.getHeight() - d) / 2), d, d);
    };

    // One voice column: the power switch carrying the voice name, then its two send amounts, each with the
    // room it feeds set vertically at its left. The switch keeps the knobs' values (greyed while off), so
    // switching a voice off and back on needs no re-dial. BOTH amounts carry the voice's activity dot: they
    // send the same voice to two rooms, so a dot on one knob only would read as "this other amount carries
    // no signal".
    auto placeVoiceColumn = [&] (juce::Rectangle<int> col, juce::ToggleButton& toggle,
                                 ovt::VerticalCaption& roomLabel, juce::Slider& roomKnob,
                                 ovt::VerticalCaption& hallLabel, juce::Slider& hallKnob)
    {
        toggle.setBounds (col.removeFromTop (headerH));

        // Two captioned cells share what is left: header (18) + room cell + gap + hall cell = 156 px.
        const int cellH = (col.getHeight() - rowGap) / 2;
        placeSpineKnob (col.removeFromTop (cellH), roomLabel, roomKnob);
        col.removeFromTop (rowGap);
        placeSpineKnob (col, hallLabel, hallKnob);
    };

    const int leadNeed = reverbHeaderNeed (leadReverbEnableButton.getButtonText());
    const int harmNeed = reverbHeaderNeed (harmonyReverbEnableButton.getButtonText());

    if (! reverbOpen)
    {
        // Folded: ONE column, the Lead one. updateBottomBandWidths() writes exactly its measured width, so
        // rv.getWidth() is the column width and nothing sits to the right.
        const int foldedW = juce::jmax (kEffectsColW, leadNeed);
        auto rvCol = rv.removeFromLeft (foldedW);
        placeVoiceColumn (rvCol, leadReverbEnableButton, leadRoomLabel, leadRoomSlider,
                          leadHallLabel, leadHallSlider);
    }
    else
    {
        // Open: three columns - Lead, Harmony, Ducking. Each voice column sizes by its measured switch name so a
        // header never clips; Ducking takes the remainder, one labelled knob with no switch (depth 0 is off) centred
        // in its own cell.
        const int leadW = juce::jmax (kEffectsColW, leadNeed);
        const int harmW = juce::jmax (kEffectsColW, harmNeed);

        auto rvCol1 = rv.removeFromLeft (leadW);   rv.removeFromLeft (kEffectsColGap);
        auto rvCol2 = rv.removeFromLeft (harmW);   rv.removeFromLeft (kEffectsColGap);
        auto duckCol = rv;   // the last column takes whatever is left

        placeVoiceColumn (rvCol1, leadReverbEnableButton, leadRoomLabel, leadRoomSlider,
                          leadHallLabel, leadHallSlider);
        placeVoiceColumn (rvCol2, harmonyReverbEnableButton, harmonyRoomLabel, harmonyRoomSlider,
                          harmonyHallLabel, harmonyHallSlider);
        // The Ducking column is a voice column without a switch: its caption takes the strip the
        // switch occupies in the two voice columns (the same 18 px, so the name sits on the same
        // line as LEAD and HARMONY) and its dial takes the FIRST amount cell, so it lands on the
        // same line as the two ROOM dials. Before, the caption strip was 14 px and the dial was
        // top-aligned under it: the Ducking dial floated 7 px above the row of knobs the rest of
        // the band reads on.
        const int duckCellH = (duckCol.getHeight() - headerH - rowGap) / 2;
        duckDepthLabel.setBounds (duckCol.removeFromTop (headerH));

        {
            const auto duckKnobCell = duckCol.removeFromTop (duckCellH);
            const int d = juce::jlimit (30, juce::jmin (effKnobD, duckKnobCell.getWidth()),
                                       duckKnobCell.getHeight() - 2);
            duckDepthSlider.setBounds (duckKnobCell.getX() + juce::jmax (0, (duckKnobCell.getWidth() - d) / 2),
                                       duckKnobCell.getY() + juce::jmax (0, (duckKnobCell.getHeight() - d) / 2),
                                       d, d);
        }

    }

    // Harmony controls block (rightmost block), an ebs::ExpansibleFrame.
    // Split into left/right columns FIRST, then fill each column.
    // Left: enable button + type + voices + toggles - visible in BOTH states.
    // Right: Volume / Blend / Formant / Attack knobs - the folded zone, hidden not painted over.
    {
        const bool harmonyOpen = harmonyFrame.expanded();

        // A folded zone is a setVisible() away from gone, never an overwritten one: an
        // invisible child of the frame takes no clicks and leaves its cell empty.
        harmonyGainLabel.setVisible   (harmonyOpen);
        harmonyGainSlider.setVisible  (harmonyOpen);
        harmonyBlendLabel.setVisible  (harmonyOpen);
        harmonyBlendSlider.setVisible (harmonyOpen);
        harmonyFormantLabel.setVisible(harmonyOpen);
        harmonyFormantSlider.setVisible(harmonyOpen);
        harmonyAttackLabel.setVisible (harmonyOpen);
        harmonyAttackSlider.setVisible(harmonyOpen);

        // interior() excludes the chevron strip, so what is left IS the content area.
        auto harmArea = harmonyFrame.interior();
        harmonyTitleBand.setBounds (harmArea.getX(), harmArea.getY(), ebs::FrameTitleBand::bandWidth(), harmArea.getHeight());
        harmArea.removeFromLeft (ebs::FrameTitleBand::bandWidth());
        auto h = harmArea.reduced (kHarmonyPad);

        // A power switch is sized from the NAME IT DRAWS, not from the text it carries: the name is
        // painted in the smallcaps caption typography (ovt::PowerToggleCaps), and capitals are WIDER
        // than a mixed-case name at the toggle font - "Follow Lead" is 52 px mixed at 14 pt but
        // 58.5 px in smallcaps at 11 pt - so a button sized from the mixed-case face clipped its own
        // name (the last letter of FOLLOW LEAD and GAIN MATCH was cut in half).
        auto powerButtonWidth = [&] (juce::Button& b) -> int
        {
            const float radius    = 18.0f * 0.3f;
            const float iconWidth = radius * 2.0f;
            float textW = 0.0f;
            if (b.getButtonText().isNotEmpty())
                textW = ovt::spine::trackedLength (ovt::spine::capsOf (b.getButtonText()),
                                                   ovt::spine::kCaptionFontSize,
                                                   ovt::spine::kCaptionLetterSpacing) + 8.0f;
            return static_cast<int> (std::ceil (iconWidth + textW + 4.0f));
        };

        // Open, the MAIN ZONE takes exactly the width its widest control needs and the knob zone
        // takes everything left over: the main zone is a fixed set of controls, so a percentage
        // split would either waste width on it (and starve the dials) or squeeze the type combo.
        // Closed, the interior IS the main zone: nothing is removed from it, so rightCol comes back
        // empty and the knob block below is skipped by its own width guard - the same rule the
        // correction frame's advanced zone uses.
        const int mainZoneW = juce::jmin (ovt::harmonyMainZoneW(), h.getWidth());
        auto leftCol = harmonyOpen ? h.removeFromLeft (mainZoneW) : h;
        if (harmonyOpen)
            h.removeFromLeft (kHarmonyZoneGap);
        auto rightCol = h;

        // --- Left column: enable button, type, voices, toggles. These six rows are the frame's main zone and they
        // stay visible in BOTH states. Each row height is fixed by what it carries; the height the band row gives this
        // frame that they do not use becomes the SPACING between them, so the column reads as five breathing groups
        // instead of one glued stack: a glued stack hides which control belongs to which group, and the leftover
        // height sat empty at the bottom edge. The spacing scales with the window, never below 3 px, capped at 12.
        const int voiceRowH  = 18;   // enable strip, same height as every power strip in the band
        const int typeRowH   = 26;   // the harmony type combo
        const int pillRowH   = 22;   // "use voice" caption + pill + tone colour swatch
        const int selectorH  = 28;   // voices + tone selectors
        const int toggleH    = 20;   // follow-lead and gain-match power buttons, twice
        const int mainRowsH  = voiceRowH + typeRowH + pillRowH + selectorH + 2 * toggleH;
        const int mainGap    = juce::jlimit (3, 12, (h.getHeight() - mainRowsH) / 5);

        auto enableRow = leftCol.removeFromTop (voiceRowH);
        const int harmonyEnableW = powerButtonWidth (harmonyEnableButton);
        harmonyEnableButton.setBounds (enableRow.removeFromLeft (harmonyEnableW).reduced (0, 0));

        leftCol.removeFromTop (mainGap);
        harmonyTypeBox.setBounds (leftCol.removeFromTop (typeRowH));

        leftCol.removeFromTop (mainGap);
        auto uvRow = leftCol.removeFromTop (pillRowH);
        // Caption first, then the pill: the switch is sized to its own sweet spot.
        const int uvLabelW = useVoiceLabel.getFont().getStringWidth (useVoiceLabel.getText()) + 4;
        useVoiceLabel.setBounds (uvRow.removeFromLeft (uvLabelW).reduced (0, 3));
        useVoiceButton.setBounds (uvRow.removeFromLeft (44));
        harmonyToneColorLabel.setBounds (uvRow.removeFromRight(34));

        leftCol.removeFromTop (mainGap);
        auto selectorRow = leftCol.removeFromTop (selectorH);
        auto selectorBox = selectorRow.removeFromLeft (juce::jmax (90, leftCol.getWidth() - 34));
        shiftedVoicesBox.setBounds (selectorBox.reduced (0, 2));
        harmonyToneBox.setBounds (selectorBox.reduced (0, 2));
        harmonyToneColorSlider.setBounds (selectorRow.withSizeKeepingCentre (28, 24));

        leftCol.removeFromTop (mainGap);
        auto flRow = leftCol.removeFromTop (toggleH);
        const int flW = powerButtonWidth (harmonyFollowLeadButton);
        harmonyFollowLeadButton.setBounds (flRow.removeFromLeft (flW).reduced (0, 1));

        leftCol.removeFromTop (mainGap);
        auto gmRow = leftCol.removeFromTop (toggleH);
        const int gmW = powerButtonWidth (harmonyGainMatchButton);
        harmonyGainMatchButton.setBounds (gmRow.removeFromLeft (gmW).reduced (0, 1));

        // --- Right column: Volume + Blend side-by-side, Formant + Attack below. This whole zone is the folded part
        // of the frame, so it is laid out only when open and only when there is room for two knob columns; closed,
        // rightCol is empty by construction. The cells go through placeLabelledKnob(), the SAME helper the Reverb
        // sends and Saturation use: 14 px label strip, 2 px gap, one knob centred inside effKnobD, two rows split by
        // the band's own rowGap of 8 px. That is what puts a Harmony dial on the same grid as an Effects dial - the
        // same interior top, the same row split, the same diameter - where this frame used to run its own private row
        // formula (label 14 + knob capped at 44, gap 6) and landed its knobs at heights no other frame in the band uses.
        // Harmony is the band's only growable frame, so spare width lands HERE and the dials take it: the zone's
        // preferred width holds two full-size 54 px knobs with air around them (ovt::kHarmonyKnobZoneW), and its floor
        // (ovt::kHarmonyKnobZoneFloorW) is the narrowest that still keeps two readable dials before the row scrolls.
        // The Doubler knob lives in the Effects block (row 2), not here: it doubles the dry lead, not the harmony voices.
        if (harmonyOpen && rightCol.getWidth() > 4)
        {
            const int harmRowH = (rightCol.getHeight() - rowGap) / 2;
            // The two dial columns are separated by the zone's own gap (kHarmonyKnobGap, wider than
            // the band's 8 px row gap): each dial is centred in its column, so the clearance shows
            // up twice between them and the pair reads as two controls instead of one cluster.
            auto knobCol1 = rightCol.removeFromLeft ((rightCol.getWidth() - ovt::kHarmonyKnobGap) / 2);
            rightCol.removeFromLeft (ovt::kHarmonyKnobGap);
            auto knobCol2 = rightCol;

            placeLabelledKnob (knobCol1.removeFromTop (harmRowH), harmonyGainLabel, harmonyGainSlider);
            placeLabelledKnob (knobCol2.removeFromTop (harmRowH), harmonyBlendLabel, harmonyBlendSlider);

            knobCol1.removeFromTop (rowGap);
            knobCol2.removeFromTop (rowGap);

            // Formant stays under the Volume knob (left column) and Harmony Attack - the per-voice fade-in - sits
            // directly under Blend (right column), as requested.
            placeLabelledKnob (knobCol1, harmonyFormantLabel, harmonyFormantSlider);
            placeLabelledKnob (knobCol2, harmonyAttackLabel, harmonyAttackSlider);
        }


    }

    // --- Block 1 : Key, Scale, Keyboard (middle) ---
    auto scaleArea = scaleFrame.getLocalBounds();
    scaleTitleBand.setBounds (scaleArea.getX(), scaleArea.getY(), ebs::FrameTitleBand::bandWidth(), scaleArea.getHeight());
    scaleArea.removeFromLeft (ebs::FrameTitleBand::bandWidth());
    auto b1 = scaleArea.reduced(10);

    auto topRow = b1.removeFromTop(44); // Key + Scale row (20 label + 24 combobox)

    // Left: Key (Root) - only ever shows up to 2 chars (e.g. "C", "C#"), so keep it narrow.
    auto bKey = topRow.removeFromLeft(56);
    keyLabel.setBounds(bKey.removeFromTop(20));
    keyBox.setBounds(bKey);

    topRow.removeFromLeft(10); // spacer

    // Middle: Scale Box (fixed narrower width)
    int desiredScaleWidth = juce::jmin(140, topRow.getWidth());
    auto bScale = topRow.removeFromLeft(desiredScaleWidth);
    scaleLabel.setBounds(bScale.removeFromTop(20));
    scaleBox.setBounds(bScale);

    b1.removeFromTop(6); // spacer

    // Keyboard (mini-piano) - placed right after Key / Scale (they are related).
    scaleKeyboard.setBounds(b1.removeFromTop(55).withSizeKeepingCentre(180, 55));

    b1.removeFromTop(6); // spacer

    // --- Key/Scale DETECTION ---
    // In ARA mode the host owns the tonality; hide the whole detector section.
    const bool isARA = processorRef.isBoundToARA_custom();

    if (!isARA)
    {
        // Line 1: a power-style toggle (on/off) + the "Key/Scale Detection" label.
        // "Manual" is no longer a source choice: when the toggle is off you set the
        // key/scale by hand; when on, the source combo below drives detection.
        {
            auto detRow = b1.removeFromTop (22);
            const int pw = 18;   // match Gate / Reverb power buttons
            keyDetectPowerButton.setBounds (detRow.removeFromLeft (pw));
            detRow.removeFromLeft (6);
            keySourceLabel.setBounds (detRow);
        }
        const bool detectOn = keyDetectPowerButton.getToggleState();
        // The group combo is only relevant when detection is on AND the source is OpenVoxKey.
        companionGroupBox.setVisible (detectOn && keySourceBox.getSelectedItemIndex() == 1);

        if (detectOn)
        {
            // Line 2: detection source combo, directly under the power/label row
            // (no gap) and kept at the same 24px height as the Key/Scale combos.
            auto srcRow = b1.removeFromTop (24);
            if (companionGroupBox.isVisible())
            {
                const int grpW = 46;
                auto grpRow = srcRow.removeFromRight (grpW);
                keySourceBox.setBounds (srcRow);
                companionGroupLabel.setBounds (0, 0, 0, 0);   // mini combo: hide the label
                companionGroupBox.setBounds (grpRow);
            }
            else
            {
                keySourceBox.setBounds (srcRow);
                companionGroupLabel.setBounds (0, 0, 0, 0);
                companionGroupBox.setBounds (0, 0, 0, 0);
            }
        }
        else
        {
            keySourceBox.setBounds (0, 0, 0, 0);
            companionGroupLabel.setBounds (0, 0, 0, 0);
            companionGroupBox.setBounds (0, 0, 0, 0);
        }
    }
    else
    {
        // ARA mode: hide all Key/Scale Detection controls
        keyDetectPowerButton.setBounds (0, 0, 0, 0);
        keySourceLabel.setBounds (0, 0, 0, 0);
        keySourceBox.setBounds (0, 0, 0, 0);
        companionGroupLabel.setBounds (0, 0, 0, 0);
        companionGroupBox.setBounds (0, 0, 0, 0);
    }
    // Every bottom-row block is a component now: two ExpansibleFrames (Correction,
    // Effects) and two FramedBodies (Root/Scale, Harmony). A re-layout therefore moves
    // components only. JUCE repaints a resized component in full, and a moved one in both
    // its old and new areas, which is exactly what the global repaint() used to compensate
    // for (the painted block regions of UI.13). Nothing painted moves any more.
}

