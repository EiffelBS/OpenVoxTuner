// PluginEditorTheme.cpp
// OpenVoxTuner GUI
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.
//
// Theme application: dark/light switching, icon restyling, caption dimming
// and the component sweep that recolours the whole editor. Split out of
// PluginEditor.cpp (step 2 of the source split) verbatim.
#include "PluginEditor.h"

#include "ui/OVTFonts.h"

// File-scope twin of the constructor's inline SVG factory; used when a
// theme switch regenerates icon drawables outside the constructor scope.
static std::unique_ptr<juce::Drawable> makeStateDrawable (const char* svgChars, juce::Colour strokeColor)
{
    auto baseXml = juce::XmlDocument::parse (svgChars);
    if (baseXml == nullptr) return std::make_unique<juce::DrawablePath>();
    auto d = juce::Drawable::createFromSVG (*baseXml);
    if (d == nullptr) return std::make_unique<juce::DrawablePath>();
    d->replaceColour (juce::Colour (0xff010101), strokeColor);
    return d;
}

void OpenVoxTunerAudioProcessorEditor::themeChanged()
{
    applyThemeToAllComponents();
}

void OpenVoxTunerAudioProcessorEditor::restyleIconButtons()
{
    for (auto& skin : iconSkins)
    {
        auto* b = skin.button;
        // Toolstrip icons rest on themed backgrounds: strokes follow the
        // palette text token. Active (toggle/transport) states regenerate
        // bright white so they read over their accent pills anywhere.
        auto normal = makeStateDrawable (skin.svgNormal, ebs::text().withAlpha (0.75f));
        auto over   = makeStateDrawable (skin.svgNormal, ebs::text());
        auto down   = makeStateDrawable (skin.svgNormal, ebs::accent());
        if (skin.svgOn != nullptr)
        {
            // Transport-style pair: "normal" states show the first glyph;
            // the On images draw bright white over the SOLID accent pill
            // (see backgroundOnColourId below) so active playback is
            // unmistakable under both themes.
            auto normalOn = makeStateDrawable (skin.svgOn, juce::Colours::white);
            auto overOn   = makeStateDrawable (skin.svgOn, juce::Colours::white);
            auto downOn   = makeStateDrawable (skin.svgOn, juce::Colours::white.withAlpha (0.85f));
            b->setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent());
            b->setImages (normal.get(), over.get(), down.get(), nullptr,
                          normalOn.get(), overOn.get(), downOn.get(), nullptr);
        }
        else if (skin.isToggle)
        {
            // Toggling canvas icons keep a full 6-slot set across theme
            // switches: white glyphs over the accent-tinted ON pill,
            // identical to the constructor wiring.
            auto normalOn = makeStateDrawable (skin.svgNormal, juce::Colours::white);
            auto overOn   = makeStateDrawable (skin.svgNormal, juce::Colours::white);
            auto downOn   = makeStateDrawable (skin.svgNormal, juce::Colours::white.withAlpha (0.85f));
            b->setImages (normal.get(), over.get(), down.get(), nullptr,
                          normalOn.get(), overOn.get(), downOn.get(), nullptr);
        }
        else
        {
            b->setImages (normal.get(), over.get(), down.get());
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::applyCaptionDimming()
{
    // A caption follows its control: at full strength while the control is live, at the caption
    // level (50 %) while it is off or disabled with it. The smallcaps captions dim the same way
    // (ovt::spine::captionColour), so a name that is doing nothing is quiet in every family of
    // the row.
    auto dim = [] (juce::Label& caption, juce::Component& control)
    {
        const auto colour = control.isEnabled() ? ebs::text()
                                                 : ebs::text().withMultipliedAlpha (ebs::kBandTextAlpha);

        if (caption.findColour (juce::Label::textColourId) != colour)
            caption.setColour (juce::Label::textColourId, colour);
    };

    dim (saturationDriveLabel,  saturationDriveSlider);
    dim (saturationToneLabel,   saturationToneSlider);
    dim (delayMixLabel,         delayMixSlider);
    dim (delayDivisionLabel,    delayDivisionKnob);
    dim (delayFeedbackLabel,    delayFeedbackSlider);
    dim (harmonyGainLabel,      harmonyGainSlider);
    dim (harmonyBlendLabel,     harmonyBlendSlider);
    dim (harmonyAttackLabel,    harmonyAttackSlider);
    dim (harmonyFormantLabel,   harmonyFormantSlider);
    dim (useVoiceLabel,         useVoiceButton);
}

void OpenVoxTunerAudioProcessorEditor::applyThemeToAllComponents()
{
    // Refresh LookAndFeel colours
    customLookAndFeel.refreshThemeColours();

    // Re-apply colours to ALL sliders (knobs)
    auto applySliderColours = [] (juce::Slider& s) {
        s.setColour (juce::Slider::textBoxTextColourId,       ebs::text());
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::rotarySliderFillColourId,  ebs::accent());
        s.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
        s.setColour (juce::Slider::thumbColourId,             juce::Colours::white);
    };
    applySliderColours (speedSlider);
    applySliderColours (amountSlider);
    applySliderColours (formantSlider);
    applySliderColours (leadRoomSlider);
    applySliderColours (leadHallSlider);
    applySliderColours (harmonyRoomSlider);
    applySliderColours (harmonyHallSlider);
    applySliderColours (duckDepthSlider);
    applySliderColours (outputTrimSlider);
    applySliderColours (noiseGateThresholdSlider);
    applySliderColours (flexTuneSlider);
    applySliderColours (humanizeSlider);
    applySliderColours (vibratoPreserveSlider);
    applySliderColours (attackReleaseSlider);
    applySliderColours (harmonyGainSlider);
    applySliderColours (harmonyBlendSlider);
    applySliderColours (harmonyFormantSlider);
    applySliderColours (doubleAmountSlider);
    // Gaps closed here: the de-esser and saturation knobs were missing from this pass, so a theme
    // switch left them painted with construction colours. The delay pair joins them in one list.
    applySliderColours (deessAmountSlider);
    applySliderColours (saturationDriveSlider);
    applySliderColours (saturationToneSlider);
    applySliderColours (delayMixSlider);
    applySliderColours (delayFeedbackSlider);

    // Re-apply colours to ALL labels
    auto applyLabelColours = [] (juce::Label& l) {
        l.setColour (juce::Label::textColourId, ebs::text());
    };
    applyLabelColours (speedLabel);
    applyLabelColours (amountLabel);
    applyLabelColours (flexTuneLabel);
    applyLabelColours (humanizeLabel);
    applyLabelColours (vibratoPreserveLabel);
    applyLabelColours (attackReleaseLabel);
    applyLabelColours (leadRoomLabel);
    applyLabelColours (leadHallLabel);
    applyLabelColours (harmonyRoomLabel);
    applyLabelColours (harmonyHallLabel);
    applyLabelColours (duckDepthLabel);
    applyLabelColours (saturationDriveLabel);
    applyLabelColours (saturationToneLabel);
    // The delay captions join the pass next to their saturation siblings.
    applyLabelColours (delayDivisionLabel);
    applyLabelColours (delayMixLabel);
    applyLabelColours (delayFeedbackLabel);
    applyLabelColours (outputTrimLabel);
    applyLabelColours (noiseGateThresholdLabel);
    applyLabelColours (harmonyToneColorLabel);
    applyLabelColours (keyLabel);
    applyLabelColours (scaleLabel);
    applyLabelColours (keySourceLabel);
    applyLabelColours (companionGroupLabel);
    applyLabelColours (latencyModeLabel);
    applyLabelColours (harmonyTypeLabel);
    applyLabelColours (voiceTypeLabel);
    applyLabelColours (formantStrategyLabel);
    applyLabelColours (measuresLabel);

    // The pass above sets every caption to full strength; the dimming pass puts the quiet ones
    // back where their control says they belong.
    applyCaptionDimming();

    // Re-apply colours to ALL combo boxes
    auto applyComboColours = [] (juce::ComboBox& c) {
        // Closed combo state: use darkest plugin background
        c.setColour (juce::ComboBox::backgroundColourId, ebs::bgDark());
        c.setColour (juce::ComboBox::textColourId,       ebs::text());
        c.setColour (juce::ComboBox::outlineColourId,    ebs::accentSoft());
        c.setColour (juce::ComboBox::arrowColourId,      ebs::accent());
        // Dropdown popup: same dark background
        c.setColour (juce::PopupMenu::backgroundColourId, ebs::bgDark());
        c.setColour (juce::PopupMenu::textColourId,       ebs::text());
        c.setColour (juce::PopupMenu::highlightedBackgroundColourId, ebs::accentSoft());
    };
    applyComboColours (keyBox);
    applyComboColours (scaleBox);
    applyComboColours (keySourceBox);
    applyComboColours (companionGroupBox);
    applyComboColours (latencyModeBox);
    applyComboColours (harmonyTypeBox);
    applyComboColours (shiftedVoicesBox);
    applyComboColours (harmonyToneBox);
    // The division combo rides the same theme pass as every other selector in the editor.
    applySliderColours (delayDivisionKnob);
    // Light-theme completion (2026-08): previously styled once with dark literals at creation.
    applyComboColours (voiceTypeBox);
    applyComboColours (formantStrategyBox);
    applyComboColours (measuresComboBox);
    applyComboColours (presetComboBox);

    // Toolbar icon glyphs are baked SVG strokes; regenerate them under the
    // active palette (they used fixed grey/lightgrey/white ramps).
    restyleIconButtons();

    // Re-apply colours to toggle buttons
    auto applyToggleColours = [] (juce::ToggleButton& b) {
        b.setColour (juce::ToggleButton::textColourId, ebs::text());
    };
    applyToggleColours (harmonyEnableButton);
    applyToggleColours (harmonyFollowLeadButton);
    // The shared switch keeps its track tokens live: re-apply them, and the
    // caption label too.
    useVoiceLabel.setColour (juce::Label::textColourId, ebs::text());
    useVoiceButton.setColour (ebs::ToggleSwitch::trackOnColourId,  ebs::accent());
    useVoiceButton.setColour (ebs::ToggleSwitch::trackOffColourId, ebs::text().withAlpha (0.18f));
    applyToggleColours (formantEnableButton);
    applyToggleColours (leadReverbEnableButton);
    applyToggleColours (harmonyReverbEnableButton);
    applyToggleColours (limiterEnableButton);
    applyToggleColours (noiseGateEnableButton);
    applyToggleColours (doubleEnableButton);

    // Re-apply to other buttons
    updateButton.setColour (juce::TextButton::buttonColourId,   ebs::bgPanel());
    updateButton.setColour (juce::TextButton::textColourOffId,  ebs::text());
    // "Advanced" correction-block expander: the frame paints its own body and the
    // chevron reads palette tokens inside its own paint, so repaints are all a theme
    // switch needs.
    correctionFrame.repaint();
    correctionFrame.disclosure().repaint();
    // The two plain framed bodies read palette tokens inside their own paint, so a theme
    // switch needs the same repaint as the ExpansibleFrames. Harmony is an ExpansibleFrame
    // too, so its chevron strip gets the disclosure repaint the other two get.
    scaleFrame.repaint();
    harmonyFrame.repaint();
    harmonyFrame.disclosure().repaint();
    // Effects and Reverb are ExpansibleFrames too. Both were missing here: a theme switch left the
    // chevron strip of these two frames painted with the OLD palette until something else repainted
    // them, because an ExpansibleFrame draws its body and chevron from palette tokens in its own
    // paint. Adding the new frame made this gap visible; both get the same pair as Harmony.
    effectsFrame.repaint();
    effectsFrame.disclosure().repaint();
    reverbFrame.repaint();
    reverbFrame.disclosure().repaint();
    // The band cues dissolve the panel colour into the edge and draw the shared glyph in
    // accent, so they need the same repaint as the frames they sit over.
    bottomBand.leftCueStrip().repaint();
    bottomBand.rightCueStrip().repaint();
    // A/B slots read palette tokens directly inside their custom paint;
    // a repaint keeps them current after a theme switch.
    buttonA.repaint();
    buttonB.repaint();
    correctionModeButton.setColour (juce::TextButton::buttonColourId,  ebs::bgPanel());
    correctionModeButton.setColour (juce::TextButton::textColourOffId, ebs::text());
    applyToggleColours (attackAwareButton);
    applyToggleColours (keyDetectPowerButton);
    buttonA.setColour (juce::TextButton::buttonColourId,  ebs::bgPanel());
    buttonA.setColour (juce::TextButton::textColourOffId, ebs::text());
    buttonB.setColour (juce::TextButton::buttonColourId,  ebs::bgPanel());
    buttonB.setColour (juce::TextButton::textColourOffId, ebs::text());

    // Re-apply to the Curve Editor "Options" and Preset Gallery buttons: framed
    // icon buttons keep their accent-tinted body and bright glyph under both themes.
    for (auto* b : { &optionsButton, &presetGalleryButton })
    {
        b->setFramed (true);
        b->setColour (ebs::IconButton::iconColourId, juce::Colours::white);
        b->setColour (ebs::IconButton::frameFillColourId, ebs::accent().withAlpha (0.22f));
    }

    // Re-apply DrawableButton colours (background and text)
    auto applyDrawableBtnColours = [] (juce::DrawableButton& btn) {
        btn.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        btn.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent().withAlpha (0.2f));
        btn.setColour (juce::DrawableButton::textColourId, ebs::text());
    };
    applyDrawableBtnColours (snapButton);
    applyDrawableBtnColours (snapGridButton);
    applyDrawableBtnColours (stepModeButton);

    // Re-apply to tabbed component
    tabbedComponent.setColour (juce::TabbedComponent::backgroundColourId, ebs::bgDark());
    tabbedComponent.setColour (juce::TabbedComponent::outlineColourId,   ebs::accentSoft());

    // Force repaint of tab content components
    tabbedComponent.repaint();
    if (pitchVisualizer != nullptr)
        pitchVisualizer->repaint();
    if (curveEditor != nullptr)
        curveEditor->repaint();

    repaint();
}

