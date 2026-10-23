// PluginEditor.cpp
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#include "PluginProcessor.h"
#include "dsp/LatencyDeclaration.h"
#include "PluginEditor.h"
#include "ui/OVTFonts.h"
#include "ui/OVTTheme.h"
#include "ui/OVTLanguages.h"
#include "ui/PresetGallery.h"
#include "ui/CorrectionModeSwitch.h"
#include "ui/MidiChannelSelectorComponent.h"
#include "ui/AudioImportDialogComponent.h"
#include "ui/PluginEditorInternal.h"
#include "ovt_assets_binary_data.h"
#include <map>

// The standalone wrapper owns the audio/MIDI device dialog. Its header lives in the
// juce_audio_plugin_client module, which is already on the include path. Only the standalone
// build has a live StandalonePluginHolder instance, so the include and the call are both
// guarded: compiling this into the VST3 target would drag the whole wrapper class into the
// plugin binary for nothing.
#if JUCE_STANDALONE_APPLICATION
 #include <juce_audio_utils/juce_audio_utils.h>   // AudioDeviceManager / AudioProcessorPlayer, declared by the wrapper header
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

// Cached vector logo (assets/icon.svg embedded as a binary resource). Parsed
// once and reused so we don't re-parse the SVG on every repaint.
static juce::Drawable* getBrandLogo()
{
    static std::unique_ptr<juce::Drawable> logo;
    if (logo == nullptr && ovtAssets::icon_svgSize > 0)
    {
        auto svgXml = juce::XmlDocument::parse (ovtAssets::icon_svg);
        if (svgXml != nullptr)
            logo = juce::Drawable::createFromSVG (*svgXml);
    }
    return logo.get();
}

// === Theme colors ("autotune" style: dark + pink/purple accent) ===

const juce::Colour OpenVoxTunerAudioProcessorEditor::kBgDark     = juce::Colour::fromString("#FF121318"); // Deep background
const juce::Colour OpenVoxTunerAudioProcessorEditor::kBgPanel    = juce::Colour::fromString("#FF14151C"); // Dark panels
const juce::Colour OpenVoxTunerAudioProcessorEditor::kAccent     = juce::Colour::fromString("#FF1A9AF0"); // Light blue (Vocal Tune)
const juce::Colour OpenVoxTunerAudioProcessorEditor::kAccentSoft = juce::Colour::fromString("#401A9AF0"); // Soft blue
const juce::Colour OpenVoxTunerAudioProcessorEditor::kText       = juce::Colour::fromString("#FFE1E1E6"); // Off-white text

// === Scale modes names for displaying slider values ===
static const juce::StringArray kScaleNames = {
    "Chromatic", "Major", "Melodic Minor", "Harmonic Minor", "Natural Minor", 
    "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Phrygian", 
    "Lydian", "Mixolydian", "Locrian", "Custom"
};
// === Note names for the Key slider ===
static const juce::StringArray kNoteNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

namespace
{
    juce::String normaliseVersionString (juce::String v)
    {
        v = v.trim();
        if (v.startsWithChar ('v') || v.startsWithChar ('V'))
            v = v.substring (1).trim();
        return v;
    }

    int compareVersionStrings (juce::String latest, juce::String current)
    {
        latest = normaliseVersionString (latest);
        current = normaliseVersionString (current);

        juce::StringArray a, b;
        a.addTokens (latest, ".", "");
        b.addTokens (current, ".", "");
        const int count = juce::jmax (a.size(), b.size());

        for (int i = 0; i < count; ++i)
        {
            const int av = (i < a.size()) ? a[i].getIntValue() : 0;
            const int bv = (i < b.size()) ? b[i].getIntValue() : 0;
            if (av > bv) return 1;
            if (av < bv) return -1;
        }

        return 0;
    }
}

struct OpenVoxTunerUpdateCheckState
{
    std::atomic<bool> finished { false };
    std::atomic<bool> cancelled { false };
    std::atomic<bool> updateAvailable { false };
    juce::String latestVersion;
    juce::String releaseUrl;
    juce::String statusText;
};

bool OpenVoxTunerAudioProcessorEditor::isVersionNewer (const juce::String& latest, const juce::String& current)
{
    return compareVersionStrings (latest, current) > 0;
}

void OpenVoxTunerAudioProcessorEditor::startUpdateCheck()
{
    updateCheckState = std::make_shared<OpenVoxTunerUpdateCheckState>();

    juce::String currentVersion;
   #if defined (JucePlugin_VersionString)
    currentVersion = JucePlugin_VersionString;
   #elif defined (JucePlugin_Version)
    currentVersion = juce::String (JucePlugin_Version);
   #else
    currentVersion = "0.0.0";
   #endif

    const juce::String updateInfoUrl = OVT_UPDATE_INFO_URL;
    auto state = updateCheckState;

    std::thread ([state, currentVersion, updateInfoUrl]
    {
        if (state == nullptr || state->cancelled.load())
            return;

        state->statusText = "Checking...";
        state->releaseUrl = "https://github.com/EiffelBS/OpenVoxTuner/releases/latest";

        juce::String response;
        try
        {
            response = juce::URL (updateInfoUrl).readEntireTextStream (false);
        }
        catch (...)
        {
            response.clear();
        }

        if (! state->cancelled.load() && response.isNotEmpty())
        {
            auto parsed = juce::JSON::parse (response);
            juce::String latestVersion;
            juce::String releaseUrl;

            if (auto* obj = parsed.getDynamicObject())
            {
                latestVersion = obj->getProperty ("version").toString();
                releaseUrl = obj->getProperty ("url").toString();
            }

            if (releaseUrl.isEmpty())
                releaseUrl = "https://github.com/EiffelBS/OpenVoxTuner/releases/latest";

            if (latestVersion.isNotEmpty())
            {
                state->latestVersion = latestVersion;
                state->releaseUrl = releaseUrl;
                state->updateAvailable.store (OpenVoxTunerAudioProcessorEditor::isVersionNewer (latestVersion, currentVersion));
                state->statusText = state->updateAvailable.load() ? "Update available" : "Up to date";
            }
            else
            {
                state->statusText = "Update check failed";
            }
        }
        else if (! state->cancelled.load())
        {
            state->statusText = "Update check failed";
        }

        state->finished.store (true);
    }).detach();
}

// === Constructor helpers ===
// Promoted out of the constructor (step 3 of the source split) so the section
// methods can share them. Bodies are byte-identical to the local lambdas they
// replace; only the header line and the indentation changed.

// Helper for creating Drawables from full SVG XML (with viewBox).
// Uses a placeholder color (#010101) that gets replaced by the desired state color.
static std::unique_ptr<juce::Drawable> createDrawableSVG (const char* svgChars, juce::Colour strokeColor)
{
    auto baseXml = juce::XmlDocument::parse(svgChars);
    if (baseXml == nullptr) return std::make_unique<juce::DrawablePath>();
    auto d = juce::Drawable::createFromSVG(*baseXml);
    if (d == nullptr) return std::make_unique<juce::DrawablePath>();
    // Replace placeholder color with the desired state color
    d->replaceColour(juce::Colour(0xff010101), strokeColor);
    return d;
}

void OpenVoxTunerAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label* l, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    s.setColour (juce::Slider::rotarySliderFillColourId,    ebs::accent());
    s.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
    s.setColour (juce::Slider::textBoxTextColourId,         ebs::text());
    s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    addAndMakeVisible (s);

    if (l != nullptr)
    {
        l->setText (name, juce::dontSendNotification);
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, ebs::text());
        l->setFont (ovt::fontCaption());   // a caption in a frame names its knob, it does not shout
        addAndMakeVisible (*l);
    }
}

// Theme-aware icon states: strokes track ebs::text() instead of the old
// fixed grey/lightgrey/white ramp so toolbar icons stay readable on
// Light backgrounds too. Registered skins are regenerated on every
// ebs::setTheme() through restyleIconButtons().
void OpenVoxTunerAudioProcessorEditor::setupIconButton (juce::DrawableButton& btn, const char* svgChars,
                                                         bool isToggle, const juce::String& tooltip)
{
    // Toolstrip icons sit on THEMED page backgrounds under both
    // palettes, so rest strokes follow the palette text token.
    // Only ACTIVE toggle states flip to bright white over the
    // accent-tinted pill, which reads on any backdrop.
    auto normal   = createDrawableSVG(svgChars, ebs::text().withAlpha(0.75f));
    auto over     = createDrawableSVG(svgChars, ebs::text());
    auto down     = createDrawableSVG(svgChars, ebs::accent());

    // Active state must read against the accent-tinted ON pill under
    // BOTH themes: bright white glyphs, never palette/accent strokes.
    if (isToggle) {
        auto normalOn = createDrawableSVG(svgChars, juce::Colours::white);
        auto overOn   = createDrawableSVG(svgChars, juce::Colours::white);
        auto downOn   = createDrawableSVG(svgChars, juce::Colours::white.withAlpha(0.85f));
        btn.setImages(normal.get(), over.get(), down.get(), nullptr,
                      normalOn.get(), overOn.get(), downOn.get(), nullptr);
        btn.setClickingTogglesState(true);
    } else {
        btn.setImages(normal.get(), over.get(), down.get());
    }
    iconSkins.push_back ({ &btn, svgChars, nullptr, isToggle });

    btn.setTooltip(tooltip);
    btn.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
    // Solid accent when toggled so the bright-white ON glyphs keep
    // contrast over themed strips under BOTH palettes.
    btn.setColour(juce::DrawableButton::backgroundOnColourId, ebs::accent());
    btn.setColour(juce::DrawableButton::textColourId, juce::Colours::white);
    btn.setColour(juce::DrawableButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible(btn);
}

// Non-toggle toolbar icons are the shared library's IconButton: it owns the
// glyph, the palette-aware ink and the accent hover, so these buttons need
// no SVG string, no per-state drawable set and no theme restyling pass.
void OpenVoxTunerAudioProcessorEditor::setupEbsIcon (ebs::IconButton& btn, const juce::String& tooltip)
{
    btn.setTooltip (tooltip);
    addAndMakeVisible (btn);
}

// Framed variant for icons sitting in a toolbar row: a real button
// affordance (body + outline), the same language as the old accent-tinted
// drawable backgrounds they replace.
void OpenVoxTunerAudioProcessorEditor::setupFramedIcon (ebs::IconButton& btn, const juce::String& tooltip)
{
    btn.setFramed (true);
    btn.setTooltip (tooltip);
    addAndMakeVisible (btn);
}

OpenVoxTunerAudioProcessorEditor::OpenVoxTunerAudioProcessorEditor (OpenVoxTunerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Reset the editor-shutdown flag set by the previous editor instance's
    // destructor.  The processor outlives the editor, so without this
    // reset the flag would stay stuck at `true` after every close/reopen
    // cycle, short-circuiting updateHostTransport() and starving the
    // visualizer of data.
    processorRef.notifyEditorResumed();

    setLookAndFeel (&customLookAndFeel);

    // Restore theme preference from plugin state
    auto* themeParam = processorRef.getParameters().getParameter ("ui_theme");
    if (themeParam != nullptr)
        ebs::currentTheme() = (themeParam->getValue() > 0.5f) ? ebs::Theme::Light : ebs::Theme::Dark;

    applyThemeToAllComponents();

    // Theme broadcast plumbing: one ebs::setTheme() call now re-applies the
    // shared L&F AND every per-instance colour through themeChanged().
    ebs::subscribeTheme (this);

    // Restore language preference from plugin state
    auto* langParam = processorRef.getParameters().getParameter ("ui_language");
    if (langParam != nullptr)
    {
        const int langIdx = juce::roundToInt (langParam->getValue() * 5.0f);
        switch (langIdx)
        {
            case 0: ovt::currentLanguage() = ovt::Language::English;  break;
            case 1: ovt::currentLanguage() = ovt::Language::French;   break;
            case 2: ovt::currentLanguage() = ovt::Language::German;   break;
            case 3: ovt::currentLanguage() = ovt::Language::Spanish;  break;
            case 4: ovt::currentLanguage() = ovt::Language::Japanese; break;
            case 5: ovt::currentLanguage() = ovt::Language::Chinese;  break;
            default: ovt::currentLanguage() = ovt::Language::English; break;
        }
    }

    // Restore waveform overlay preference from APVTS.
    if (auto* wfParam = processorRef.getParameters().getParameter ("ui_show_waveform"))
    {
        showWaveform = wfParam->getValue() > 0.5f;
        processorRef.setWaveformCaptureEnabled (showWaveform);
    }

    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 700);
    tooltipWindow->setLookAndFeel (&customLookAndFeel);
    // Sync the TooltipWindow with the user-configured delay (Normal=700ms
    // by default; the user can pick Slow=1500ms or Fast=200ms via the
    // wrench / Interface / Tooltip Delay submenu).
    setTooltipDelayChoice (tooltipDelayChoice);

    updateButton.setButtonText (ovt::tr(ovt::Keys::kLabelUpdates));
    updateButton.setTooltip (ovt::tr(ovt::Keys::kTooltipCheckUpdates));
    updateButton.onClick = [this]
    {
        if (updateCheckState != nullptr && updateCheckState->updateAvailable.load())
        {
            if (updateCheckState->releaseUrl.isNotEmpty())
                juce::URL (updateCheckState->releaseUrl).launchInDefaultBrowser();
        }
        else
        {
            juce::URL ("https://github.com/EiffelBS/OpenVoxTuner/releases").launchInDefaultBrowser();
        }
    };
    updateButton.setColour (juce::TextButton::buttonColourId, ebs::bgPanel());
    updateButton.setColour (juce::TextButton::textColourOffId, ebs::text());
    updateButton.setColour (juce::TextButton::textColourOnId, ebs::accent());
    addAndMakeVisible (updateButton);

    startUpdateCheck();

    // === Configuration of the 4 sliders (rotary knobs) ===

    setupKnob (speedSlider,  &speedLabel,  "Speed (ms)");
    translatableLabels.push_back ({ &speedLabel, ovt::Keys::kLabelSpeed    });
    setupKnob (amountSlider, &amountLabel, "Amount");
    translatableLabels.push_back ({ &amountLabel, ovt::Keys::kLabelAmount    });
    speedSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSpeed));
    amountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipAmount));

    // The Correction block is an ebs::ExpansibleFrame, the same component the Effects
    // frame uses: the chevron on its right edge folds the advanced zone (Vibrato,
    // Humanize, Voice Type, Formant Strategy) away, and the block WIDENS to take it,
    // pushing Harmony - exactly the Effects contract. The frame owns the fold flag,
    // the editor answers by re-laying out the bottom row.
    // Every bottom-row block goes into one ebs::FrameBand. The band lays them out left
    // to right in registration order, gives spare room only to blocks marked growable,
    // and scrolls below the hard floor instead of stretching the last block. It reparents
    // each frame, so the widgets already inside a frame body stay there.
    addAndMakeVisible (bottomBand);
    // Every frame now carries a left title spine, so the width each block asks for includes it:
    // ebs::FrameTitleBand::bandWidth() is one fixed number for every locale.
    const int kTitleBandW = ebs::FrameTitleBand::bandWidth();
    bottomBand.addFrame (&correctionFrame, kKnobBlockW + kTitleBandW, kKnobBlockW + kTitleBandW, true);   // restated by the width model below
    // Key/Scale takes the SECOND position: registration order IS left-to-right, so the keyboard - the block a user
    // reads before dialling anything - sits inside the non-scrolling part of the row and Harmony stays at the right
    // edge as the only growable frame. Final order: Correction | Key & Scale | Effects | Reverb | Harmony.
    bottomBand.addFrame (&scaleFrame, kScalePrefW + kTitleBandW, kScaleMinW + kTitleBandW, false);
    bottomBand.addFrame (&effectsFrame, 2 * kEffectsColW + 2 * kEffectsPad + kTitleBandW, 2 * kEffectsColW + 2 * kEffectsPad + kTitleBandW, false);
    // The Reverb frame sits immediately right of Effects: folded it is ONE column carrying the LEAD voice switch and
    // the two amounts it sends (Room, Hall), and opening it adds the HARMONY column and the sidechain Ducking column.
    // Registration order IS left-to-right order in the band, so this line puts the frame next to the Harmony block its
    // send feeds. Non-growable like Effects: its width is its cell count when open and the measured width of the Lead
    // switch when folded - the numbers here are only an initial placeholder, updateBottomBandWidths() restates them
    // from the loaded language right away.
    const int reverbChevronW = ebs::ExpansibleFrame::chevronWidth() + ebs::ExpansibleFrame::chevronGap();
    const int reverbFoldedW  = kEffectsColW + 2 * kEffectsPad + reverbChevronW + kTitleBandW;
    bottomBand.addFrame (&reverbFrame, reverbFoldedW, reverbFoldedW, false);
    bottomBand.addFrame (&harmonyFrame, harmonyFoldedW(), harmonyFoldedW(), true);
    // Each title band is a CHILD of the frame it names, so it paints inside that frame's rounded corner and travels
    // with the frame when the row re-lays out. The keys ride the i18n list: refreshLabels() restates every spine
    // when the interface language changes, exactly as it does for labels.
    correctionFrame.addAndMakeVisible (correctionTitleBand);
    scaleFrame.addAndMakeVisible (scaleTitleBand);
    effectsFrame.addAndMakeVisible (effectsTitleBand);
    reverbFrame.addAndMakeVisible (reverbTitleBand);
    harmonyFrame.addAndMakeVisible (harmonyTitleBand);
    translatableBands.push_back ({ &correctionTitleBand, ovt::Keys::kFrameTitleCorrection });
    translatableBands.push_back ({ &scaleTitleBand,      ovt::Keys::kFrameTitleKeyScale   });
    translatableBands.push_back ({ &effectsTitleBand,    ovt::Keys::kFrameTitleEffects     });
    translatableBands.push_back ({ &reverbTitleBand,     ovt::Keys::kFrameTitleReverb      });
    translatableBands.push_back ({ &harmonyTitleBand,    ovt::Keys::kFrameTitleHarmony     });
    // The row ends with Harmony: it is the only growable frame, so the spare room the band has left goes to it.
    // Root/Scale and Harmony are framed components too, so every bottom-row block is a
    // component: a re-layout moves components, never a painted region.

    correctionFrame.disclosure().setTooltip (ovt::tr (ovt::Keys::kTooltipAdvanced));
    // Restore the state persisted in the plugin XML BEFORE the callback is armed, so
    // restoring a saved session does not fire a re-layout.
    correctionFrame.setExpanded (processorRef.getAdvancedExpanded());
    // Harmony obeys the same rule: its chevron folds the four knob columns (Volume / Blend /
    // Formant / Attack) away and the block narrows to its left column, handing that room back
    // to the band. The flag must land before the width model below runs, otherwise a session
    // saved with Harmony folded would open sized for the wide block.
    harmonyFrame.disclosure().setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyMore));
    harmonyFrame.setExpanded (processorRef.getHarmonyExpanded());
    // The Reverb frame obeys the same rule, and for the same reason: its flag must land before
    // the width model runs, so a session saved with the send room folded opens at the folded size.
    reverbFrame.disclosure().setTooltip (ovt::tr (ovt::Keys::kTooltipReverbMore));
    reverbFrame.setExpanded (processorRef.getReverbExpanded());
    // The Effects frame obeys the same rule as the three above: its flag lands before the width model
    // runs, so a session saved with the open zone showing opens at the five-column size (467 px measured)
    // instead of folding away the Delay column on every launch.
    effectsFrame.setExpanded (processorRef.getEffectsExpanded());
    // The width model is the single source of truth for every block size, so it runs right
    // after the restored fold flags and before any layout.
    updateBottomBandWidths();
    correctionFrame.onExpandedChanged = [this]
    {
        processorRef.setAdvancedExpanded (correctionFrame.expanded());
        updateBottomBandWidths();   // the fold changes what this block asks for
        resized();
    };
    // The frame owns the flag; the editor answers by re-stating every block width and re-laying
    // out. All four frames now persist their fold in the plugin XML, so a row folded to fit a
    // narrow window stays folded when the session reopens - and a rack opened to reach the Delay
    // column comes back open.
    harmonyFrame.onExpandedChanged = [this]
    {
        processorRef.setHarmonyExpanded (harmonyFrame.expanded());
        updateBottomBandWidths();
        resized();
    };
    // Same contract for the Reverb frame: opening it adds one column to the right, so the band
    // has to be re-stated and the row re-laid out. Its fold is persisted like Harmony's, because
    // a send room folded to fit a narrow window must stay folded when the session reopens.
    reverbFrame.onExpandedChanged = [this]
    {
        processorRef.setReverbExpanded (reverbFrame.expanded());
        updateBottomBandWidths();
        resized();
    };

    // The Speed / Amount knobs and the studio-console switch stay visible in both states;
    // the advanced zone is the one that folds. The widgets move into the frame body with
    // modeSwitch, created below.
    setupKnob (formantSlider, nullptr, "");
    // Pivot knob: fill grows from the central 0 value, not from the minimum.
    formantSlider.getProperties().set ("centred", true);
    // Formant knob: no value textbox; the live value is shown in JUCE's popup
    // display while dragging. The TooltipWindow can't be used for this: it only
    // appears after the mouse is stationary, which never happens during a
    // rotate-drag, so the value tooltip would never show.
    formantSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    formantSlider.setPopupDisplayEnabled (true, false, this);
    formantSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " st"; };

    // === Harmony UI ===
    // Harmony enable toggle (use same visual style as Formant)
    harmonyEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyBtn));
    harmonyEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    harmonyEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    harmonyEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyEn));
    addAndMakeVisible (harmonyEnableButton);

    // Harmony "Follow Lead" toggle (power-style): when on, the harmony voices move with
    // the lead correction character (vibrato preservation, humanize, flex, attack-aware)
    // instead of staying locked to the scale grid. Default on.
    harmonyFollowLeadButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyFollow));
    harmonyFollowLeadButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    harmonyFollowLeadButton.setColour (juce::ToggleButton::tickColourId,  ebs::accent());
    harmonyFollowLeadButton.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyFollow));
    addAndMakeVisible (harmonyFollowLeadButton);

    // Harmony "Gain Match" toggle (power-style): when on, the harmony mix is scaled
    // by 1/sqrt(1+N) where N = number of active harmony voices, so the total output
    // RMS is roughly equal to the dry input RMS. Compensates for the additive
    // volume boost that is most noticeable on Unison and Unison+Octaves. Default on.
    // The dry signal is untouched; the user-facing harmony volume knob still
    // controls the overall harmony level (post-compensation).
    harmonyGainMatchButton.setButtonText (ovt::tr(ovt::Keys::kLabelHarmonyGainMatch));
    harmonyGainMatchButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    harmonyGainMatchButton.setColour (juce::ToggleButton::tickColourId,  ebs::accent());
    harmonyGainMatchButton.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyGainMatch));
    addAndMakeVisible (harmonyGainMatchButton);

    // Harmony type combo - index 3 = "3rd Below + Above" (default).
    // IMPORTANT: this list MUST contain ALL 22 choices, matching the
    // "harmony_type" AudioParameterChoice (0..21). The ComboBoxParameterAttachment
    // maps the parameter's normalized value via `round(norm * (numItems-1))`, so a
    // box with fewer items than the parameter shifts every index above the missing
    // ones (e.g. Power Chord = param 17 but box index 15 when "Unison" items are
    // absent), making the displayed selection wrong.
    harmonyTypeBox.addItemList (juce::StringArray {
        "None",
        "3rd Below", "3rd Above", "3rd Below + Above",
        "4th Below", "4th Above", "4th Below + Above",
        "5th Below", "5th Above", "5th Below + Above",
        "3rd Below + 5th Above", "5th Below + 3rd Above",
        "Octave Below", "Octave Above", "Octave Below + Above",
        "Vocal Stack (3 voices)", "Vocal Stack (4 voices)",
        "Power Chord", "Parallel 3rd", "Drone",
        "Unison (2 voices)", "Unison + Octaves (4 voices)"
    }, 1);
    harmonyTypeBox.setSelectedItemIndex (3, juce::dontSendNotification);
    harmonyTypeBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    harmonyTypeBox.setColour (juce::ComboBox::textColourId, ebs::text());
    harmonyTypeBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    addAndMakeVisible (harmonyTypeBox);

    // Harmony knobs (Volume, Blend) - use same rotary knob style as main knobs
    setupKnob (harmonyGainSlider, nullptr, "");
    harmonyGainSlider.setRange (0.0, 1.0, 0.01);
    harmonyGainSlider.setValue (1.0);
    harmonyGainSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyGainSlider.setPopupDisplayEnabled (true, false, this);
    harmonyGainSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    harmonyGainSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipVolume));

    setupKnob (harmonyBlendSlider, nullptr, "");
    harmonyBlendSlider.setRange (0.0, 1.0, 0.01);
    harmonyBlendSlider.setValue (0.5);
    harmonyBlendSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyBlendSlider.setPopupDisplayEnabled (true, false, this);
    harmonyBlendSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    harmonyBlendSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipBlend));
    harmonyAttackSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyAttack));

    // Harmony Attack knob: per-voice fade-in duration (ms). Placed under Blend.
    setupKnob (harmonyAttackSlider, nullptr, "");
    harmonyAttackSlider.setRange (1.0, 300.0, 1.0);
    harmonyAttackSlider.setValue (35.0);
    harmonyAttackSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyAttackSlider.setPopupDisplayEnabled (true, false, this);
    harmonyAttackSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };

    setupKnob (harmonyFormantSlider, nullptr, "");
    // Pivot knob: fill grows from the central 0 value, not from the minimum.
    harmonyFormantSlider.getProperties().set ("centred", true);
    harmonyFormantSlider.setRange (-5.0, 5.0, 0.1);
    harmonyFormantSlider.setValue (0.0);
    harmonyFormantSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyFormantSlider.setPopupDisplayEnabled (true, false, this);
    harmonyFormantSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " st"; };
    harmonyFormantSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyFormant));

    // Doubler knob: 0..100 % stereo doubling of the corrected lead. Independent
    // of harmony_enable, so it stays live when the Harmony block is switched off.
    // Greyed (not disabled as a control) while double_enable is off; the value is
    // kept, exactly like the reverb amounts behind their voice switch.
    setupKnob (doubleAmountSlider, nullptr, "");
    doubleAmountSlider.setRange (0.0, 100.0, 1.0);
    doubleAmountSlider.setValue (0.0);
    doubleAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    doubleAmountSlider.setPopupDisplayEnabled (true, false, this);
    doubleAmountSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    doubleAmountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDoubleAmount));

    // Doubler power toggle: a real parameter (double_enable), attached to the button
    // like every sibling effect in this block (Gate / Compress / Reverb / Formant).
    // Switching off does not clear the amount, it only stops applying the copy.
    doubleEnableButton.setButtonText (ovt::tr (ovt::Keys::kLabelDouble));
    doubleEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    doubleEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    doubleEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipDoubleEn));
    addAndMakeVisible (doubleEnableButton);

    // Title labels above the Harmony knobs (no value display, just names).
    auto setupTitleLabel = [this] (juce::Label& l, const juce::String& name) {
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, ebs::text());
        l.setFont (ovt::fontCaption());
        addAndMakeVisible (l);
    };
    setupTitleLabel (harmonyGainLabel, "Volume");
    setupTitleLabel (harmonyBlendLabel, "Blend");
    setupTitleLabel (harmonyAttackLabel, "Attack");
    setupTitleLabel (harmonyFormantLabel, "Formant");

    // De-Esser: ONE knob. The intensity value drives the detector threshold
    // (-38 x amount dB) and the depth cap (19 x amount dB) together, the way a
    // hardware de-esser's Intensity knob does. 0 is inert: the threshold sits at
    // 0 dBFS so the open point is unreachable, and the depth floor is unity gain.
    // The band centre is not a control: it follows voice_type, the register the
    // user already sets for pitch detection, so a low voice is de-essed around
    // 5 kHz and a soprano around 8.4 kHz. The knob is greyed while deess_enable is
    // off and keeps its value, exactly like the reverb amounts behind their switch.
    setupKnob (deessAmountSlider, nullptr, "");
    deessAmountSlider.setRange (0.0, 1.0, 0.01);
    deessAmountSlider.setValue (0.65);
    deessAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    deessAmountSlider.setPopupDisplayEnabled (true, false, this);
    deessAmountSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    deessAmountSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDeesserAmount));

    // De-Esser power toggle: a real parameter (deess_enable), attached to the
    // button like every sibling effect in this block (Gate / Comp / Reverb /
    // Formant / Doubler). Switching off does not clear the knobs.
    deessEnableButton.setButtonText (ovt::tr (ovt::Keys::kLabelDeesser));
    deessEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    deessEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    deessEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipDeesserEnable));
    addAndMakeVisible (deessEnableButton);

    // Saturation: a soft tanh drive on the master output, after the formant stage and before
    // the reverb return, so the tail stays clean. The power toggle carries the name; Drive and
    // Warmth are two labelled knobs in one column, the same shape the send-room cells use.
    // Drive 0 is a transparent unity bypass (the stage is skipped), so the knob at zero changes
    // nothing. Warmth is the warm low-pass (8 kHz bright .. 5 kHz warm) that tames the
    // harmonics the clipper adds. The name is drive / warmth, never "tape".
    setupKnob (saturationDriveSlider, &saturationDriveLabel, "Drive");
    saturationDriveSlider.setRange (0.0, 1.0, 0.01);
    saturationDriveSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    saturationDriveSlider.setPopupDisplayEnabled (true, false, this);
    saturationDriveSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    saturationDriveSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSaturationDrive));
    translatableLabels.push_back ({ &saturationDriveLabel, ovt::Keys::kLabelSaturationDrive });

    setupKnob (saturationToneSlider, &saturationToneLabel, "Warmth");
    saturationToneSlider.setRange (0.0, 1.0, 0.01);
    saturationToneSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    saturationToneSlider.setPopupDisplayEnabled (true, false, this);
    saturationToneSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    saturationToneSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipSaturationWarmth));
    translatableLabels.push_back ({ &saturationToneLabel, ovt::Keys::kLabelSaturationWarmth });

    // Saturation power toggle: a real parameter (saturation_enable), attached to the button
    // like every sibling effect in this block. Switching off greys both knobs and keeps their
    // values; the stage fades out over 25 ms, so no click is added on the way out.
    saturationEnableButton.setButtonText (ovt::tr (ovt::Keys::kLabelSaturation));
    saturationEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    saturationEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    saturationEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSaturationEnable));
    addAndMakeVisible (saturationEnableButton);

    // Delay: a tempo-synced ping-pong stage on the master output. The power toggle carries
    // the name, like every sibling effect in this block; switching off greys the division combo
    // and both knobs via refreshLabels() and keeps their values. Division is a note value synced
    // to the host tempo (the ARA source panel already reads it), so the items are musical
    // fractions - locale-independent glyphs, never translated, never dotted per language.
    setupKnob (delayMixSlider, &delayMixLabel, "Mix");
    delayMixSlider.setRange (0.0, 1.0, 0.01);
    delayMixSlider.setValue (0.30);   // matches the APVTS default of delay_mix
    delayMixSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    delayMixSlider.setPopupDisplayEnabled (true, false, this);
    delayMixSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    translatableLabels.push_back ({ &delayMixLabel, ovt::Keys::kLabelDelayMix });

    setupKnob (delayFeedbackSlider, &delayFeedbackLabel, "Feedback");
    delayFeedbackSlider.setRange (0.0, 1.0, 0.01);
    delayFeedbackSlider.setValue (0.35);   // matches the APVTS default of delay_feedback
    delayFeedbackSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    delayFeedbackSlider.setPopupDisplayEnabled (true, false, this);
    delayFeedbackSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + "%"; };
    translatableLabels.push_back ({ &delayFeedbackLabel, ovt::Keys::kLabelDelayFeedback });

    // Division combo: the same population pattern as shiftedVoicesBox above - addItemList with a
    // start id of 1, so ComboBoxAttachment maps item ids 1..5 straight onto the AudioParameterChoice
    // indices. Default index 2 ("1/4") = itemId 3, set without notification like shiftedVoicesBox.
    delayDivisionLabel.setText (ovt::tr (ovt::Keys::kLabelDelayDivision), juce::dontSendNotification);
    delayDivisionLabel.setJustificationType (juce::Justification::centred);
    delayDivisionLabel.setColour (juce::Label::textColourId, ebs::text());
    delayDivisionLabel.setFont (ovt::fontCaption());
    addAndMakeVisible (delayDivisionLabel);
    translatableLabels.push_back ({ &delayDivisionLabel, ovt::Keys::kLabelDelayDivision });

    // The division is a DETENTED knob (one step per note value), not a combo: the parameter is an
    // AudioParameterChoice whose normalisable range is 0..8 step 1, so a rotary SliderAttachment
    // snaps to the nine values and the popup reads them as fractions. The entries are ordered by
    // REPETITION LENGTH (shortest first), so turning the knob walks the echoes from a 1/8T slap to
    // a 1/2D long repeat. "D" is a dotted note and "T" a triplet, the DAW notation for the two.
    // The fractions themselves are musical notation, not prose: they stay identical in every locale.
    delayDivisionKnob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    delayDivisionKnob.setRange (0.0, 8.0, 1.0);
    delayDivisionKnob.setValue (4.0, juce::dontSendNotification);   // index 4 = "1/4", the APVTS default
    delayDivisionKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    delayDivisionKnob.setPopupDisplayEnabled (true, false, this);
    delayDivisionKnob.textFromValueFunction = [] (double v)
    {
        // Duration order, same as the APVTS choice list:
        // 1/8T, 1/8, 1/4T, 1/8D, 1/4, 1/2T, 1/4D, 1/2, 1/2D.
        static const char* const fractions[] = { "1/8T", "1/8", "1/4T", "1/8D", "1/4",
                                                 "1/2T", "1/4D", "1/2", "1/2D" };
        return juce::String (fractions[juce::jlimit (0, 8, juce::roundToInt (v))]);
    };
    delayDivisionKnob.setColour (juce::Slider::rotarySliderFillColourId, ebs::accent());
    delayDivisionKnob.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    delayDivisionKnob.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    addAndMakeVisible (delayDivisionKnob);

    // Delay power toggle: a real parameter (delay_enable), attached to the button like every
    // sibling effect in this block. Switching off greys the combo and both knobs, keeps their
    // values; the DSP fades its copies out, so no click is added on the way out.
    delayEnableButton.setButtonText (ovt::tr (ovt::Keys::kLabelDelay));
    delayEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    delayEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    addAndMakeVisible (delayEnableButton);

    // Every control of the Delay explains itself: the switch, the mix, the note value of the echo
    // and the repeats. The Delay was the one module whose controls had no tooltip at all, so a
    // user had to guess what DIVISION and FEEDBACK did.
    delayEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayEn));
    delayMixSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayMix));
    delayDivisionKnob.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayDivision));
    delayFeedbackSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipDelayFeedback));

    // "Use Voice": the shared iOS-style sliding switch. It paints only the pill,
    // so the caption is a separate label placed to its left in resized().
    useVoiceLabel.setText (ovt::tr(ovt::Keys::kLabelUseVoice), juce::dontSendNotification);
    useVoiceLabel.setColour (juce::Label::textColourId, ebs::text());
    useVoiceLabel.setFont (ovt::fontCaption());
    // Regular weight: the only bold voices in a frame are the spines' smallcaps and the mode pill.
    addAndMakeVisible (useVoiceLabel);
    useVoiceButton.setColour (ebs::ToggleSwitch::trackOnColourId,  ebs::accent());
    useVoiceButton.setColour (ebs::ToggleSwitch::trackOffColourId, ebs::text().withAlpha (0.18f));
    addAndMakeVisible (useVoiceButton);

    shiftedVoicesBox.addItemList ({ "1", "2", "3", "4" }, 1);
    shiftedVoicesBox.setSelectedId (4, juce::dontSendNotification);
    shiftedVoicesBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    shiftedVoicesBox.setColour (juce::ComboBox::textColourId, ebs::text());
    shiftedVoicesBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    addAndMakeVisible (shiftedVoicesBox);

    harmonyToneBox.addItemList ({ "Choir", "Organ" }, 1);
    harmonyToneBox.setSelectedItemIndex (0, juce::dontSendNotification);
    harmonyToneBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    harmonyToneBox.setColour (juce::ComboBox::textColourId, ebs::text());
    harmonyToneBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    addAndMakeVisible (harmonyToneBox);

    harmonyToneColorLabel.setText ("Tone", juce::dontSendNotification);
    harmonyToneColorLabel.setJustificationType (juce::Justification::centred);
    harmonyToneColorLabel.setColour (juce::Label::textColourId, ebs::text());
    harmonyToneColorLabel.setFont (ovt::fontCaption());
    addAndMakeVisible (harmonyToneColorLabel);

    harmonyToneColorSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    harmonyToneColorSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    harmonyToneColorSlider.setRange (0.0, 1.0, 0.01);
    harmonyToneColorSlider.setValue (0.5, juce::dontSendNotification);
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderFillColourId, ebs::accent());
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    harmonyToneColorSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    harmonyToneColorSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipToneColor));
    addAndMakeVisible (harmonyToneColorSlider);

    // Key and Scale are discrete values -> ComboBox.
    auto setupCombo = [this] (juce::ComboBox& b, juce::Label& l, const juce::String& name,
                              const juce::StringArray& items, int initialIndex)
    {
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, ebs::text());
        l.setFont (ovt::fontCaption());   // "Group" stays mixed case, in regular weight
        addAndMakeVisible (l);

        for (int i = 0; i < items.size(); ++i)
            b.addItem (items[i], i + 1);
        b.setSelectedItemIndex (initialIndex, juce::dontSendNotification);
        b.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
        b.setColour (juce::ComboBox::textColourId,       ebs::text());
        b.setColour (juce::ComboBox::outlineColourId,    ebs::accentSoft());
        b.setColour (juce::ComboBox::arrowColourId,      ebs::accent());
        addAndMakeVisible (b);
    };

    setupCombo (keyBox,   keyLabel,   "Root", kNoteNames,  0);
    translatableLabels.push_back ({ &keyLabel, ovt::Keys::kLabelRoot    });
    setupCombo (scaleBox, scaleLabel, "Scale", kScaleNames, 0);
    translatableLabels.push_back ({ &scaleLabel, ovt::Keys::kLabelScale    });

    // Key/Scale DETECTION source combo (Auto / OpenVoxKey / Sidechain). The old
    // "Manual" choice is gone: the power button below drives manual vs. detected.
    // The "Key Src" label is repurposed as the detection-row label (next to the
    // power button); the source combo has no label of its own.
    keySourceBox.addItemList (juce::StringArray { "Auto", "OpenVoxKey", "Sidechain" }, 1);
    keySourceBox.setSelectedItemIndex (0, juce::dontSendNotification);
    keySourceBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    keySourceBox.setColour (juce::ComboBox::textColourId,       ebs::text());
    keySourceBox.setColour (juce::ComboBox::outlineColourId,    ebs::accentSoft());
    keySourceBox.setColour (juce::ComboBox::arrowColourId,      ebs::accent());
    keySourceBox.setTooltip (ovt::tr (ovt::Keys::kTooltipKeySource));
    addAndMakeVisible (keySourceBox);

    // Line-1 label "Key/Scale Detection", sitting next to the power button.
    keySourceLabel.setText (ovt::tr (ovt::Keys::kLabelKeyDetect), juce::dontSendNotification);
    keySourceLabel.setJustificationType (juce::Justification::centredLeft);
    keySourceLabel.setColour (juce::Label::textColourId, ebs::text());
    addAndMakeVisible (keySourceLabel);   // drawn in the spines' caption typography, not in this font
    // Clicking the "Key/Scale Detection" label toggles the power button, matching
    // the behaviour of the other Power buttons (whose label is drawn inside them).
    keySourceLabel.addMouseListener (this, false);
    translatableLabels.push_back ({ &keySourceLabel, ovt::Keys::kLabelKeyDetect    });

    // Companion group (A/B/C/D) - shown only when the source is OpenVoxKey.
    setupCombo (companionGroupBox, companionGroupLabel, "Group",
                juce::StringArray { "A", "B", "C", "D" }, 0);
    translatableLabels.push_back ({ &companionGroupLabel, ovt::Keys::kLabelCompanionGroup    });

    // Power-icon toggle for Key/Scale detection (on/off). No text: the label
    // next to it carries the "Key/Scale Detection" wording.
    keyDetectPowerButton.setButtonText (juce::String());
    keyDetectPowerButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    keyDetectPowerButton.setColour (juce::ToggleButton::tickColourId,  ebs::accent());
    keyDetectPowerButton.setTooltip (ovt::tr (ovt::Keys::kTooltipKeyDetect));
    addAndMakeVisible (keyDetectPowerButton);
    keyDetectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
        (processorRef.getParameters(), "key_detect", keyDetectPowerButton);

    // Show the Companion Group control only when detection is on AND the source is
    // OpenVoxKey; re-layout so the second row appears / disappears. Expanding the
    // block already calls resized(); the power button and combo also trigger it.
    keySourceBox.onChange = [this] {
        companionGroupBox.setVisible (keyDetectPowerButton.getToggleState()
                                       && keySourceBox.getSelectedItemIndex() == 1);
        resized();
    };
    keyDetectPowerButton.onClick = [this] { resized(); };
    // The caption is the switch's name: it brightens with the switch, like every other switch name
    // in the row. Read at paint time, so the button stays the only source of truth.
    keySourceLabel.setEngagedSource ([this] { return keyDetectPowerButton.getToggleState(); });

    latencyModeLabel.setText ("", juce::dontSendNotification);
    latencyModeLabel.setJustificationType (juce::Justification::centred);
    latencyModeLabel.setColour (juce::Label::textColourId, ebs::text());
    latencyModeLabel.setFont (ovt::fontVersion());
    latencyModeLabel.setVisible (false);

    latencyModeBox.addItemList ({ "Direct Monitoring", "Low Latency", "Quality", "Safe" }, 1);
    latencyModeBox.setSelectedItemIndex (2, juce::dontSendNotification);
    latencyModeBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgPanel());
    latencyModeBox.setColour (juce::ComboBox::textColourId, ebs::text());
    latencyModeBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    latencyModeBox.setColour (juce::ComboBox::arrowColourId, ebs::accent());
    addAndMakeVisible (latencyModeBox);

    // === Hamburger menu button (gear icon) ===
    menuButton.onClick = [this]
    {
        juce::PopupMenu menu;

        // 1. Latency submenu: three modes (Fast = internal 10 ms, Standard = 20 ms,
        // High quality = 30 ms). The old 12 ms entry was removed; the parameter keeps
        // four values for state compatibility, so the legacy value 1 is migrated to
        // Fast when a state/preset/slot is loaded (see PluginProcessor). The suffix is
        // the DECLARED latency (the measured effective delay for the selected Voice
        // Type, Source/dsp/LatencyDeclaration.h), not the internal mode value: it
        // changes with the register, and it is what the host compensates.
        int declaredVoiceType = 0;
        if (auto* vp = processorRef.getParameters().getParameter ("voice_type"))
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (vp))
                declaredVoiceType = choice->getIndex();

        auto withDeclaredMs = [declaredVoiceType] (const char* key, int mode)
        {
            return ovt::tr (key) + "  (~"
                 + juce::String (ovtdsp::declaredLatencyMs (mode, declaredVoiceType)) + " ms)";
        };

        juce::PopupMenu latencyMenu;
        latencyMenu.addItem (withDeclaredMs (ovt::Keys::kMenuLatencyFast, 0), true, latencyModeBox.getSelectedId() == 1, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (0.0f);
    });
        latencyMenu.addItem (withDeclaredMs (ovt::Keys::kMenuLatencyStandard, 2), true, latencyModeBox.getSelectedId() == 3, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (2.0f / 3.0f);
    });
        latencyMenu.addItem (withDeclaredMs (ovt::Keys::kMenuLatencyHigh, 3), true, latencyModeBox.getSelectedId() == 4, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (3.0f / 3.0f);
    });
        menu.addSubMenu (ovt::tr(ovt::Keys::kMenuLatency), latencyMenu);

        menu.addSeparator();

        // MIDI OUT toggle
        {
            bool midiOn = processorRef.getParameters().getParameter ("midi_out_enable")->getValue() > 0.5f;
            menu.addItem (ovt::tr(ovt::Keys::kMenuMidiOut), true, midiOn, [this] {
                if (auto* p = processorRef.getParameters().getParameter ("midi_out_enable"))
                    p->setValueNotifyingHost (1.0f - p->getValue());
    });
        }

        // Tuning follows MIDI IN toggle : an incoming held MIDI note
        // drives the correction target (the voice is tuned to the note).
        {
            bool midiTargetOn = processorRef.getParameters().getParameter ("midi_target_enable")->getValue() > 0.5f;
            menu.addItem (ovt::tr(ovt::Keys::kMenuMidiTarget), true, midiTargetOn, [this] {
                if (auto* p = processorRef.getParameters().getParameter ("midi_target_enable"))
                    p->setValueNotifyingHost (1.0f - p->getValue());
    });
        }

        menu.addSeparator();

        // Interface submenu
        {
            juce::PopupMenu interfaceMenu;

            // Theme toggle
            {
                juce::PopupMenu themeMenu;
                const bool isDark = ebs::isDark();
                themeMenu.addItem (ovt::tr(ovt::Keys::kMenuDarkTheme), true, isDark, [this] {
                    if (auto* p = processorRef.getParameters().getParameter ("ui_theme"))
                        p->setValueNotifyingHost (0.0f);
                    ebs::setTheme (ebs::Theme::Dark);   // broadcast re-applies everything
    });
                themeMenu.addItem (ovt::tr(ovt::Keys::kMenuLightTheme), true, !isDark, [this] {
                    if (auto* p = processorRef.getParameters().getParameter ("ui_theme"))
                        p->setValueNotifyingHost (1.0f);
                    ebs::setTheme (ebs::Theme::Light);
    });
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuTheme), themeMenu);
            }

            // Language selector
            {
                juce::PopupMenu langMenu;
                const auto currentLang = ovt::currentLanguage();
                auto addLangItem = [&](ovt::Language lang, int langIdx)
                {
                    const juce::String label = ovt::languageDisplayName (lang);
                    langMenu.addItem (label, true, (currentLang == lang), [this, lang, langIdx] {
                        ovt::currentLanguage() = lang;
                        if (auto* p = processorRef.getParameters().getParameter ("ui_language"))
                            p->setValueNotifyingHost ((float) langIdx / 5.0f);
                        repaint();
                        refreshLabels();
                        resized();
    });
                };
                addLangItem (ovt::Language::English,  0);
                addLangItem (ovt::Language::French,   1);
                addLangItem (ovt::Language::German,   2);
                addLangItem (ovt::Language::Spanish,  3);
                addLangItem (ovt::Language::Japanese, 4);
                addLangItem (ovt::Language::Chinese,  5);
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuLanguage), langMenu);
            }

            interfaceMenu.addSeparator();

            // Show Waveform toggle
            interfaceMenu.addItem (ovt::tr(ovt::Keys::kMenuShowWaveform), true, showWaveform, [this] {
                showWaveform = ! showWaveform;
                if (auto* p = processorRef.getParameters().getParameter ("ui_show_waveform"))
                    p->setValueNotifyingHost (showWaveform ? 1.0f : 0.0f);
                processorRef.setWaveformCaptureEnabled (showWaveform);
                if (! showWaveform)
                {
                    if (pitchVisualizer != nullptr)
                        pitchVisualizer->setWaveformOverlay (nullptr, 0, 44100.0);
                    if (curveEditor != nullptr)
                        curveEditor->setWaveformOverlay (nullptr, 0, 44100.0);
                }
    });

            // Waveform display type submenu
            {
                juce::PopupMenu waveformMenu;
                const int currentType = processorRef.getWaveformDisplayType();
                waveformMenu.addItem (ovt::tr(ovt::Keys::kMenuWaveformLine), true, currentType == 0, [this] { setWaveformDisplayType (0);    });
                waveformMenu.addItem (ovt::tr(ovt::Keys::kMenuWaveformMirror), true, currentType == 1, [this] { setWaveformDisplayType (1);    });
                waveformMenu.addItem (ovt::tr(ovt::Keys::kMenuWaveformSpectral), true, currentType == 2, [this] { setWaveformDisplayType (2);    });
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuWaveformDisplay), waveformMenu, true);
            }

            interfaceMenu.addSeparator();

            // Auto-Center Pitch Display (Live tab only)
            if (pitchVisualizer != nullptr)
            {
                const bool acEnabled = pitchVisualizer->isAutoCenter();
                interfaceMenu.addItem ("Auto-Center Pitch", true, acEnabled, [this] {
                    if (pitchVisualizer != nullptr)
                    {
                        pitchVisualizer->setAutoCenter (! pitchVisualizer->isAutoCenter());
                        if (auto* p = processorRef.getParameters().getParameter ("ui_auto_center"))
                            p->setValueNotifyingHost (pitchVisualizer->isAutoCenter() ? 1.0f : 0.0f);
                    }
                });
            }

            interfaceMenu.addSeparator();

            // Tooltip delay submenu. Slow=1500ms (more contemplative,
            // useful when demoing the plugin), Normal=700ms (JUCE default),
            // Fast=200ms (snappy, for power users). The choice is applied
            // live to the global TooltipWindow so the user sees the effect
            // on the very next hover, no editor restart required.
            {
                juce::PopupMenu tooltipMenu;
                auto addDelayItem = [&](int idx, const char* key)
                {
                    tooltipMenu.addItem (ovt::tr (key), true,
                                         (tooltipDelayChoice == idx),
                                         [this, idx] { setTooltipDelayChoice (idx); });
                };
                addDelayItem (0, ovt::Keys::kLabelTooltipDelaySlow);
                addDelayItem (1, ovt::Keys::kLabelTooltipDelayNormal);
                addDelayItem (2, ovt::Keys::kLabelTooltipDelayFast);
                interfaceMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuTooltipDelay), tooltipMenu);
            }

            interfaceMenu.addSeparator();

            // Keyboard Shortcuts
            interfaceMenu.addItem (ovt::tr(ovt::Keys::kMenuKeyboardShortcuts), [this] { toggleHelpOverlay();    });

            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuInterface), interfaceMenu);
        }

        menu.addSeparator();

        const bool isStandalone = processorRef.isStandaloneWrapper();

        // Advanced submenu
        {
            juce::PopupMenu advancedMenu;

            // Export as Image...
            advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuExportImage), [this] {
                const bool isCurveEditor = (tabbedComponent.getCurrentTabIndex() == 1);
                auto* pitchViz = dynamic_cast<ui::PitchVisualizer*> (tabbedComponent.getTabContentComponent (0));

                if ((isCurveEditor && curveEditor == nullptr) || (!isCurveEditor && pitchViz == nullptr))
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                        ovt::tr(ovt::Keys::kDlgExport), ovt::tr(ovt::Keys::kDlgExportNotFound));
                    return;
                }

                auto defaultDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
                auto downloadsDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                        .getChildFile ("Downloads");
                if (downloadsDir.isDirectory())
                    defaultDir = downloadsDir;

                juce::Component::SafePointer<ui::PitchVisualizer> pitchVizSafe (pitchViz);
                juce::Component::SafePointer<ui::PitchCurveEditor> curveEditorSafe (curveEditor.get());
                auto chooserPtr = std::make_shared<juce::FileChooser>(
                    ovt::tr(ovt::Keys::kDlgExportPng),
                    defaultDir,
                    "*.png");
                chooserPtr->launchAsync (
                    juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                    [chooserPtr, isCurveEditor, pitchVizSafe, curveEditorSafe] (const juce::FileChooser& fc) {
                        auto file = fc.getResult();
                        if (file != juce::File{})
                        {
                            if (file.getFileExtension() != ".png")
                                file = file.withFileExtension (".png");

                            bool ok = false;
                            if (isCurveEditor && curveEditorSafe != nullptr)
                                ok = curveEditorSafe->exportAsImage (file);
                            else if (pitchVizSafe != nullptr)
                                ok = pitchVizSafe->exportAsImage (file);

                            if (ok)
                                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                    ovt::tr(ovt::Keys::kDlgExport), ovt::tr(ovt::Keys::kDlgImageSaved) + file.getFullPathName());
                            else
                                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                    ovt::tr(ovt::Keys::kDlgExport), ovt::tr(ovt::Keys::kDlgImageFailed));
                        }
    });
    });

            // Formant Mode and Pitch Detection are hidden for now - may be
            // re-enabled later when additional modes become available.

            // Audio/MIDI setup (standalone only): the system title bar removed the JUCE-drawn
            // caption that used to carry the device settings button, so the dialog is opened
            // from here. JUCE owns the dialog through its standalone holder.
            if (isStandalone)
            {
                advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuAudioMidiSetup), [this]
                {
                   #if JUCE_STANDALONE_APPLICATION
                    if (auto* holder = juce::StandalonePluginHolder::getInstance())
                        holder->showAudioSettingsDialog();
                   #endif
                });
            }

            // MIDI Learn (standalone only)
            if (isStandalone)
            {
                juce::PopupMenu midiLearnMenu;
                struct ParamEntry { const char* id; std::string name; };
                const ParamEntry params[] = {
                    {"speed", ovt::tr(ovt::Keys::kMidiLearnSpeed).toStdString()},
                    {"amount", ovt::tr(ovt::Keys::kMidiLearnAmount).toStdString()},
                    {"formant", ovt::tr(ovt::Keys::kMidiLearnFormant).toStdString()},
                    {"lead_room_send", ovt::tr(ovt::Keys::kMidiLearnLeadRoom).toStdString()},
                    {"lead_hall_send", ovt::tr(ovt::Keys::kMidiLearnLeadHall).toStdString()},
                    {"harmony_room_send", ovt::tr(ovt::Keys::kMidiLearnHarmonyRoom).toStdString()},
                    {"harmony_hall_send", ovt::tr(ovt::Keys::kMidiLearnHarmonyHall).toStdString()},
                    {"reverb_duck_depth", ovt::tr(ovt::Keys::kMidiLearnReverbDucking).toStdString()},
                    {"output_trim", ovt::tr(ovt::Keys::kMidiLearnOutputTrim).toStdString()},
                    {"flex_tune", ovt::tr(ovt::Keys::kMidiLearnFlexTune).toStdString()},
                    {"humanize", ovt::tr(ovt::Keys::kMidiLearnHumanize).toStdString()},
                    {"vibrato_preserve", ovt::tr(ovt::Keys::kMidiLearnVibrato).toStdString()},
                    {"attack_aware", ovt::tr(ovt::Keys::kMidiLearnAttack).toStdString()},
                    {"attack_release", ovt::tr(ovt::Keys::kMidiLearnAttackRelease).toStdString()},
                    {"harmony_gain", ovt::tr(ovt::Keys::kMidiLearnHarmonyGain).toStdString()},
                    {"harmony_blend", ovt::tr(ovt::Keys::kMidiLearnHarmonyBlend).toStdString()},
                    {"harmony_tone_color", ovt::tr(ovt::Keys::kMidiLearnHarmonyTone).toStdString()},
                    {"delay_mix", ovt::tr(ovt::Keys::kMidiLearnDelayMix).toStdString()}
                };
                for (const auto& p : params)
                {
                    midiLearnMenu.addItem (juce::String (p.name), [this, id = juce::String (p.id)] {
                        startMidiLearn (id);
                    });
                }
                advancedMenu.addSubMenu (ovt::tr(ovt::Keys::kMenuMidiLearn), midiLearnMenu);
            }

            // Chord Detection (ovtchord): real-time chord-aware tuning from the
            // MIDI input ("Tuning follows MIDI IN") and/or the sidechain bus.
            {
                bool chordOn = processorRef.getParameters().getParameter ("chord_detect_enable")->getValue() > 0.5f;
                advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuChordDetection), true, chordOn, [this] {
                    if (auto* p = processorRef.getParameters().getParameter ("chord_detect_enable"))
                        p->setValueNotifyingHost (1.0f - p->getValue());
                });
            }

            // Reset to Default
            advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuResetDefault), [this] {
                juce::PopupMenu confirmMenu;
                confirmMenu.addItem (ovt::tr(ovt::Keys::kMenuCancel), []{    });
                confirmMenu.addSeparator();
                confirmMenu.addItem (ovt::tr(ovt::Keys::kMenuConfirmReset), [this] {
                    auto& paramTree = processorRef.getParameters().state;
                    for (int i = 0; i < paramTree.getNumChildren(); ++i)
                    {
                        auto id = paramTree.getChild(i).getProperty ("id").toString();
                        if (auto* param = processorRef.getParameters().getParameter (id))
                            param->setValueNotifyingHost (param->getDefaultValue());
                    }
    });
                applyMenuLookAndFeel (confirmMenu, customLookAndFeel);
                confirmMenu.showMenuAsync (juce::PopupMenu::Options()
                    .withTargetComponent (&menuButton)
                    .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards));
    });

            // Bypass (standalone only)
            if (processorRef.isStandaloneWrapper())
            {
                bool bypassOn = processorRef.getParameters().getParameter ("bypass")->getValue() > 0.5f;
                advancedMenu.addItem (ovt::tr(ovt::Keys::kMenuBypass), true, bypassOn, [this] {
                    if (auto* p = processorRef.getParameters().getParameter ("bypass"))
                        p->setValueNotifyingHost (1.0f - p->getValue());
    });
            }

            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuAdvanced), advancedMenu);
        }

        menu.addSeparator();

        // Check for Updates
        menu.addItem (ovt::tr(ovt::Keys::kMenuCheckUpdates), [this] {
            updateButton.onClick();
    });

       #if JUCE_DEBUG
        menu.addItem (ovt::tr(ovt::Keys::kMenuDebugWindow), [this] { debugWindowButton.onClick();    });
       #endif


        applyMenuLookAndFeel (menu, customLookAndFeel);
        auto opts = juce::PopupMenu::Options();
        if (! pendingMenuScreenPos.isOrigin())
        {
            opts = opts.withTargetScreenArea (juce::Rectangle<int> (
                pendingMenuScreenPos.x, pendingMenuScreenPos.y, 1, 1));
            pendingMenuScreenPos = {};
        }
        else
        {
            opts = opts.withTargetComponent (&menuButton)
                       .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards);
        }
        menu.showMenuAsync (opts);
    };
    addAndMakeVisible (menuButton);

    // === A/B Comparison buttons ===
    // Colours come from ABTextButton::paint tokens directly; a repaint on
    // theme switches keeps them current (see applyThemeToAllComponents).
    auto setupABButton = [this] (ABTextButton& btn, const juce::String& label, int slotIdx) {
        btn.setTooltip (label == "A" ? ovt::tr(ovt::Keys::kTooltipAbSlotA) : ovt::tr(ovt::Keys::kTooltipAbSlotB));
        btn.onClick = [this, slotIdx] {
            auto& clickedSlot = (slotIdx == 0) ? slotA : slotB;

            // Auto-save the CURRENT slot before loading the other one.
            // Only save if the current state matches this slot's original state
            // (morph slider at the correct endpoint), not a morphed blend.
            // Always commit the CURRENT slot's state when switching away, so
            // edits made on it are never lost. Capture the LIVE parameters
            // directly. Do NOT snap the morph first: snapping fires
            // onMorphSliderChanged, which would re-apply the slot's stale
            // stored state via morphSource and overwrite the live edit before
            // capture (exactly the "slot A reset to defaults" bug seen).
            {
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
                auto* lp = processorRef.getParameters().getParameter ("vibrato_preserve");
                auto* lh = processorRef.getParameters().getParameter ("humanize");
                auto* lf = processorRef.getParameters().getParameter ("harmony_formant");
                auto* lt = processorRef.getParameters().getParameter ("harmony_type");
                juce::Logger::writeToLog (
                    "[DIAG] pre-save live: vibrato=" + juce::String (lp ? lp->getValue() : -1.0f, 4)
                    + " humanize=" + juce::String (lh ? lh->getValue() : -1.0f, 4)
                    + " formant=" + juce::String (lf ? lf->getValue() : -1.0f, 4)
                    + " type=" + juce::String (lt ? (int) std::round (lt->getValue() * 21.0f) : -1));
#endif
                auto& currentSlot = isSlotAActive ? slotA : slotB;
                const int currentIdx = isSlotAActive ? 0 : 1;
                saveSlot (currentSlot, currentIdx);
            }

            // Initialize the clicked slot if empty (first click)
            if (! clickedSlot.hasData)
                saveSlot (clickedSlot, slotIdx);

            // Clear old morph state before loading
            morphSource.reset();
            morphTarget.reset();
            morphUndoState.reset();
            // Drop the external-automation exclusion baseline. loadSlot() below
            // overwrites the parameters with the slot's saved values; without
            // clearing this map the next slider move would treat every changed
            // parameter as "externally driven" and exclude it from the morph,
            // which (once excluded, a parameter's baseline is never refreshed)
            // progressively kills the slider after a few A<->B toggles.
            lastMorphIntendedValues.clear();

            // Load the clicked slot
            loadSlot (clickedSlot);
            isSlotAActive = (slotIdx == 0);
            processorRef.setAbActiveSlot (slotIdx);

            // Set up morph state: source = A (slider left), target = B (slider right).
            // This is always the same regardless of which slot is active.
            if (slotA.morphState != nullptr && slotB.morphState != nullptr)
            {
                morphSource = std::make_unique<ovtdsp::MorphState> (*slotA.morphState);
                morphTarget = std::make_unique<ovtdsp::MorphState> (*slotB.morphState);
            }

            // Position slider: A=left (0), B=right (1)
            processorRef.setMorphAmount (slotIdx == 0 ? 0.0f : 1.0f);

            // Reflect the (now active) slot's assigned plugin preset in the
            // preset selector. If the slot still matches its preset exactly
            // (no user edits since it was loaded) show that preset; otherwise
            // it is considered "User" (detached from the preset).
            auto& activeSlot = isSlotAActive ? slotA : slotB;
            if (slotMatchesPreset (activeSlot))
            {
                currentPluginPresetName = activeSlot.presetName;
                presetComboBox.setText (ebs::sanitizeForShaping (activeSlot.presetName), juce::dontSendNotification);
            }
            else
            {
                currentPluginPresetName = "User";
                presetComboBox.setText ("User", juce::dontSendNotification);
            }

            updateABButtonStates();
        };
        addAndMakeVisible (btn);
    };
    setupABButton (buttonA, "A", 0);
    setupABButton (buttonB, "B", 1);
    // Right-click on A/B saves the current state into that slot (detaches it
    // from any named plugin preset, since it is now an explicit manual state).
    buttonA.onRightClick = [this] { saveSlot (slotA, 0); slotA.presetName = ""; updateABButtonStates(); };
    buttonB.onRightClick = [this] { saveSlot (slotB, 1); slotB.presetName = ""; updateABButtonStates(); };
    // Button A always has valid data (it's the default state)
    buttonA.hasValidData = true;
    buttonA.isActive = true;

    // === Morph Slider ===
    morphSlider.setName ("Morph");   // kept for component identification/debugging
    morphSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    morphSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    morphSlider.setRange (0.0, 1.0, 0.01);
    morphSlider.setColour (juce::Slider::thumbColourId, ebs::accent());
    morphSlider.setColour (juce::Slider::trackColourId, ebs::accent().withAlpha (0.7f));
    morphSlider.setColour (juce::Slider::backgroundColourId, ebs::bgPanel());
    morphSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphDrag));
    // Morph is now a host-automatable parameter ("morph_amount"). The slider is
    // bound to it via this attachment, so DAW automation and the slider stay in
    // sync. The actual morph application is driven from timerCallback().
    morphAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
        (processorRef.getParameters(), "morph_amount", morphSlider);
    addAndMakeVisible (morphSlider);
    morphSlider.addMouseListener (this, false); // for right-click context menu

    morphSliderLabel.setText (ovt::tr(ovt::Keys::kLabelMorph), juce::dontSendNotification);
    morphSliderLabel.setFont (ovt::fontLegendHint());
    morphSliderLabel.setColour (juce::Label::textColourId, ebs::textDim());
    morphSliderLabel.setTooltip (ovt::tr(ovt::Keys::kTooltipMorphLabel));
    morphSliderLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (morphSliderLabel);

    // === Slider ranges ===
    speedSlider.setRange (0.0, 200.0, 1.0);
    speedSlider.setValue (50.0);
    amountSlider.setRange (0.0, 1.0, 0.01);
    amountSlider.setValue (1.0);

    // === Bindings for the Key/Scale ComboBoxes to their parameters ===
    // ComboBoxAttachment keeps the combo box and the host parameter in sync in BOTH
    // directions: parameter -> combo box (so morph/automation correctly drives the displayed
    // scale/key) and combo box -> parameter on user selection. It uses the ComboBox Listener
    // mechanism (addListener / comboBoxChanged) to write the parameter on a genuine user
    // selection; this is robust and idempotent with whatever we do below.
    // IMPORTANT: when the attachment drives the combo (morph / host automation) it updates the
    // displayed index with sendNotificationSync. JUCE guards the Listener with an internal
    // ignoreCallbacks flag, but the onChange callback is NOT guarded, so onChange ALSO fires in
    // that case. Therefore the onChange handlers below must NOT write the parameter (that would
    // fight the morph / automation, since the index is read back from the combo display which can
    // momentarily lag the morph target during a crossfade). onChange only mirrors the per-note
    // custom flags / piano keys for whatever scale/key the combo currently shows.
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "key", keyBox);
    // The ComboBoxAttachment writes the "key" parameter on user selection (via its Listener).
    // Here we only re-sync the per-note custom flags / piano keys for the new key. We must not
    // write the parameter here (see the note above about morph/automation driving the combo).
    keyBox.onChange = [this] {
        const int scaleIdx = scaleBox.getSelectedItemIndex();
        if (scaleIdx >= 0 && scaleIdx != 13)
            scaleBox.onChange(); // Re-sync intervals/piano for the new key
    };

    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "scale", scaleBox);
    // The ComboBoxAttachment writes the "scale" parameter on user selection (via its Listener),
    // so the engine and the curve editor snap always use the selected scale. This onChange
    // handler must NOT also write the parameter: it fires whenever the attachment drives the
    // combo (morph / host automation) via sendNotificationSync, and writing back would fight the
    // morph (the index comes from the combo display, which can lag the morph target during a
    // crossfade). Here we only keep the per-note custom flags / piano keys in sync with the
    // selected preset scale.
    scaleBox.onChange = [this] {
        const int idx = scaleBox.getSelectedItemIndex();
        if (idx < 0)
            return;
        if (idx != 13) // Not Custom (Custom keeps the user's custom note flags)
        {
            // Suppress onUserInteraction callbacks on the piano keys during the
            // programmatic sync, so selecting a preset from the combo does NOT
            // re-trigger the "switch to Custom" logic (which would set the combo
            // back to "Custom" and cancel the user's preset selection).
            scaleKeyboard.setUpdatingFromScaleCombo (true);
            const int keyIdx = processorRef.getKeyIndex();

            ovtdsp::ScaleQuantizer tempQuantizer;
            tempQuantizer.setKey (keyIdx);
            tempQuantizer.setScale (static_cast<ovtdsp::Scale> (juce::jlimit (0, 13, idx)));
            auto intervals = tempQuantizer.getScaleIntervals ();

            for (int i = 0; i < 12; ++i)
            {
                auto* p = processorRef.getParameters().getParameter ("custom" + juce::String (i));
                if (p != nullptr)
                {
                    float targetVal = intervals.contains (i) ? 1.0f : 0.0f;
                    if (p->getValue() != targetVal)
                        p->setValueNotifyingHost (targetVal);
                }
            }
            scaleKeyboard.setUpdatingFromScaleCombo (false);
        }
    };
    latencyModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "latency_mode", latencyModeBox);
    detectorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "pitch_detector", detectorBox);
    keySourceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "key_source", keySourceBox);
    companionGroupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "companion_group", companionGroupBox);

    // Reverb bus attachments: one switch and two send amounts per voice, plus the ducker.
    leadReverbEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "lead_reverb_enable", leadReverbEnableButton);
    harmonyReverbEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "harmony_reverb_enable", harmonyReverbEnableButton);
    leadRoomSendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "lead_room_send", leadRoomSlider);
    leadHallSendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "lead_hall_send", leadHallSlider);
    harmonyRoomSendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "harmony_room_send", harmonyRoomSlider);
    harmonyHallSendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "harmony_hall_send", harmonyHallSlider);
    limiterEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "limiter_enable", limiterEnableButton);
    outputTrimAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "output_trim", outputTrimSlider);

    duckDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "reverb_duck_depth", duckDepthSlider);

    noiseGateEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getParameters(), "noise_gate_enable", noiseGateEnableButton);
    noiseGateThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getParameters(), "noise_gate_threshold", noiseGateThresholdSlider);

    // Upward Compressor attachments
    upwardCompEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getParameters(), "upward_comp_enable", upwardCompEnableButton);
    upwardCompAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.getParameters(), "upward_comp_amount", upwardCompAmountSlider);

    // FlexTune / Humanize / Vibrato / Correction Mode attachments
    // 2026-07-24 (Deprecation): FlexTune and Attack-Aware attachments
    // are disabled (see PluginEditor.h). The APVTS parameters still
    // exist for preset compatibility, but the UI sliders/buttons are
    // not attached and not visible.
    // flexTuneAttachment = std::make_unique<...> (...);  // DEPRECATED
    humanizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "humanize", humanizeSlider);
    vibratoPreserveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "vibrato_preserve", vibratoPreserveSlider);
    // attackAwareAttachment = std::make_unique<...> (...);   // DEPRECATED
    // attackReleaseAttachment = std::make_unique<...> (...); // DEPRECATED
    correctionModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "correction_mode", correctionModeButton);
    hardTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "hard_tune", hardTuneButton);

    // === Voice Type combo (Correction block, Advanced area, row 2) ===
    // Same visual style as the other dropdowns (Scale, Key, etc.).
    voiceTypeBox.addItemList ({ "Universal", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 1);
    voiceTypeBox.setSelectedItemIndex (0, juce::dontSendNotification);
    voiceTypeBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgDark());
    voiceTypeBox.setColour (juce::ComboBox::textColourId, ebs::text());
    voiceTypeBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    voiceTypeBox.setColour (juce::ComboBox::arrowColourId, ebs::accent());
    voiceTypeBox.setColour (juce::PopupMenu::backgroundColourId, ebs::bgDark());
    voiceTypeBox.setColour (juce::PopupMenu::textColourId, ebs::text());
    voiceTypeBox.setTooltip (ovt::tr (ovt::Keys::kTooltipVoiceType));
    voiceTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "voice_type", voiceTypeBox);
    addAndMakeVisible (voiceTypeBox);

    voiceTypeLabel.setText (ovt::tr (ovt::Keys::kVoiceTypeLabel), juce::dontSendNotification);
    voiceTypeLabel.setJustificationType (juce::Justification::left);
    voiceTypeLabel.setColour (juce::Label::textColourId, ebs::textDim());
    voiceTypeLabel.setFont (ovt::fontLegendHint());
    voiceTypeLabel.setTooltip (ovt::tr (ovt::Keys::kTooltipVoiceType));
    addAndMakeVisible (voiceTypeLabel);

    // Register the label so refreshLabels() updates it on language switch.
    translatableLabels.push_back ({ &voiceTypeLabel, ovt::Keys::kVoiceTypeLabel });

    // === Formant Strategy combo (Advanced area, row 3) ===
    // Selects the formant-preservation method. Each strategy has a distinct
    // character; the tooltip below summarises them in plain language.
    formantStrategyBox.addItemList ({
        ovt::tr (ovt::Keys::kFormantStrategySubtle),
        ovt::tr (ovt::Keys::kFormantStrategyBalanced),
        ovt::tr (ovt::Keys::kFormantStrategyMarked),
        ovt::tr (ovt::Keys::kFormantStrategyReactive),
        ovt::tr (ovt::Keys::kFormantStrategyPrecise) }, 1);
    formantStrategyBox.setSelectedItemIndex (4, juce::dontSendNotification);  // Precise is default
    formantStrategyBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgDark());
    formantStrategyBox.setColour (juce::ComboBox::textColourId, ebs::text());
    formantStrategyBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    formantStrategyBox.setColour (juce::ComboBox::arrowColourId, ebs::accent());
    formantStrategyBox.setColour (juce::PopupMenu::backgroundColourId, ebs::bgDark());
    formantStrategyBox.setColour (juce::PopupMenu::textColourId, ebs::text());
    formantStrategyBox.setTooltip (ovt::tr (ovt::Keys::kTooltipFormantStrategy));
    formantStrategyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "formant_strategy", formantStrategyBox);
    addAndMakeVisible (formantStrategyBox);

    formantStrategyLabel.setText (ovt::tr (ovt::Keys::kLabelFormantStrategy), juce::dontSendNotification);
    formantStrategyLabel.setJustificationType (juce::Justification::left);
    formantStrategyLabel.setColour (juce::Label::textColourId, ebs::textDim());
    formantStrategyLabel.setFont (ovt::fontLegendHint());
    formantStrategyLabel.setTooltip (ovt::tr (ovt::Keys::kLabelFormantStrategy));
    addAndMakeVisible (formantStrategyLabel);

    // Register for live language switch.
    translatableLabels.push_back ({ &formantStrategyLabel, ovt::Keys::kLabelFormantStrategy });

    // UI updates (visibility of custom buttons, etc.) are handled in timerCallback.

    // === SVG icons as full XML strings (Lucide-style, 24x24 viewBox) ===
    // Each uses stroke="#010101" as a placeholder for per-state coloring.

    // The toolbar glyphs that IconButton does not carry are gone: zoom, scroll,
    // reset, gear, hamburger, grid, save, undo / redo and the preset chevrons
    // all come from ebs::IconButton now. These six stay for the toggle buttons
    // below, which need a solid accent pill on their ON state (snap, grid, step,
    // bypass, MIDI out) plus the play / stop pair.
    static const char* svgScale = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="7" cy="18" r="3"/><path d="M10 18V4l11-2v15"/></svg>)";

    static const char* svgGrid = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/></svg>)";

    static const char* svgStep = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20v-5h6v-6h6V4h4"/></svg>)";

    static const char* svgPower = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2v10"/><path d="M18.36 6.64a9 9 0 1 1-12.73 0"/></svg>)";

    // Transport (standalone only).
    static const char* svgPlay = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polygon points="6 4 20 12 6 20 6 4"/></svg>)";
    static const char* svgStop = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="6" width="12" height="12" rx="1"/></svg>)";

    // Setup Toolbar Buttons
    // Curve Editor "Options" and Preset Gallery: framed icon buttons from the
    // shared library. Framed mode gives them the pressable affordance the old
    // accent-tinted drawable background provided, and the glyph ink stays
    // palette-aware without a restyling pass.
    setupFramedIcon (optionsButton, ovt::tr(ovt::Keys::kTooltipCurveOptions));
    optionsButton.onClick = [this] { showCurveOptionsMenu(); };

    // Preset Gallery toolbar button: icon-only grid, opens the browsable gallery.
    setupFramedIcon (presetGalleryButton, ovt::tr(ovt::Keys::kTooltipPresetGallery));
    presetGalleryButton.onClick = [this] { showPresetGallery(); };

    setupIconButton(snapButton, svgScale, true, "Snap to scale");
    snapButton.setToggleState(true, juce::dontSendNotification);
    snapButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setSnapEnabled(snapButton.getToggleState());
    };
    snapButton.setTooltip(ovt::tr(ovt::Keys::kTooltipSnapToScale));

    setupIconButton(snapGridButton, svgGrid, true, "Snap to grid");
    snapGridButton.setToggleState(false, juce::dontSendNotification);
    snapGridButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setSnapToGridEnabled(snapGridButton.getToggleState());
    };
    snapGridButton.setTooltip(ovt::tr(ovt::Keys::kTooltipSnapToGrid));

    setupIconButton(stepModeButton, svgStep, true, "Step mode (staircase interpolation)");
    stepModeButton.setToggleState(true, juce::dontSendNotification);
    stepModeButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setStepModeEnabled(stepModeButton.getToggleState());
    };
    stepModeButton.setTooltip(ovt::tr(ovt::Keys::kTooltipStepMode));

    // Curve Editor view controls (mirror the Visualizer toolbar: zoom / scroll / reset).
    setupEbsIcon (zoomInButton, ovt::tr(ovt::Keys::kTooltipZoomIn));
    zoomInButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->zoomIn(); };

    setupEbsIcon (zoomOutButton, ovt::tr(ovt::Keys::kTooltipZoomOut));
    zoomOutButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->zoomOut(); };

    setupEbsIcon (scrollUpButton, ovt::tr(ovt::Keys::kTooltipScrollUp));
    scrollUpButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->scrollUp(); };

    setupEbsIcon (scrollDownButton, ovt::tr(ovt::Keys::kTooltipScrollDown));
    scrollDownButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->scrollDown(); };

    setupEbsIcon (resetViewButton, ovt::tr(ovt::Keys::kTooltipResetView));
    resetViewButton.onClick = [this] { if (curveEditor != nullptr) curveEditor->resetView(); };

    // "Measures" control: number of measures shown in the curve editor time window.
    // Moved here from the embedded editor controls so it does not cover the ruler.
    measuresLabel.setText (ovt::tr(ovt::Keys::kLabelMeasures), juce::dontSendNotification);
    measuresLabel.setJustificationType (juce::Justification::left);
    measuresLabel.setColour (juce::Label::textColourId, ebs::textDim());
    measuresLabel.setFont (ovt::fontMeasuresLabel());
    addAndMakeVisible (measuresLabel);

    // Standalone tempo field. The Options menu carries the usual values (60..180 in tens), but a
    // session at 137 BPM is not a menu item, and this register is no longer cosmetic: the timeline
    // advances at it, the Curve Editor draws its grid on it and the tempo-synced Delay reads it.
    // It is an editable label rather than a slider because a BPM is a NUMBER the user knows, not a
    // value to hunt for - one click, type, Enter. A DAW replaces this register with the host tempo
    // (an ARA source with its tempo map), so the field is standalone-only, like the menu entry.
    bpmLabel.setJustificationType (juce::Justification::centred);
    bpmLabel.setColour (juce::Label::textColourId, ebs::text());
    bpmLabel.setColour (juce::Label::backgroundColourId, ebs::bgDark());
    bpmLabel.setColour (juce::Label::outlineColourId, ebs::accentSoft());
    bpmLabel.setColour (juce::Label::textWhenEditingColourId, ebs::text());
    bpmLabel.setColour (juce::Label::backgroundWhenEditingColourId, ebs::bgPanel());
    bpmLabel.setFont (ovt::fontMeasuresLabel());
    bpmLabel.setEditable (true, true, false);   // one click starts editing; losing focus keeps it
    bpmLabel.setTooltip (ovt::tr (ovt::Keys::kMenuTempo));
    bpmLabel.setText (juce::String (juce::roundToInt (processorRef.getBpm())) + " BPM", juce::dontSendNotification);
    bpmLabel.onTextChange = [this]
    {
        // Accept "137", "137 BPM" or anything starting with a number; the processor clamps 20..400.
        const float typed = bpmLabel.getText().getFloatValue();
        if (typed > 0.0f)
            processorRef.setBpm (typed);

        bpmLabel.setText (juce::String (juce::roundToInt (processorRef.getBpm())) + " BPM",
                          juce::dontSendNotification);
    };
    addAndMakeVisible (bpmLabel);

    measuresComboBox.addItemList ({ "1", "2", "4", "8", "16", "32" }, 1);
    measuresComboBox.setSelectedItemIndex (2, juce::dontSendNotification); // default "4"
    measuresComboBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgDark());
    measuresComboBox.setColour (juce::ComboBox::textColourId, ebs::text());
    measuresComboBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
    measuresComboBox.setColour (juce::ComboBox::arrowColourId, ebs::accent());
    measuresComboBox.setColour (juce::PopupMenu::backgroundColourId, ebs::bgDark());
    measuresComboBox.setColour (juce::PopupMenu::textColourId, ebs::text());
    measuresComboBox.onChange = [this] {
        if (curveEditor != nullptr)
            curveEditor->setMeasuresVisible (measuresComboBox.getText().getIntValue());
    };
    addAndMakeVisible (measuresComboBox);

    // === Global plugin Undo/Redo buttons (Option 1) ===
    // Icon-only glyphs from the shared library, operating on the
    // processor-wide UndoManager (every automatable parameter). Wired to
    // performGlobalUndo() / performGlobalRedo(); the keyboard shortcuts
    // Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z do the same (see keyPressed()).
    // They sit in the header between button B and the Options gear.
    setupEbsIcon (undoButton, ovt::tr(ovt::Keys::kTooltipUndo));
    undoButton.onClick = [this] { performGlobalUndo(); };

    setupEbsIcon (redoButton, ovt::tr(ovt::Keys::kTooltipRedo));
    redoButton.onClick = [this] { performGlobalRedo(); };

    // === Plugin Preset selector (separate from Curve Presets) ===
    // Centred in the top banner: [<] [Combo: name] [>] [save]. Mirrors the
    // standard plugin-preset bar. Applies the full parameter state (no
    // curve) via the processor's global-undo-aware apply.
    {
        setupEbsIcon (presetPrevButton, ovt::tr(ovt::Keys::kTooltipPluginPresetPrev));
        presetPrevButton.onClick = [this] {
            const int idx = pluginPresetNames.indexOf (currentPluginPresetName);
            const int n = pluginPresetNames.size();
            if (n == 0) return;
            const int prev = (idx <= 0) ? n - 1 : idx - 1;
            applyPluginPresetByName (pluginPresetNames[prev]);
        };

        setupEbsIcon (presetNextButton, ovt::tr(ovt::Keys::kTooltipPluginPresetNext));
        presetNextButton.onClick = [this] {
            const int idx = pluginPresetNames.indexOf (currentPluginPresetName);
            const int n = pluginPresetNames.size();
            if (n == 0) return;
            const int next = (idx < 0 || idx >= n - 1) ? 0 : idx + 1;
            applyPluginPresetByName (pluginPresetNames[next]);
        };

        setupEbsIcon (presetSaveButton, ovt::tr(ovt::Keys::kTooltipPluginPresetSave));
        presetSaveButton.onClick = [this] { promptSavePluginPreset(); };

        presetComboBox.setColour (juce::ComboBox::backgroundColourId, ebs::bgDark());
        presetComboBox.setColour (juce::ComboBox::textColourId, ebs::text());
        presetComboBox.setColour (juce::ComboBox::outlineColourId, ebs::accentSoft());
        presetComboBox.setColour (juce::ComboBox::arrowColourId, ebs::accent());
        presetComboBox.setColour (juce::PopupMenu::backgroundColourId, ebs::bgDark());
        presetComboBox.setColour (juce::PopupMenu::textColourId, juce::Colours::white);
        presetComboBox.onChange = [this] {
            const int idx = presetComboBox.getSelectedItemIndex();
            if (idx >= 0 && idx < pluginPresetNames.size())
                applyPluginPresetByName (pluginPresetNames[idx]);
        };
        addAndMakeVisible (presetComboBox);

        refreshPluginPresetList();
    }

    // Standalone transport: a single Play/Pause toggle plus a "Return to start"
    // (rewind) button. The toggle shows the Play glyph when stopped and the Stop
    // glyph when playing (see syncTransportButtons).
    {
        auto playNorm = createDrawableSVG (svgPlay, ebs::text().withAlpha (0.75f));
        auto playOver = createDrawableSVG (svgPlay, ebs::text());
        auto playDown = createDrawableSVG (svgPlay, ebs::accent());
        // Active state: solid accent pill -> bright white stop square,
        // readable over the themed toolstrip in both palettes.
        auto stopNorm = createDrawableSVG (svgStop,  juce::Colours::white);
        auto stopOver = createDrawableSVG (svgStop,  juce::Colours::white);
        auto stopDown = createDrawableSVG (svgStop,  juce::Colours::white.withAlpha (0.85f));
        // Normal images = Play, "on" images = Stop. Clicking does NOT auto-toggle
        // (we drive the displayed state from the processor in syncTransportButtons).
        playButton.setImages (playNorm.get(), playOver.get(), playDown.get(), nullptr,
                              stopNorm.get(), stopOver.get(), stopDown.get(), nullptr);
        iconSkins.push_back ({ &playButton, svgPlay, svgStop });
        // Active (playing): unmistakable solid accent pill with a bright
        // white stop square - blue-on-blue-tint was invisible.
        playButton.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        playButton.setColour (juce::DrawableButton::backgroundOnColourId, ebs::accent());
        playButton.setColour (juce::DrawableButton::textColourId, juce::Colours::white);
        playButton.setColour (juce::DrawableButton::textColourOnId, juce::Colours::white);
        playButton.onClick = [this] {
            processorRef.setTransportPlaying (! processorRef.isTransportPlaying());
            syncTransportButtons();
        };
        addAndMakeVisible (playButton);
    }

    setupEbsIcon (rewindButton, ovt::tr(ovt::Keys::kTooltipRewind));
    rewindButton.onClick = [this] {
        processorRef.resetTransportTime();
        if (curveEditor != nullptr)
        {
            curveEditor->clearInputTrace();
            curveEditor->returnToStart();
        }
    };

    setupIconButton(bypassButton, svgPower, true, "Bypass audio processing");
    bypassButton.setTooltip (ovt::tr(ovt::Keys::kTooltipBypassIcon));
    addAndMakeVisible (bypassButton);

    // MIDI Out icon (clicking toggles the attached toggle button)
    setupIconButton(midiOutButton, svgPower, true, "Enable MIDI Out");
    addAndMakeVisible (midiOutButton);

    // Menu button (gear icon for options)
    setupEbsIcon (menuButton, "OpenVoxTuner options");

    // Toggle buttons with text (attached to parameters)
    // Bypass lives only in the Advanced menu now (direct parameter write);
    // the old textual mirror button and its ghost attachment were removed.


    // Hide legacy icon buttons from top bar (kept for backward compatibility)
    bypassButton.setVisible (false);
    midiOutButton.setVisible (false);

    // Debug window button near bypass
   #if JUCE_DEBUG
    debugWindowButton.setButtonText (ovt::tr(ovt::Keys::kLabelDebug));
    debugWindowButton.setTooltip (ovt::tr(ovt::Keys::kTooltipDebugWindow));
    addAndMakeVisible (debugWindowButton);
    debugWindowButton.onClick = [this]() {
        // Create a simple modeless window if not already
        if (debugWindow != nullptr)
        {
            debugWindow->setVisible (true);
            debugWindow->toFront (true);
            return;
        }

        class ClosableDebugWindow : public juce::DocumentWindow
        {
        public:
            ClosableDebugWindow (const juce::String& name, juce::Colour backgroundColour, int requiredButtons)
                : juce::DocumentWindow (name, backgroundColour, requiredButtons) {}

            void closeButtonPressed() override
            {
                setVisible (false);
            }
        };

        auto* w = new ClosableDebugWindow ("Harmony Debug",
                                           juce::Colours::black,
                                           juce::DocumentWindow::closeButton);

        // Content component
        class DebugContent : public juce::Component, public juce::Timer
        {
        public:
            DebugContent (OpenVoxTunerAudioProcessor& p) : proc (p)
            {
                attackSlider.setRange (0.0, 200.0, 1.0);
                attackSlider.setValue (5.0);
                releaseSlider.setRange (0.0, 1000.0, 1.0);
                releaseSlider.setValue (proc.getHarmonyOutputLevel() > 0.0f ? 80.0 : 80.0);

                addAndMakeVisible(attackSliderLabel);
                addAndMakeVisible(releaseSliderLabel);
                addAndMakeVisible(attackSlider);
                addAndMakeVisible(releaseSlider);

                attackSliderLabel.setText ("Attack ms", juce::dontSendNotification);
                releaseSliderLabel.setText ("Release ms", juce::dontSendNotification);

                applyButton.setButtonText ("Apply");
                addAndMakeVisible (applyButton);
                applyButton.onClick = [this] {
                    proc.setHarmonyEnvelopeTimes ((float) attackSlider.getValue(), (float) releaseSlider.getValue());
                };

                clearButton.setButtonText ("Force Clear");
                addAndMakeVisible (clearButton);
                clearButton.onClick = [this] {
                    proc.clearHarmonyCache();
                };

                dumpButton.setButtonText ("Dump VST3 Info");
                addAndMakeVisible (dumpButton);
                dumpButton.onClick = [this] {
                    proc.dumpVST3BundleInfo();
                };

                testGrainButton.setButtonText ("Test Grain");
                addAndMakeVisible (testGrainButton);
                // Attach the button to the debug parameter so the host will propagate the change
                testGrainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.getParameters(), "dbg_test_grain", testGrainButton);

                // The shared read-only log view: CodeDocument-backed, keeps a text
                // selection (Ctrl+C), and knows where the reader is looking.
                addAndMakeVisible(midiLog);

                startTimerHz (10);
            }
            ~DebugContent() override
            {
                stopTimer();
            }
            void resized() override
            {
                auto r = getLocalBounds().reduced(8);
                attackSliderLabel.setBounds (r.removeFromTop(18));
                attackSlider.setBounds (r.removeFromTop(24));
                releaseSliderLabel.setBounds (r.removeFromTop(18));
                releaseSlider.setBounds (r.removeFromTop(24));
                auto bottom = r.removeFromTop(28);
                applyButton.setBounds (bottom.removeFromLeft(80));
                clearButton.setBounds (bottom.removeFromLeft(90));
                dumpButton.setBounds (bottom.removeFromLeft(100));
                testGrainButton.setBounds (bottom.removeFromLeft(120));
                midiLog.setBounds (r);
            }
            void timerCallback() override
            {
                // Rebuild the midi snapshot. The view is refreshed ONLY when the
                // text changed, so a reader scrolled up is never yanked back at 10 Hz.
                std::vector<juce::String> lines;
                for (int ch = 1; ch <= 9; ++ch)
                {
                    const int note = proc.getLastSentMidiNoteForChannel(ch);
                    if (note >= 0)
                        lines.push_back ("Ch" + juce::String(ch) + ":" + juce::String(note));
                }
                juce::String joined;
                for (const auto& l : lines)
                    joined << l << "\n";
                if (joined != lastMidiSnapshot)
                {
                    lastMidiSnapshot = joined;
                    midiLog.reset (lines);
                }
            }
        private:
            OpenVoxTunerAudioProcessor& proc;
            juce::Label attackSliderLabel, releaseSliderLabel;
            juce::Slider attackSlider, releaseSlider;
            juce::TextButton applyButton, clearButton;
            juce::TextButton dumpButton;
            juce::TextButton testGrainButton;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> testGrainAttachment;
            ebs::LogView midiLog;
            juce::String lastMidiSnapshot;   // so the view is rebuilt only on change
        };

        w->setContentOwned (new DebugContent (processorRef), true);
        w->setUsingNativeTitleBar (true);
        w->setResizable (true, false);
        w->centreWithSize (400, 320);
        w->setVisible (true);
        debugWindow.reset (w);
    };
   #else
    debugWindowButton.setVisible (false);
    debugWindowButton.setEnabled (false);
   #endif

    formantEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelFormantBtn));
    formantEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    formantEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    addAndMakeVisible (formantEnableButton);

    // Reverb bus controls: TWO SHARED ROOMS (ROOM + HALL), four sends, one sidechain
    // ducker. Each voice column is one power switch carrying the voice name, with the two
    // amounts that voice sends under it; the knob IS the send (no separate send control,
    // and the dry voice is never attenuated). A switch keeps its knobs' values, so
    // switching a voice off and back on needs no re-dial, and the knobs start at the
    // "ready to sing" amounts from ReverbTuning.h: the FIRST switch-on already applies a
    // balanced short-room + hall pair. That is the legacy one-knob experience back, with
    // the voice left intact.
    leadReverbEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelReverbBtn));
    leadReverbEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    leadReverbEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    leadReverbEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipLeadReverbEn));
    addAndMakeVisible (leadReverbEnableButton);

    setupKnob (leadRoomSlider, &leadRoomLabel, "Room");
    leadRoomSlider.setRange (0.0, 1.0, 0.01);
    leadRoomSlider.setEnabled (false); // greyed until the Lead switch is on, value kept
    leadRoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    leadRoomSlider.setPopupDisplayEnabled (true, false, this);
    leadRoomSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    leadRoomSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipLeadRoom));
    translatableLabels.push_back ({ &leadRoomLabel, ovt::Keys::kLabelRoom });

    setupKnob (leadHallSlider, &leadHallLabel, "Hall");
    leadHallSlider.setRange (0.0, 1.0, 0.01);
    leadHallSlider.setEnabled (false); // greyed until the Lead switch is on, value kept
    leadHallSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    leadHallSlider.setPopupDisplayEnabled (true, false, this);
    leadHallSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    leadHallSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipLeadHall));
    translatableLabels.push_back ({ &leadHallLabel, ovt::Keys::kLabelHall });

    harmonyReverbEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelPadReverbBtn));
    harmonyReverbEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    harmonyReverbEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    harmonyReverbEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyReverbEn));
    addAndMakeVisible (harmonyReverbEnableButton);

    setupKnob (harmonyRoomSlider, &harmonyRoomLabel, "Room");
    harmonyRoomSlider.setRange (0.0, 1.0, 0.01);
    harmonyRoomSlider.setEnabled (false); // greyed until the Harmony switch is on, value kept
    harmonyRoomSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyRoomSlider.setPopupDisplayEnabled (true, false, this);
    harmonyRoomSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    harmonyRoomSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyRoom));
    translatableLabels.push_back ({ &harmonyRoomLabel, ovt::Keys::kLabelRoom });

    setupKnob (harmonyHallSlider, &harmonyHallLabel, "Hall");
    harmonyHallSlider.setRange (0.0, 1.0, 0.01);
    harmonyHallSlider.setEnabled (false); // greyed until the Harmony switch is on, value kept
    harmonyHallSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    harmonyHallSlider.setPopupDisplayEnabled (true, false, this);
    harmonyHallSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    harmonyHallSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipHarmonyHall));
    translatableLabels.push_back ({ &harmonyHallLabel, ovt::Keys::kLabelHall });

    setupKnob (duckDepthSlider, &duckDepthLabel, "Ducking");
    duckDepthSlider.setRange (0.0, 1.0, 0.01);
    duckDepthSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    duckDepthSlider.setPopupDisplayEnabled (true, false, this);
    duckDepthSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    duckDepthSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipReverbDucking));
    translatableLabels.push_back ({ &duckDepthLabel, ovt::Keys::kLabelDuckBtn });

    // Output stage (master bus): the trim is a raw dB gain applied before the limiter,
    // and the limiter is a true-peak guard with a declared 8-sample lookahead. Trim stays
    // live with the limiter
    // off - it is the level match into the mix, not the safety net.
    limiterEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelLimiterBtn));
    limiterEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    limiterEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    limiterEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipLimiterEn));
    addAndMakeVisible (limiterEnableButton);

    setupKnob (outputTrimSlider, nullptr, "");
    outputTrimSlider.setRange (-12.0, 12.0, 0.1);
    // No value textbox; the live value is shown in JUCE's popup display while dragging.
    outputTrimSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    outputTrimSlider.setPopupDisplayEnabled (true, false, this);
    outputTrimSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    outputTrimSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipOutputTrim));
    outputTrimLabel.setVisible (false); // same reason as the reverb captions: the switch carries the name

    // The meter is display only: it takes no mouse, and its numbers arrive from the
    // processor's atomic snapshot in timerCallback, never from the audio thread.
    outputMeter.setBarHeight (5);
    outputMeter.setCaptions (ovt::tr(ovt::Keys::kLabelMeterGr), ovt::tr(ovt::Keys::kLabelMeterTrim));
    outputMeter.setTooltip (ovt::tr(ovt::Keys::kTooltipOutputMeter));

    // Noise Gate
    noiseGateEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelNoiseGate));
    noiseGateEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipNoiseGate));
    noiseGateEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    noiseGateEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    addAndMakeVisible (noiseGateEnableButton);

    noiseGateThresholdSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    noiseGateThresholdSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    noiseGateThresholdSlider.setPopupDisplayEnabled (true, false, this);
    noiseGateThresholdSlider.textFromValueFunction = [] (double v) { return juce::String (v, 0) + " dB"; };
    noiseGateThresholdSlider.setRange (-80.0, 0.0, 1.0);
    noiseGateThresholdSlider.setColour (juce::Slider::rotarySliderFillColourId, ebs::accent());
    noiseGateThresholdSlider.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    noiseGateThresholdSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    noiseGateThresholdSlider.setColour (juce::Slider::textBoxTextColourId, ebs::text());
    noiseGateThresholdSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    noiseGateThresholdSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    noiseGateThresholdSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipThreshold));
    addAndMakeVisible (noiseGateThresholdSlider);

    noiseGateThresholdLabel.setText (ovt::tr(ovt::Keys::kLabelThreshold), juce::dontSendNotification);
    noiseGateThresholdLabel.setColour (juce::Label::textColourId, ebs::text());
    noiseGateThresholdLabel.setJustificationType (juce::Justification::centred);
    noiseGateThresholdLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    noiseGateThresholdLabel.setVisible (false); // Hidden - same style as Formant/Reverb (no label)

    // Upward Compressor controls (input effect, before tuning).
    // Same visual style as the other effect knobs: power toggle on top, a single
    // "amount" knob with no visible value (only JUCE's drag popup shows it).
    upwardCompEnableButton.setButtonText (ovt::tr(ovt::Keys::kLabelUpwardComp));
    upwardCompEnableButton.setColour (juce::ToggleButton::textColourId, ebs::text());
    upwardCompEnableButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    upwardCompEnableButton.setTooltip (ovt::tr(ovt::Keys::kTooltipUpwardCompEn));
    addAndMakeVisible (upwardCompEnableButton);

    upwardCompAmountSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    upwardCompAmountSlider.setRange (0.0, 1.0, 0.01);
    upwardCompAmountSlider.setEnabled (false); // disabled until upward comp is toggled on
    // No value textbox; the live value is shown in JUCE's popup display while dragging.
    upwardCompAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    upwardCompAmountSlider.setPopupDisplayEnabled (true, false, this);
    upwardCompAmountSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
    upwardCompAmountSlider.setColour (juce::Slider::rotarySliderFillColourId, ebs::accent());
    upwardCompAmountSlider.setColour (juce::Slider::rotarySliderOutlineColourId, ebs::accentSoft());
    upwardCompAmountSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    upwardCompAmountSlider.setColour (juce::Slider::textBoxTextColourId, ebs::text());
    upwardCompAmountSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    upwardCompAmountSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    upwardCompAmountSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipUpwardCompAmount));
    addAndMakeVisible (upwardCompAmountSlider);

    // Effects frame (ebs::ExpansibleFrame): the effect cells live inside the frame body, so
    // the chevron on its right edge folds everything past the two always-visible columns away
    // - Formant/De-Esser, Saturation and Output Limit + meter. The frame owns the open flag;
    // the editor re-lays out when it flips, and the frame widens to take the extra columns,
    // pushing the Reverb block (registered right of Effects) and then Harmony.
    // The band registered this frame in the constructor, so it is already a band child. Its
    // fold is persisted like the other three, because the frame owns a control the user wants on
    // screen: the tempo-synced Delay lives in the open zone, and a fold that forgot itself would
    // hide it on every launch. The flag must be written BEFORE the width model re-runs, so the
    // saved width is the one the row actually shows.
    effectsFrame.onExpandedChanged = [this]
    {
        processorRef.setEffectsExpanded (effectsFrame.expanded());
        updateBottomBandWidths();   // the fold changes what this block asks for
        resized();
    };
    effectsFrame.disclosure().setTooltip (ovt::tr (ovt::Keys::kTooltipEffectsMore));

    // The cells move into the frame: as children of the body they paint above it and
    // are clipped by it, so a folded column disappears with the frame's own fold.
    // The list runs in the order the columns read on screen: Gate, Compressor, Doubler, Formant (the two folded
    // columns), then Output Limit + meter, De-Esser and Saturation in the open zone. A screen reader follows this
    // insertion order, so a limit control placed after Saturation here would be announced out of position.
    for (juce::Component* c : { static_cast<juce::Component*> (&noiseGateEnableButton),
                                static_cast<juce::Component*> (&noiseGateThresholdSlider),
                                static_cast<juce::Component*> (&upwardCompEnableButton),
                                static_cast<juce::Component*> (&upwardCompAmountSlider),
                                static_cast<juce::Component*> (&doubleEnableButton),
                                static_cast<juce::Component*> (&doubleAmountSlider),
                                static_cast<juce::Component*> (&formantEnableButton),
                                static_cast<juce::Component*> (&formantSlider),
                                static_cast<juce::Component*> (&limiterEnableButton),
                                static_cast<juce::Component*> (&outputTrimSlider),
                                static_cast<juce::Component*> (&outputMeter),
                                static_cast<juce::Component*> (&deessEnableButton),
                                static_cast<juce::Component*> (&deessAmountSlider),
                                static_cast<juce::Component*> (&saturationEnableButton),
                                static_cast<juce::Component*> (&saturationDriveSlider),
                                static_cast<juce::Component*> (&saturationDriveLabel),
                                static_cast<juce::Component*> (&saturationToneSlider),
                                static_cast<juce::Component*> (&saturationToneLabel),
                                static_cast<juce::Component*> (&delayEnableButton),
                                static_cast<juce::Component*> (&delayDivisionLabel),
                                static_cast<juce::Component*> (&delayDivisionKnob),
                                static_cast<juce::Component*> (&delayMixSlider),
                                static_cast<juce::Component*> (&delayMixLabel),
                                static_cast<juce::Component*> (&delayFeedbackSlider),
                                static_cast<juce::Component*> (&delayFeedbackLabel) })
        effectsFrame.addAndMakeVisible (c);

    // The Reverb frame owns the two voice columns. The Lead column's switch, both of its
    // amounts and the ducker are children here; the Harmony column's four controls join
    // them and the layout code hides exactly those when the frame is folded. Nothing
    // reverb-related stays a child of effectsFrame: a widget parented to Effects would be
    // clipped by the wrong fold.
    for (juce::Component* c : { static_cast<juce::Component*> (&leadReverbEnableButton),
                                static_cast<juce::Component*> (&leadRoomSlider),
                                static_cast<juce::Component*> (&leadRoomLabel),
                                static_cast<juce::Component*> (&leadHallSlider),
                                static_cast<juce::Component*> (&leadHallLabel),
                                static_cast<juce::Component*> (&harmonyReverbEnableButton),
                                static_cast<juce::Component*> (&harmonyRoomSlider),
                                static_cast<juce::Component*> (&harmonyRoomLabel),
                                static_cast<juce::Component*> (&harmonyHallSlider),
                                static_cast<juce::Component*> (&harmonyHallLabel),
                                static_cast<juce::Component*> (&duckDepthSlider),
                                static_cast<juce::Component*> (&duckDepthLabel) })
        reverbFrame.addAndMakeVisible (c);

    // 2026-07-24 (Deprecation): FlexTune and Attack-Aware UI setup
    // is disabled. The slider/label members still exist (in
    // PluginEditor.h) for future re-implementation, but they are
    // not added to the visible UI and not attached to parameters.
    // The setupKnob calls are commented out to keep the code
    // available as reference.
    // ----------------------------------------------------------------
    // FlexTune / Humanize knobs
    // setupKnob (flexTuneSlider, &flexTuneLabel, "FlexTune");
    // translatableLabels.push_back ({ &flexTuneLabel, ovt::Keys::kLabelFlex    });
    // flexTuneSlider.setRange (0.0, 100.0, 1.0);
    // flexTuneSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipFlexTune));

    setupKnob (humanizeSlider, &humanizeLabel, "Humanize");
    translatableLabels.push_back ({ &humanizeLabel, ovt::Keys::kLabelHumanize    });
    humanizeSlider.setRange (0.0, 50.0, 1.0);
    humanizeSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipHumanize));
    // The live value is shown in JUCE's popup display while dragging (see below).

    // FlexTune and Humanize: no textbox, smaller inline labels
    // flexTuneLabel.setText ("Flex", juce::dontSendNotification);
    // flexTuneSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    // flexTuneSlider.setPopupDisplayEnabled (true, false, this);
    // flexTuneSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " cents"; };
    humanizeLabel.setText ("Humanize", juce::dontSendNotification);
    humanizeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    humanizeSlider.setPopupDisplayEnabled (true, false, this);
    humanizeSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " cents"; };

    // Vibrato preservation knob (0..1 -> displayed as 0..100 %).
    setupKnob (vibratoPreserveSlider, &vibratoPreserveLabel, "Vibrato");
    translatableLabels.push_back ({ &vibratoPreserveLabel, ovt::Keys::kLabelVibrato    });
    vibratoPreserveSlider.setRange (0.0, 1.0, 0.01);
    vibratoPreserveSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipVibrato));
    vibratoPreserveLabel.setText ("Vibrato", juce::dontSendNotification);
    vibratoPreserveSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    vibratoPreserveSlider.setPopupDisplayEnabled (true, false, this);
    vibratoPreserveSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };

    // 2026-07-24 (Deprecation): Attack release knob setup is disabled.
    // setupKnob (attackReleaseSlider, &attackReleaseLabel, "Attack Rel");
    // translatableLabels.push_back ({ &attackReleaseLabel, ovt::Keys::kLabelAttackRelease    });
    // attackReleaseSlider.setRange (10.0, 300.0, 1.0);
    // attackReleaseSlider.setTooltip (ovt::tr(ovt::Keys::kTooltipAttackRelease));
    // attackReleaseLabel.setText ("Rel", juce::dontSendNotification);
    // attackReleaseSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    // attackReleaseSlider.setPopupDisplayEnabled (true, false, this);
    // attackReleaseSlider.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v)) + " ms"; };

    // Correction Mode toggle button
    correctionModeButton.setButtonText (ovt::tr(ovt::Keys::kLabelModernBtn));
    correctionModeButton.setClickingTogglesState (true);
    correctionModeButton.setColour (juce::TextButton::buttonColourId, ebs::accentSoft());
    correctionModeButton.setColour (juce::TextButton::buttonOnColourId, ebs::accent());
    correctionModeButton.setColour (juce::TextButton::textColourOffId, ebs::text());
    correctionModeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    correctionModeButton.setTooltip (ovt::tr(ovt::Keys::kTooltipCorrection));
    correctionModeButton.onClick = [this] {
        bool isTransparent = correctionModeButton.getToggleState();
        correctionModeButton.setButtonText (isTransparent ? ovt::tr(ovt::Keys::kLabelTransparentBtn) : ovt::tr(ovt::Keys::kLabelModernBtn));
    };
    addAndMakeVisible (correctionModeButton);

    // Hard Tune backing button: the third position of the mode switch. Invisible
    // like correctionModeButton, attached to `hard_tune`. The switch writes both
    // parameters through the editor callback, in ONE undo transaction.
    hardTuneButton.setButtonText (ovt::tr (ovt::Keys::kLabelHardTune));
    hardTuneButton.setClickingTogglesState (true);
    hardTuneButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    addAndMakeVisible (hardTuneButton);

    // Graphic Modern / Transparent / Hard switch below the Speed/Amount knobs
    // (console style). Backed by correctionModeButton + hardTuneButton so the
    // parameter attachments and external sync are reused unchanged.
    modeSwitch = std::make_unique<CorrectionModeSwitch> (correctionModeButton, hardTuneButton);
    modeSwitch->onSelect = [this] (bool transparent, bool hard)
    {
        // One user gesture -> one undo entry. The two buttons fire their
        // ButtonAttachments (which write the parameters); the editor is not
        // registered as a listener on them, so this snapshot is the only commit.
        pushUndoAroundCall ([this, transparent, hard]
        {
            correctionModeButton.setToggleState (transparent, juce::sendNotification);
            hardTuneButton.setToggleState (hard, juce::sendNotification);
        });
    };
    // The studio-console switch lives inside the Correction frame, like every widget of
    // that block: as a child of the body it paints above the frame and is clipped by it.
    correctionFrame.addAndMakeVisible (*modeSwitch);

    // Every widget of this block moves into the frame body: as children they paint above it
    // and are clipped by it, so a folded zone is hidden with setVisible(false), never painted
    // over. The Speed / Amount knobs and the studio-console switch stay visible in both states;
    // the advanced zone (Vibrato, Humanize, Voice Type, Formant Strategy) is the one that folds.
    for (juce::Component* c : { static_cast<juce::Component*> (&speedSlider),
                                static_cast<juce::Component*> (&speedLabel),
                                static_cast<juce::Component*> (&amountSlider),
                                static_cast<juce::Component*> (&amountLabel),
                                static_cast<juce::Component*> (&vibratoPreserveSlider),
                                static_cast<juce::Component*> (&vibratoPreserveLabel),
                                static_cast<juce::Component*> (&humanizeSlider),
                                static_cast<juce::Component*> (&humanizeLabel),
                                static_cast<juce::Component*> (&voiceTypeBox),
                                static_cast<juce::Component*> (&voiceTypeLabel),
                                static_cast<juce::Component*> (&formantStrategyBox),
                                static_cast<juce::Component*> (&formantStrategyLabel) })
        correctionFrame.addAndMakeVisible (c);
    // Root/Scale and Harmony widgets move into their own frame body, the same contract as
    // the Correction frame: as children they paint above the frame and are clipped by it,
    // so each block repaints itself when the bottom row re-lays out.
    for (juce::Component* c : { static_cast<juce::Component*> (&keyBox),
                                static_cast<juce::Component*> (&keyLabel),
                                static_cast<juce::Component*> (&scaleBox),
                                static_cast<juce::Component*> (&scaleLabel),
                                static_cast<juce::Component*> (&scaleKeyboard),
                                static_cast<juce::Component*> (&keyDetectPowerButton),
                                static_cast<juce::Component*> (&keySourceLabel),
                                static_cast<juce::Component*> (&keySourceBox),
                                static_cast<juce::Component*> (&companionGroupLabel),
                                static_cast<juce::Component*> (&companionGroupBox) })
        scaleFrame.addAndMakeVisible (c);

    for (juce::Component* c : { static_cast<juce::Component*> (&harmonyEnableButton),
                                static_cast<juce::Component*> (&harmonyTypeBox),
                                static_cast<juce::Component*> (&useVoiceLabel),
                                static_cast<juce::Component*> (&useVoiceButton),
                                static_cast<juce::Component*> (&harmonyToneColorLabel),
                                static_cast<juce::Component*> (&shiftedVoicesBox),
                                static_cast<juce::Component*> (&harmonyToneBox),
                                static_cast<juce::Component*> (&harmonyToneColorSlider),
                                static_cast<juce::Component*> (&harmonyFollowLeadButton),
                                static_cast<juce::Component*> (&harmonyGainMatchButton),
                                static_cast<juce::Component*> (&harmonyGainLabel),
                                static_cast<juce::Component*> (&harmonyGainSlider),
                                static_cast<juce::Component*> (&harmonyBlendLabel),
                                static_cast<juce::Component*> (&harmonyBlendSlider),
                                static_cast<juce::Component*> (&harmonyFormantLabel),
                                static_cast<juce::Component*> (&harmonyFormantSlider),
                                static_cast<juce::Component*> (&harmonyAttackLabel),
                                static_cast<juce::Component*> (&harmonyAttackSlider) })
        harmonyFrame.addAndMakeVisible (c);

    // Attack-Aware correction toggle button - power-icon style like Gate / Reverb / Formant.
    // 2026-07-24 (Deprecation): Attack-Aware button setup is disabled.
    // attackAwareButton.setButtonText (ovt::tr(ovt::Keys::kLabelAttackBtn));
    // attackAwareButton.setColour (juce::ToggleButton::tickColourId, ebs::accent());
    // attackAwareButton.setTooltip (ovt::tr(ovt::Keys::kTooltipAttack));
    // addAndMakeVisible (attackAwareButton);

    // scaleKeyboard is made visible inside scaleFrame by the reparent loop above; a
    // second addAndMakeVisible here would move it back to the editor and give its
    // frame-local bounds editor-local meaning.

    // === Bidirectional attachments to AudioParameters ===
    auto& tree = processorRef.getParameters();
    speedAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "speed",  speedSlider);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "amount", amountSlider);
    formantAttachment= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "formant", formantSlider);
    formantEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "formant_enable", formantEnableButton);
    // also keep legacy names for backward compatibility
    bypassAttachment = nullptr;
    midiOutAttachment = nullptr;

    // Harmony attachments
    harmonyEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_enable", harmonyEnableButton);
    harmonyTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, "harmony_type", harmonyTypeBox);
    harmonyGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_gain", harmonyGainSlider);
    harmonyBlendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_blend", harmonyBlendSlider);
    harmonyAttackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_attack", harmonyAttackSlider);
    harmonyFollowLeadAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_follow_lead", harmonyFollowLeadButton);
    harmonyGainMatchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_gain_match", harmonyGainMatchButton);
    harmonyFormantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_formant", harmonyFormantSlider);
    doubleAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "double_amount", doubleAmountSlider);
    // Doubler power: a real parameter attached to the button, exactly like
    // lead_reverb_enable. The knob keeps its value when switched off; refreshLabels()
    // greys it and the processor stops applying the copy.
    doubleEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "double_enable", doubleEnableButton);
    // De-Esser attachments: one power switch covering the single intensity knob.
    // The knob keeps its value when the effect is switched off; refreshLabels()
    // greys it, exactly as it does for a voice's reverb amounts behind its switch.
    deessEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "deess_enable", deessEnableButton);
    deessAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "deess_amount", deessAmountSlider);
    saturationEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "saturation_enable", saturationEnableButton);
    saturationDriveAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "saturation_drive", saturationDriveSlider);
    saturationToneAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "saturation_tone", saturationToneSlider);
    // Delay attachments: one power switch, one note-value combo and two knobs. The combo
    // rides the ComboBoxAttachment pattern of shiftedVoicesBox above - items are populated
    // with addItemList ({...}, 1), so item ids start at 1 and the attachment maps them to
    // the AudioParameterChoice index verbatim.
    delayEnableAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "delay_enable", delayEnableButton);
    delayDivisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "delay_division", delayDivisionKnob);
    delayMixAttachment      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "delay_mix", delayMixSlider);
    delayFeedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "delay_feedback", delayFeedbackSlider);
    // Keep the Follow Lead and Gain Match sub-toggles in sync with the parent
    // Harmony switch. onStateChange fires on a direct click, so we re-run
    // refreshLabels() (which now also disables those two sub-toggles based on
    // the Harmony state). This guarantees the sub-toggles are greyed out the
    // instant Harmony is turned off, and stays consistent with preset load /
    // automation / language switch - all paths already call refreshLabels().
    harmonyEnableButton.onStateChange = [this] {
        refreshLabels();
    };
    useVoiceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_use_voice", useVoiceButton);
    shiftedVoicesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, "harmony_shifted_voices", shiftedVoicesBox);
    harmonyToneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, "harmony_tone", harmonyToneBox);
    harmonyToneColorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_tone_color", harmonyToneColorSlider);
    for (int i = 0; i < 12; ++i)
    {
        const juce::String id = "custom" + juce::String (i);
        customAttachments[i] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, id, scaleKeyboard.getButton(i));

        scaleKeyboard.getButton(i).onUserInteraction = [this] {
            // Switch to Custom mode silently: push the new scale value
            // through the AudioParameterChoice (NOT just the raw atomic
            // pointer), because the audio callback's syncParameters()
            // reads the AudioParameterChoice (line 2261 - `scaleChoiceParam
            // ->getIndex()`) when rebuilding the scale intervals. Storing
            // into the atomic alone leaves scaleChoiceParam pointing at the
            // previous preset, so on the next block syncParameters() pushes
            // the previous preset's intervals back into the quantizer and
            // the piano keyboard - and the toggle we just applied is
            // visually invisible (the user-reported bug on 2026-07-17:
            // "je clique sur la touche D => rien ne se passe (touche dans
            // la gamme précédente C Natural Minor)").
            //
            // setValueNotifyingHost fires valueChanged, which the
            // ComboBoxAttachment listens to in order to update the combo
            // display (no need for a separate setSelectedItemIndex call).
            // The scaleBox.onChange callback also fires; in Custom mode
            // (the target of this switch) it short-circuits immediately
            // ("Custom keeps the user's custom note flags" comment in
            // onChange), so the toggles we just set are NOT clobbered.
            auto* scaleParam = dynamic_cast<juce::AudioParameterChoice*>(
                processorRef.getParameters().getParameter ("scale"));
            if (scaleParam != nullptr)
            {
                const int numChoices = scaleParam->choices.size();
                const int customIdx  = numChoices - 1; // Custom is the last entry
                if (scaleParam->getIndex() != customIdx && numChoices > 1)
                {
                    const float normalized = static_cast<float> (customIdx)
                                           / static_cast<float> (numChoices - 1);
                    scaleParam->setValueNotifyingHost (normalized);
                }
            }
        };
    }

    // Key/scale ComboBox binding was already done above via ComboBoxAttachment.
    // === Pitch visualizer ===
    pitchVisualizer = std::make_unique<ui::PitchVisualizer>();
    pitchVisualizer->onRightClick = [this] {
        // Store the current mouse position so the wrench menu opens at the cursor.
        pendingMenuScreenPos = juce::Desktop::getInstance().getMousePosition();
        menuButton.triggerClick();
    };

    // Restore auto-center pitch preference from APVTS.
    if (auto* acParam = processorRef.getParameters().getParameter ("ui_auto_center"))
        pitchVisualizer->setAutoCenter (acParam->getValue() > 0.5f);

    // Restore visualizer zoom range from APVTS.
    {
        float fMinHz = 50.0f, fMaxHz = 1500.0f;
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmin"))
            fMinHz = p->convertFrom0to1 (p->getValue());
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmax"))
            fMaxHz = p->convertFrom0to1 (p->getValue());
        pitchVisualizer->setZoomRange (fMinHz, fMaxHz);
    }

    // Save visualizer zoom to APVTS when it changes.
    pitchVisualizer->onZoomChanged = [this] (float fMinHz, float fMaxHz) {
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmin"))
            p->setValueNotifyingHost (p->convertTo0to1 (fMinHz));
        if (auto* p = processorRef.getParameters().getParameter ("viz_fmax"))
            p->setValueNotifyingHost (p->convertTo0to1 (fMaxHz));
    };

    curveEditor = std::make_unique<ui::PitchCurveEditor>();
    curveEditor->addListener (this);
    curveEditor->setViewRange (16.0, 50.0f, 1000.0f);
    curveEditor->onRightClick = [this] (const juce::MouseEvent& e) {
        showPresetsMenu (&e);
    };
    // Ruler-click seek: forward the requested playhead time to the transport.
    // In "Follow host" mode (VST3/AU, not ARA, not looping) the DAW owns the
    // timeline, so a ruler click cannot move the DAW playhead - seeking there
    // would only create a permanent offset that desyncs from the DAW loop.
    // We ignore the seek in that mode (the playhead snaps back to the DAW on
    // the next block). In Loop Playhead (Measures) / Standalone / ARA the seek
    // is honoured.
    curveEditor->onSeek = [this] (double t) {
#if OVT_ARA_ENABLED
        if (processorRef.isBoundToARA())
            processorRef.seekToTime (t);          // ARA: host follows the plug-in
        else
#endif
        if (processorRef.isPlayheadLooping())
            processorRef.seekToTime (t);          // Loop / Standalone: local timeline
        // else: Follow host -> DAW is master, ignore (no desync)
    };

    // Initialize tabs
      tabbedComponent.setOutline(0);
      // Tab content areas always use dark background regardless of theme
      const juce::Colour tabContentColour = juce::Colour (0xff14151c);
      tabbedComponent.addTab(ovt::tr(ovt::Keys::kTabLive), tabContentColour, pitchVisualizer.get(), false);
      tabbedComponent.addTab(ovt::tr(ovt::Keys::kTabCurveEditor), tabContentColour, curveEditor.get(), false);
    addAndMakeVisible(tabbedComponent);
    tabbedComponent.setColour (juce::TabbedComponent::backgroundColourId, ebs::bgDark());
    tabbedComponent.setColour (juce::TabbedComponent::outlineColourId, ebs::accentSoft());

    // Replace the standard tab bar with the shared animated pill switch.
    tabbedComponent.getTabbedButtonBar().setVisible(false);

    // The pill switch replaces the visible tab bar, and hiding the bar does NOT shrink the
    // strip it reserves: with the default depth of 30 the canvas starts 30 px below the top of
    // the component, so a taller toolbar band overlapped it. Zero depth makes the content fill
    // the whole bounds, which is what the band-above-viewport layout assumes.
    tabbedComponent.setTabBarDepth (0);
    tabSwitch = std::make_unique<ebs::PillTabSwitch> (tabbedComponent);
    addAndMakeVisible (*tabSwitch);

    // Glyphs in the pill slots. The shared switch measures icon + gap + label
    // itself, so the strip asks for the width the PAIR needs; the glyph colour
    // follows the label (white on the active slot, dim elsewhere).
    tabSwitch->onTabIcon = [] (int i) -> std::optional<ebs::IconButton::Shape>
    {
        if (i == 0) return ebs::IconButton::Shape::mic;      // Live: the live voice feed
        if (i == 1) return ebs::IconButton::Shape::activity; // Curve Editor: the pitch curve
        return std::nullopt;
    };

    // Initialize piano keyboards with chromatic intervals so they display
    // all keys on first paint, before the first timerCallback fires.
    {
        juce::Array<int> chromatic;
        for (int i = 0; i < 12; ++i) chromatic.add (i);
        pitchVisualizer->setScaleIntervals (chromatic);
        curveEditor->setScaleIntervals (chromatic);
        scaleKeyboard.setActiveScaleIntervals (chromatic);
    }

    // Make sure tools are drawn over the tabbed component
    optionsButton.toFront(false);
    snapButton.toFront(false);
    snapGridButton.toFront(false);
    stepModeButton.toFront(false);
    zoomInButton.toFront(false);
    zoomOutButton.toFront(false);
    scrollUpButton.toFront(false);
    scrollDownButton.toFront(false);
    resetViewButton.toFront(false);
    playButton.toFront(false);
    rewindButton.toFront(false);
    measuresLabel.toFront(false);
    measuresComboBox.toFront(false);

    // Add overlay LAST so it renders on top of all other components
    addAndMakeVisible (helpOverlay);

    // Force curve sync on first timer tick (covers both setStateInformation
    // before/after createEditor, and editor recreation on UI show/hide).
    processorRef.getPendingCurveRestore().store (true);
    
    // Sync immediately so the curve is correct on the very first paint
    // (avoids a 33ms flash of the "default" preset).
    if (curveEditor != nullptr)
    {
        curveEditor->setCurve (processorRef.getPitchCurve());
        processorRef.getPendingCurveRestore().store (false);
    }
    // Restore A/B slots from the processor (persisted across project reload).
    {
        auto restoreSlot = [this] (ABState& slot, int slotIdx)
        {
            if (! processorRef.hasAbSlotData (slotIdx)) return;
            const auto* ms = processorRef.getAbSlotMorphState (slotIdx);
            if (ms == nullptr) return;

            // Backward-compatibility: remap old Vocoder-like index (5) to Organ (1)
            auto remapTone = *ms;
            if (remapTone.harmonyTone == 5) remapTone.harmonyTone = 1;
            // Old indices 1..4 (Bright/SynthLead/Strings/Guitar) were removed -
            // silently keep them as-is (will map to wrong preset, but user will notice).

            slot.morphState = std::make_unique<ovtdsp::MorphState> (remapTone);
            slot.hasData = true;
            slot.name = "filled";
        };

        restoreSlot (slotA, 0);
        restoreSlot (slotB, 1);
        // Restore the active slot so the editor reopens on the same slot the
        // user left. The main APVTS holds that slot's values, so keeping the
        // active-slot flag in sync prevents slot A from appearing to contain
        // slot B's content after a restart.
        isSlotAActive = (processorRef.getAbActiveSlot() == 0);
        // Snap the live parameters to the ACTIVE slot's saved MorphState ONLY
        // when the restored state is a "frozen morph" (morph_amount mid-range).
        // Otherwise the global parameters restored from `.settings` are
        // authoritative and MUST NOT be overwritten by the active slot's values
        // — otherwise every global change (Vibrato, Humanize, harmony type /
        // formant) made before quitting is silently lost on restart.
        const float restoredMorph = processorRef.getMorphAmount();
        const bool frozenMorph = (restoredMorph > 0.02f && restoredMorph < 0.98f);
        {
            auto& activeSlot = isSlotAActive ? slotA : slotB;
            if (frozenMorph && activeSlot.hasData && activeSlot.morphState != nullptr)
                loadSlot (activeSlot);
        }
        processorRef.setMorphAmount (isSlotAActive ? 0.0f : 1.0f);
        lastMorphValue = isSlotAActive ? 0.0f : 1.0f;
        updateABButtonStates();
    }
    
    // Initialize tab from processor parameter
    float initialMode = processorRef.getParameters().getParameter("mode")->getValue();
    tabbedComponent.setCurrentTabIndex(initialMode > 0.5f ? 1 : 0);

    // Tab change callback
    // Parameter update is done in timerCallback() to avoid complex loops.

    // Default size: AFTER creating children, otherwise setSize triggers
    // resized() which accesses pitchVisualizer/curveEditor still nullptr.
    // Min size only: no max size constraint so the plugin can use all
    // available space in ARA mode (DAW may allocate arbitrarily large views).
    setSize (900, 650);
    setResizable (true, true);
    setResizeLimits (1100, 600, 99999, 99999);

    // Refresh labels with current language translations
    refreshLabels();

    // === Global plugin Undo/Redo (Option 1) ===
    // Seed the live snapshot with the current state and wire the gesture
    // listeners. The UndoManager itself lives in the processor.
    undoLiveSnapshot_ = processorRef.getParameters().copyState();
    registerUndoGestureListeners();

    // Timer to update the visualizer (~30 fps).
    startTimerHz (30);
}

OpenVoxTunerAudioProcessorEditor::~OpenVoxTunerAudioProcessorEditor()
{
    // Theme broadcast teardown must precede member destruction.
    ebs::unsubscribeTheme (this);

    if (updateCheckState != nullptr)
        updateCheckState->cancelled.store (true);

    // Signal the processor to stop touching the AU playhead BEFORE we
    // stop our own timer.  A MessageQueue timer callback may already be
    // queued (it is dispatched asynchronously), so without this flag the
    // callback could reach updateHostTransport() -> getPlayHead()->
    // getPosition() after the AU's internal state has been torn down,
    // crashing the host (Live 12 AU, macOS 26).
    processorRef.notifyEditorShuttingDown();

    stopTimer();

    // These are independent top-level Desktop components. Hide and destroy
    // them before the editor's child components and LookAndFeel are torn down.
    midiImportFileChooser.reset();
    if (debugWindow != nullptr)
    {
        debugWindow->setVisible (false);
        debugWindow->setContentOwned (nullptr, true);
        debugWindow.reset();
    }
    if (galleryWindow != nullptr)
    {
        galleryWindow->setVisible (false);
        galleryWindow->setContentOwned (nullptr, true);
        galleryWindow.reset();
    }

    // -------------------------------------------------------------------
    // Clear LookAndFeel references BEFORE the customLookAndFeel member is
    // destroyed (declaration order in the header puts it first in the
    // private section, so C++ destroys it LAST after this destructor
    // returns). Every child Component that was added to the editor or
    // to any of its sub-components received a LookAndFeel propagation
    // when addAndMakeVisible() was called: JUCE walks the parent's
    // lookAndFeel weak-ref down to each new child. Those weak-refs
    // remain in the child even if we setLookAndFeel(nullptr) on the
    // parent alone - each Component owns its own LookAndFeel pointer.
    // We therefore must clear them recursively, in the REVERSE order
    // of the visual hierarchy (most-deeply-nested children first), so
    // that by the time customLookAndFeel's destructor runs (after the
    // implicit member-destruction phase that follows this body), no
    // surviving weak-ref still points at the (about-to-die) object.
    // Otherwise the JUCE leak/assertion detector in Debug mode reports
    // `refCount.value = 2` and aborts.
    // -------------------------------------------------------------------

    // Drop the LookAndFeel on the deepest children first (Slider, Label,
    // Button, ComboBox, custom widgets...). This is done via a recursive
    // lambda so any future component added to the editor is covered
    // without needing to update this list.
    // juce::Component exposes its children via the `getChildren()` range
    // (Component::Children), which is a safe iterable even during
    // teardown (it skips children being deleted).
    std::function<void(juce::Component&)> clearLookAndFeelRecursive =
        [&clearLookAndFeelRecursive](juce::Component& c)
    {
        // Recurse first (post-order: deepest first), then null out the
        // LookAndFeel on the current component. This ensures children
        // are fully released before their parent.
        for (auto* child : c.getChildren())
            clearLookAndFeelRecursive (*child);
        c.setLookAndFeel (nullptr);
    };
    clearLookAndFeelRecursive (*this);

    // Now safe to release the explicit teardown of components that own
    // resources beyond the LookAndFeel pointer.
    if (tooltipWindow != nullptr)
    {
        tooltipWindow->setLookAndFeel (nullptr);
        tooltipWindow.reset();
    }
    if (curveEditor != nullptr)
    {
        curveEditor->removeListener();
        curveEditor->setLookAndFeel (nullptr);
    }
    setLookAndFeel (nullptr);
}

void OpenVoxTunerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Main background (flat, no gradient)
    g.setColour (ebs::bgDark());
    g.fillAll();

    // Every bottom-row block paints its own body: the two ExpansibleFrames (Correction,
    // Effects) and the two FramedBodies (Root/Scale, Harmony). paint() draws no block
    // fill or outline here any more.

    // Top banner with title (opaque, matches tab bar background).
    g.setColour (ebs::bgDark());
    g.fillRect (0, 0, getWidth(), 50);

    // --- LOGO DRAWING (assets/icon.svg) ---
    juce::Rectangle<float> logoArea (20.0f, 10.0f, 30.0f, 30.0f);
    if (auto* logo = getBrandLogo())
        logo->drawWithin (g, logoArea, juce::Justification::centred, 1.0f);

    // --- TITLE TEXT ---
    // Drawn with GlyphArrangement so "OpenVox" (accent) and "Tuner" (white) sit
    // flush together with no measurement/truncation mismatch. The previous code
    // measured each word's width, cast it to int, and passed it to drawText, which
    // then added an ellipsis ("OpenV...Tun...") because the box was 1px too narrow.
    const juce::Font titleFont = ebs::fontTitle();
    const float titleY = 8.0f;
    // Vertically centre the title against the logo (logoArea centre Y).
    const float baseline = logoArea.getCentreY()
                         + (titleFont.getAscent() - titleFont.getDescent()) * 0.5f;
    float x = 60.0f;

    juce::GlyphArrangement ga;
    ga.addLineOfText (titleFont, "OpenVox", x, baseline);
    g.setColour (ebs::accent());
    ga.draw (g);
    x += juce::GlyphArrangement::getStringWidth (titleFont, "OpenVox");

    ga.clear();
    ga.addLineOfText (titleFont, "Tuner", x, baseline);
    g.setColour (ebs::text());
    ga.draw (g);
    x += juce::GlyphArrangement::getStringWidth (titleFont, "Tuner");

    // Version string (debug builds only - hides the version number in release).
#if defined (JUCE_DEBUG)
    g.setColour (ebs::textDim());
    g.setFont (ovt::fontVersion());
#  if defined (JucePlugin_VersionString)
    g.drawText (juce::String ("v") + JucePlugin_VersionString, x + 8, titleY, 160, 36, juce::Justification::left);
#  elif defined (JucePlugin_Version)
    g.drawText (juce::String ("v") + juce::String (JucePlugin_Version), x + 8, titleY, 160, 36, juce::Justification::left);
#  else
    g.drawText ("v0.1.1", x + 8, titleY, 160, 36, juce::Justification::left);
#  endif
#endif

    // CPU usage meter (debug builds only). Sits to the LEFT of the A/B slot
    // buttons in the top banner, so it is out of the way of the preset
    // selector and the version string. Hidden entirely in release builds.
#if defined (JUCE_DEBUG)
    {
        const int cpuW = 64;
        const int cpuH = 16;
        const int cpuGap = 8;
        // Anchor to the right edge of button A (buttonA has valid bounds once
        // resized() has run, which always precedes paint()).
        const int cpuX = buttonA.getX() - cpuW - cpuGap;
        const int cpuY = buttonA.getY() + (buttonA.getHeight() - cpuH) / 2;

        const int cpuPct = static_cast<int> (currentCpuUsage * 100.0f);
        const juce::String cpuText = ovt::tr(ovt::Keys::kLabelCpu) + juce::String (cpuPct) + "%";

        // Background
        g.setColour (ebs::cpuBg());
        g.fillRoundedRectangle ((float) cpuX, (float) cpuY, (float) cpuW, (float) cpuH, 4.0f);

        // Colour based on usage
        juce::Colour cpuColour;
        if (cpuPct < 30)      cpuColour = juce::Colour (0xff4caf50);
        else if (cpuPct < 60) cpuColour = juce::Colour (0xffffc107);
        else if (cpuPct < 85) cpuColour = juce::Colour (0xffff9800);
        else                  cpuColour = juce::Colour (0xfff44336);

        g.setColour (cpuColour);
        const int barW = static_cast<int> ((cpuW - 8) * juce::jmin (1.0f, currentCpuUsage));
        g.fillRoundedRectangle ((float) (cpuX + 4), (float) (cpuY + cpuH - 4),
                                (float) barW, 2.0f, 1.0f);

        g.setColour (ebs::cpuText());
        g.setFont (ovt::fontLegendHint());
        g.drawText (cpuText, cpuX, cpuY, cpuW, cpuH, juce::Justification::centred);
    }
#endif

    // === Chord badge (ARA or live ovtchord) ===
    // A small pill shown when the host provides lead-sheet chord info via ARA,
    // or when real-time chord detection (ovtchord, MIDI/sidechain) is active in
    // non-ARA mode. Anchored between the preset selector and the A/B cluster.
    {
        juce::String badgeText;
        bool outOfScale = false;
        bool showBadge = false;

        if (processorRef.isBoundToARA_custom())
        {
            int araRoot = -999, araBass = -999;
            juce::String araChordName;
            processorRef.getAraChordAt (processorRef.getInterpolatedTransportTime(), araRoot, araBass, araChordName);
            if (araRoot != -999)
            {
                auto cofToPc = [] (int cof) { return ((cof * 7) % 12 + 12) % 12; };
                // Show just the chord symbol (e.g. "C7", "Csus4"); fall back to
                // root (+bass) if no name could be derived.
                if (araChordName.isNotEmpty())
                    badgeText = araChordName;
                else
                {
                    badgeText = ovtdsp::noteInOctaveName (cofToPc (araRoot));
                    if (araBass != -999 && araBass != araRoot)
                        badgeText += "/" + juce::String (ovtdsp::noteInOctaveName (cofToPc (araBass)));
                }
                outOfScale = processorRef.isAraChordOutOfScale (processorRef.getInterpolatedTransportTime());
                showBadge = true;
            }
        }
        else
        {
            // Non-ARA: show the live chord detected by ovtchord (MIDI/sidechain).
            processorRef.getLiveChord (badgeText, outOfScale);
            showBadge = badgeText.isNotEmpty();
        }

        // When the chord contains notes outside the selected scale, the
        // chord-context override widens the allowed notes (bypass). Show that
        // with a warning colour instead of the normal accent.
        if (showBadge && presetSaveButton.getRight() > 0)
        {
            const juce::Colour badgeColour = outOfScale ? juce::Colour (0xffffb300) : ebs::accent();

            const juce::Font badgeFont = ovt::fontLegendHint();
            const int badgeH = 22;
            const int badgeW = (int) juce::GlyphArrangement::getStringWidth (badgeFont, badgeText) + 18;
            const int badgeX = presetSaveButton.getRight() + 8;
            const int badgeY = 14;
            const juce::Rectangle<int> badgeRect (badgeX, badgeY, badgeW, badgeH);

            g.setColour (badgeColour.withAlpha (0.18f));
            g.fillRoundedRectangle (badgeRect.toFloat(), badgeH * 0.5f);
            g.setColour (badgeColour.withAlpha (0.7f));
            g.drawRoundedRectangle (badgeRect.toFloat(), badgeH * 0.5f, 1.0f);
            g.setColour (badgeColour);
            g.setFont (badgeFont);
            g.drawText (badgeText, badgeRect, juce::Justification::centred);
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::parentHierarchyChanged()
{
    AudioProcessorEditor::parentHierarchyChanged();

    // JUCE's StandaloneFilterWindow is created requesting only the minimise and close
    // title-bar buttons (see juce_StandaloneFilterWindow.h), so the maximise button is
    // absent even though the window is resizable. Re-add it once we are attached to the
    // top-level Standalone window so it can be maximised like a normal desktop app.
    // In a DAW host the top-level component is not a DocumentWindow, so the cast yields
    // nullptr and this is a safe no-op.
    if (auto* w = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))
    {
        w->setTitleBarButtonsRequired (juce::DocumentWindow::minimiseButton
                                        | juce::DocumentWindow::maximiseButton
                                        | juce::DocumentWindow::closeButton, false);

        // System title bar is the house norm: the OS chrome owns the caption, not the
        // JUCE-drawn strip. Only the standalone wrapper owns a top-level window; in a
        // DAW host the cast yields nullptr, so this stays a safe no-op. Guarded so the
        // peer is recreated once, not on every parentHierarchyChanged.
        if (processorRef.isStandaloneWrapper() && ! w->isUsingNativeTitleBar())
            w->setUsingNativeTitleBar (true);
    }
}

void OpenVoxTunerAudioProcessorEditor::timerCallback()
{
    // If the editor is being destroyed, do nothing.  A MessageQueue
    // timer callback may already be in flight when the destructor
    // runs, and the AU's internal state may already be torn down.
    if (processorRef.isEditorShuttingDown())
        return;

    currentCpuUsage = processorRef.getCpuUsage();

    // The output meter: three plain numbers read from the processor's atomic snapshot at
    // this timer's 30 Hz. The audio thread writes them and never paints; the setters are
    // called only when a number actually moved, so an idle plugin repaints nothing.
    const auto meterState = processorRef.getOutputMeterState();
    if (meterState.peakLinear != outputMeter.getPeak())
        outputMeter.setPeak (meterState.peakLinear);
    if (meterState.grDb != outputMeter.getGainReduction())
        outputMeter.setGainReduction (meterState.grDb);
    if (meterState.trimDb != outputMeter.getTrim())
        outputMeter.setTrim (meterState.trimDb);

    // The activity dots: one peak per signal path, read from the processor's own snapshot at
    // this same 30 Hz. Each dot is fed every tick and repaints only when the disc it draws
    // actually moved, so an idle plugin repaints nothing.
    //
    // A dot is gated by the power switch of the module it sits in - the same flag that greys
    // its knob (see the enable block below). An off knob must not claim activity, or the cell
    // reads "switched off" and "working" at once. A bare path peak answered "is this path
    // carrying anything", which the output meter already tells the user; what only the dot can
    // say is "this module is processing something right now".
    const auto pathState = processorRef.getPathActivityState();
    const auto modulePeak = [] (bool moduleOn, float pathPeak)
    { return moduleOn ? pathPeak : 0.0f; };

    noiseGateThresholdSlider.setActivityLevel (modulePeak (noiseGateEnableButton.getToggleState(),  pathState.leadPeak));    // Gate: the lead path
    upwardCompAmountSlider.setActivityLevel (modulePeak (upwardCompEnableButton.getToggleState(),  pathState.leadPeak));      // Compressor: the lead path
    doubleAmountSlider.setActivityLevel (modulePeak (doubleEnableButton.getToggleState(),          pathState.leadPeak));      // Doubler: the lead path
    formantSlider.setActivityLevel (modulePeak (formantEnableButton.getToggleState(),              pathState.leadPeak));      // Formant: the lead path
    deessAmountSlider.setActivityLevel (modulePeak (deessEnableButton.getToggleState(),            pathState.leadPeak));      // De-Esser: the lead path
    saturationDriveSlider.setActivityLevel (modulePeak (saturationEnableButton.getToggleState(),  pathState.masterPeak));    // Saturation: the master path
    delayMixSlider.setActivityLevel (modulePeak (delayEnableButton.getToggleState(),               pathState.leadPeak));      // Delay: the lead path
    leadHallSlider.setActivityLevel (modulePeak (leadReverbEnableButton.getToggleState(),          pathState.leadPeak));      // Lead -> HALL: the lead path
    leadRoomSlider.setActivityLevel (modulePeak (leadReverbEnableButton.getToggleState(),          pathState.leadPeak));      // Lead -> ROOM: the lead path
    harmonyHallSlider.setActivityLevel (modulePeak (harmonyReverbEnableButton.getToggleState(),    pathState.harmonyPeak));   // Harmony -> HALL: the harmony path
    harmonyRoomSlider.setActivityLevel (modulePeak (harmonyReverbEnableButton.getToggleState(),    pathState.harmonyPeak));   // Harmony -> ROOM: the harmony path

    // Output Limit's trim is deliberately NOT gated by limiter_enable - the trim stays live
    // with the limiter off (see the enable block below) - so its dot keeps reporting the
    // master path, exactly as its knob keeps working.
    outputTrimSlider.setActivityLevel (pathState.masterPeak);

    // The standalone tempo field follows the register: the menu's fixed values, a typed value and a
    // restored state all land in the same atomic, so the field must show what the timeline, the
    // Curve Editor's grid and the Delay are actually using. Skipped while the user is typing.
    if (bpmLabel.isVisible() && ! bpmLabel.isBeingEdited())
    {
        const juce::String shown (juce::roundToInt (processorRef.getBpm()));
        const juce::String wanted = shown + " BPM";
        if (bpmLabel.getText() != wanted)
            bpmLabel.setText (wanted, juce::dontSendNotification);
    }

    // One-shot diagnostic dump on the first tick, after all restore/attachment
    // messages have settled, to verify what is actually restored at startup.
    if (! startupDiagLogged)
    {
        startupDiagLogged = true;
#if defined(JUCE_DEBUG) || defined(OVT_FORCE_LOG)
        auto* htype = processorRef.getParameters().getParameter ("harmony_type");
        const float hval = htype != nullptr ? htype->getValue() : -1.0f;
        const int hIdx = (hval >= 0.0f) ? (int) std::round (hval * 21.0f) : -1;
        const ovtdsp::MorphState* slotBms = processorRef.getAbSlotMorphState (1);
        const int slotBType = slotBms != nullptr ? slotBms->harmonyType : -999;
        const ovtdsp::MorphState* slotAms = processorRef.getAbSlotMorphState (0);
        auto f4 = [] (float v) { return juce::String (v, 4); };
        juce::Logger::writeToLog (
            "[DIAG] startup: activeSlot=" + juce::String (processorRef.getAbActiveSlot())
            + " morph=" + juce::String (processorRef.getMorphAmount(), 2)
            + " slotA_hasData=" + juce::String (slotA.hasData ? 1 : 0)
            + " slotB_hasData=" + juce::String (slotB.hasData ? 1 : 0)
            + " slotB_type=" + juce::String (slotBType)
            + " harmony_type_param=" + juce::String (hIdx)
            + " harmony_type_val=" + juce::String (hval, 4)
            + " slotA_type=" + juce::String (slotAms != nullptr ? slotAms->harmonyType : -999)
            + " slotA_vibrato=" + (slotAms ? f4 (slotAms->vibratoPreserve) : juce::String ("null"))
            + " slotA_humanize=" + (slotAms ? f4 (slotAms->humanize) : juce::String ("null"))
            + " slotA_formant=" + (slotAms ? f4 (slotAms->harmonyFormant) : juce::String ("null")));
#endif
    }

    // Apply deferred parameter changes from the audio thread (key/scale detection).
    // Must happen on the UI thread to avoid deadlocks in some hosts.
    processorRef.flushPendingParameterChanges();

    // Read host transport (playhead position, playing state, time signature).
    // getPlayHead()->getPosition() can deadlock Cubase and Live VST3 from
    // the audio thread, so we read it here and cache in atomics.  Wrapped
    // in try/catch because on macOS a torn-down AU can throw Objective-C++
    // exceptions out of JUCE internals.
    try
    {
        processorRef.updateHostTransport();
    }
    catch (...)
    {
    }

    // Read ARA metadata (key/bar signatures) from the host on the UI thread.
    // HostContentReader can deadlock the audio thread in Cubase/Live VST3.
    try
    {
        processorRef.updateAraMetadata();
    }
    catch (...)
    {
    }

    // Repaint the chord badge only when the displayed chord or its out-of-scale
    // state changes (the badge is drawn in paint(); a repaint per tick would
    // redraw the whole banner). In ARA mode the chord is looked up at the
    // current playhead so the badge follows the chord track; in non-ARA mode it
    // reflects the live ovtchord detection (MIDI/sidechain).
    {
        int araRoot = -999, araBass = -999;
        juce::String araChordName;
        processorRef.getAraChordAt (processorRef.getInterpolatedTransportTime(), araRoot, araBass, araChordName);
        const bool outOfScale = processorRef.isAraChordOutOfScale (processorRef.getInterpolatedTransportTime());

        juce::String liveSymbol;
        bool liveOutOfScale = false;
        processorRef.getLiveChord (liveSymbol, liveOutOfScale);

        if (araRoot != lastAraChordBadgeRoot
            || outOfScale != lastAraChordBadgeOutOfScale
            || liveSymbol != lastLiveChordBadgeSymbol)
        {
            lastAraChordBadgeRoot = araRoot;
            lastAraChordBadgeOutOfScale = outOfScale;
            lastLiveChordBadgeSymbol = liveSymbol;
            repaint();
        }
    }

    refreshVisualizer();

    // Forward waveform data to both visualizer and curve editor when enabled
    if (showWaveform)
    {
        juce::AudioBuffer<float> waveform;
        double sr = 44100.0;
        processorRef.copyAraWaveform (waveform, sr);
        if (waveform.getNumSamples() > 0)
        {
            const float* data = waveform.getReadPointer (0);
            if (pitchVisualizer != nullptr)
                pitchVisualizer->setWaveformOverlay (data, waveform.getNumSamples(), sr);
            if (curveEditor != nullptr)
                curveEditor->setWaveformOverlay (data, waveform.getNumSamples(), sr);
        }
    }

    // Sync waveform display type (only when it changes)
    {
        const int dispType = processorRef.getWaveformDisplayType();
        if (dispType != lastWaveformDisplayType)
        {
            lastWaveformDisplayType = dispType;
            if (pitchVisualizer != nullptr)
                pitchVisualizer->setDisplayType (dispType);
            if (curveEditor != nullptr)
                curveEditor->setDisplayType (dispType);
        }
    }

    // Morph: apply whenever the DAW (or the slider) changes the automatable
    // "morph_amount" parameter. Driven here so DAW automation works even without
    // user interaction with the slider.
    {
        const float m = processorRef.getMorphAmount();
        if (m != lastMorphValue)
            onMorphSliderChanged (m);
    }

    // Keep the Harmony sub-toggles (Follow Lead / Gain Match / Use Voice) in
    // sync with the Harmony switch even when Harmony changes WITHOUT a direct
    // click - e.g. preset load or DAW automation. The ButtonAttachment updates
    // the Harmony button with dontSendNotification, so onStateChange does not
    // fire in those cases. When the Harmony enable state changes, re-run
    // refreshLabels() (which disables those sub-toggles based on the Harmony
    // state) instead of only mirroring the two Follow Lead / Gain Match
    // buttons here. refreshLabels() is cheap enough to run once per state
    // change, and it keeps every Harmony-dependent control consistent.
    {
        const bool harmonyOn = harmonyEnableButton.getToggleState();
        if (harmonyOn != lastHarmonyEnabled)
        {
            lastHarmonyEnabled = harmonyOn;
            refreshLabels();
        }
    }

    if (updateCheckState != nullptr)
    {
        if (! updateCheckState->finished.load())
        {
            updateButton.setButtonText (ovt::tr(ovt::Keys::kStatusChecking));
            updateButton.setEnabled (false);
        }
        else
        {
            updateButton.setEnabled (true);
            if (updateCheckState->updateAvailable.load())
            {
                updateButton.setButtonText (ovt::tr(ovt::Keys::kStatusUpdatePrefix) + updateCheckState->latestVersion);
                updateButton.setColour (juce::TextButton::buttonColourId, ebs::accent().darker (0.2f));
                updateButton.setTooltip (ovt::tr(ovt::Keys::kTooltipUpdateAvailable));
            }
            else
            {
                updateButton.setButtonText (ovt::tr(ovt::Keys::kStatusUpToDate));
                updateButton.setColour (juce::TextButton::buttonColourId, ebs::bgPanel());
                updateButton.setTooltip (updateCheckState->statusText.isNotEmpty() ? updateCheckState->statusText : ovt::tr(ovt::Keys::kTooltipUpdateReleases));
            }
        }
    }

    // Sync tab <-> "mode" parameter
    int tabIndex = tabbedComponent.getCurrentTabIndex();

    // Visibility of Curve Editor Mode specific buttons
    bool isCurveEditorMode = (tabIndex == 1);
    
    // (clearHarmonyTraces is now done in the tab-change detection block
    // below - doing it on every tick would wipe the buffer the draw
    // routine needs to render the harmony blue lines.)
    
    optionsButton.setVisible (isCurveEditorMode);
    presetGalleryButton.setVisible (isCurveEditorMode);
    snapButton.setVisible (isCurveEditorMode);
    snapGridButton.setVisible (isCurveEditorMode);
    stepModeButton.setVisible (isCurveEditorMode);
    zoomInButton.setVisible (isCurveEditorMode);
    zoomOutButton.setVisible (isCurveEditorMode);
    scrollUpButton.setVisible (isCurveEditorMode);
    scrollDownButton.setVisible (isCurveEditorMode);
    resetViewButton.setVisible (isCurveEditorMode);

    // Measures control + standalone transport live on the toolbar row.
    measuresLabel.setVisible (isCurveEditorMode);
    measuresComboBox.setVisible (isCurveEditorMode);
    playButton.setVisible (isCurveEditorMode && processorRef.isStandaloneWrapper());
    rewindButton.setVisible (isCurveEditorMode && processorRef.isStandaloneWrapper());

    // Gray out Formant slider if disabled
    bool isFormantEnabled = formantEnableButton.getToggleState();
    formantSlider.setEnabled (isFormantEnabled);

    // Gray out a voice's two reverb amounts when its switch is off. The values are KEPT, so
    // switching the voice back on needs no re-dial, and the first switch-on applies the
    // "ready to sing" amounts the knobs already carry. The ducker is NOT gated: it is a
    // property of the two rooms' return, not of a voice.
    const bool isLeadReverbEnabled = leadReverbEnableButton.getToggleState();
    leadRoomSlider.setEnabled (isLeadReverbEnabled);
    leadHallSlider.setEnabled (isLeadReverbEnabled);

    const bool isHarmonyReverbEnabled = harmonyReverbEnableButton.getToggleState();
    harmonyRoomSlider.setEnabled (isHarmonyReverbEnabled);
    harmonyHallSlider.setEnabled (isHarmonyReverbEnabled);

    // The Output Trim knob is NOT gated by the limiter switch: trim is the level match into
    // the mix, so it stays live with the limiter off (a muted trim with no limiter is still a
    // real gain change).

    // Gray out Upward Comp slider if disabled
    bool isUpwardCompEnabled = upwardCompEnableButton.getToggleState();
    upwardCompAmountSlider.setEnabled (isUpwardCompEnabled);

    // Gray out Doubler knob if disabled: double_enable gates the copy, the amount
    // itself is kept so switching back on re-arms the effect at the same level.
    bool isDoublerEnabled = doubleEnableButton.getToggleState();
    doubleAmountSlider.setEnabled (isDoublerEnabled);

    // Gray out the De-Esser knob if disabled: deess_enable gates the whole effect,
    // and the value is kept so switching back on re-arms the de-esser at the same
    // intensity. The band centre is not a control, so nothing else greys here.
    bool isDeesserEnabled = deessEnableButton.getToggleState();
    deessAmountSlider.setEnabled (isDeesserEnabled);

    // Gray out both saturation knobs if disabled: saturation_enable gates the whole stage,
    // and drive / warmth keep their values so switching back on re-arms the shaper at the
    // same level. The labels grey with their knobs, as for every labelled cell.
    bool isSaturationEnabled = saturationEnableButton.getToggleState();
    saturationDriveSlider.setEnabled (isSaturationEnabled);
    saturationToneSlider.setEnabled (isSaturationEnabled);

    // Gray out the delay controls if disabled: delay_enable gates the whole stage,
    // and division / mix / feedback keep their values so switching back on re-arms the
    // ping-pong at the same setting. The labels grey with knobs per existing convention -
    // setEnabled is not applied to Labels anywhere in this block (saturation does it for
    // its pair via refreshLabels above), and disabled widgets paint dimmed, so the labels
    // follow their controls exactly as saturation's do. Only the combo + two sliders gate.
    bool isDelayEnabled = delayEnableButton.getToggleState();
    delayDivisionKnob.setEnabled (isDelayEnabled);
    delayMixSlider.setEnabled (isDelayEnabled);
    delayFeedbackSlider.setEnabled (isDelayEnabled);

    // Harmony controls enable/disable
    bool isHarmonyEnabled = harmonyEnableButton.getToggleState();
    harmonyTypeBox.setEnabled (isHarmonyEnabled);
    harmonyGainSlider.setEnabled (isHarmonyEnabled);
    harmonyBlendSlider.setEnabled (isHarmonyEnabled);
    harmonyAttackSlider.setEnabled (isHarmonyEnabled);
    harmonyFormantSlider.setEnabled (isHarmonyEnabled);
    // The Doubler lives in the Effects block with its own power toggle, so nothing
    // Harmony does touches it: its greyed state is driven by double_enable above.
    // The doubler doubles the dry lead voice, so it stays live whatever Harmony does.
    // Use Voice is a sub-toggle of Harmony: it must be disabled whenever
    // Harmony is off (otherwise its state is silently read on re-enable).
    useVoiceButton.setEnabled (isHarmonyEnabled);
    // Follow Lead and Gain Match are sub-toggles of Harmony: they must be
    // disabled whenever Harmony itself is off, otherwise the user can still
    // flip them (and the engine would silently read the stale values on the
    // next Harmony re-enable). Centralising this in refreshLabels() also
    // keeps them in sync when Harmony changes via preset load / automation /
    // language switch, not just on a direct click. We also call
    // setInterceptsMouseClicks(false) so the custom PowerButton rendering
    // (which ignores isEnabled() for the glow) cannot be toggled by a click,
    // and setAlpha to make the disabled state visually obvious.
    harmonyFollowLeadButton.setEnabled (isHarmonyEnabled);
    harmonyGainMatchButton.setEnabled (isHarmonyEnabled);
    harmonyFollowLeadButton.setInterceptsMouseClicks (isHarmonyEnabled, isHarmonyEnabled);
    harmonyGainMatchButton.setInterceptsMouseClicks (isHarmonyEnabled, isHarmonyEnabled);
    const bool useVoice = useVoiceButton.getToggleState();
    shiftedVoicesBox.setEnabled (isHarmonyEnabled && useVoice);
    harmonyToneBox.setEnabled (isHarmonyEnabled && !useVoice);
    harmonyToneColorSlider.setEnabled (isHarmonyEnabled && !useVoice);

    // Every caption that names one of the controls gated above follows it: a caption whose control
    // is off or disabled sits at the caption level (50 %), a live control's caption at full
    // strength. Without this, a disabled knob kept a FULL-strength caption (Drive, Warmth, Mix,
    // Division, Feedback) while its own switch sat quiet at 50 %, so the cell read "off" and
    // "active" at the same time.
    applyCaptionDimming();

    // Show only the relevant selector to keep the UI compact and readable
    shiftedVoicesBox.setVisible (isHarmonyEnabled && useVoice);
    harmonyToneBox.setVisible (isHarmonyEnabled && !useVoice);
    harmonyToneColorSlider.setVisible (isHarmonyEnabled && !useVoice);
    harmonyToneColorLabel.setVisible (isHarmonyEnabled && !useVoice);

    // "Reset Playhead" is now in the Curve Editor Options menu; it is disabled
    // there when bound to ARA (the host owns the timeline in ARA mode).

    auto* modeParam = processorRef.getParameters().getParameter("mode");
    
    // If user clicked a tab
    if (tabIndex != static_cast<int>(modeParam->getValue())) {
        modeParam->setValueNotifyingHost(tabIndex == 1 ? 1.0f : 0.0f);
        // Just-entered the Curve Editor: start with a fresh harmony buffer
        // so the first few blue lines appear from a clean state instead of
        // mixing with stale samples from a previous tab visit.
        if (tabIndex == 1 && curveEditor != nullptr)
            curveEditor->clearHarmonyTraces();
    }
    
    // Update edit state and playhead
    if (curveEditor != nullptr) {
        curveEditor->setEditorEnabled(tabIndex == 1);
        // Pass the extrapolated transport time so the playhead moves
        // smoothly at 60 fps even though the worker only refreshes the
        // host PPQ at 30 Hz (see getInterpolatedTransportTime in the
        // processor header).  When the host is paused, the extrapolation
        // returns the frozen PPQ, so the playhead does not drift.
        curveEditor->setPlayheadTime(processorRef.getInterpolatedTransportTime(), processorRef.getIsPlaying(), processorRef.isPlayheadLooping());
        // Propagate time signature (Feature 1) - read from processor
        int num = processorRef.getCurrentTimeSigNumerator();
        int den = processorRef.getCurrentTimeSigDenominator();
        curveEditor->setTimeSignature (num, den);
        // Sync controls with persisted parameters (first time only).
        if (!measuresSyncDone) {
            // Restore Measures combo from the persisted parameter.
            auto* measuresRaw = processorRef.getParameters().getRawParameterValue("editor_measures");
            float measures = measuresRaw ? measuresRaw->load() : 4.0f;
            const int mVal = static_cast<int> (measures);
            if (measuresComboBox.getText().getIntValue() != mVal) {
                for (int i = 0; i < measuresComboBox.getNumItems(); ++i) {
                    if (measuresComboBox.getItemText(i).getIntValue() == mVal) {
                        measuresComboBox.setSelectedItemIndex (i, juce::dontSendNotification);
                        break;
                    }
                }
            }
            curveEditor->setMeasuresVisible (mVal);

            // Restore Auto-Scroll from the persisted parameter. In Standalone the
            // playhead always loops on the Measures window, so auto-scroll (which
            // follows the playhead by panning the view) is disabled/ignored.
            if (processorRef.isStandaloneWrapper())
                curveEditor->setAutoScroll (false);
            else
            {
                auto* scrollRaw = processorRef.getParameters().getRawParameterValue("auto_scroll");
                bool scrollOn = scrollRaw ? (scrollRaw->load() > 0.5f) : true;
                curveEditor->setAutoScroll (scrollOn);
            }

            measuresSyncDone = true;
        }
        // Persist control states -> AudioProcessor parameters (saved/loaded by the host).
        auto* measuresParam = processorRef.getParameters().getRawParameterValue("editor_measures");
        if (measuresParam) {
            int curVal = measuresComboBox.getText().getIntValue();
            if (curVal > 0)
                const_cast<std::atomic<float>*>(measuresParam)->store(static_cast<float>(curVal));
        }
        auto* scrollParam = processorRef.getParameters().getRawParameterValue("auto_scroll");
        if (scrollParam)
            const_cast<std::atomic<float>*>(scrollParam)->store(curveEditor->getAutoScroll() ? 1.0f : 0.0f);
        // Reflect standalone transport state on the toolbar buttons.
        syncTransportButtons();
        // Sync curve from processor to editor after state restore or editor recreation
        if (processorRef.getPendingCurveRestore().load() && curveEditor != nullptr)
        {
            curveEditor->setCurve (processorRef.getPitchCurve());
            processorRef.getPendingCurveRestore().store (false);
            syncEditButtons();
            resetMorph(); // state changed - invalidate any active morph
        }
        // Also sync on every timer tick for safety (e.g. model changes via capture/clear)
        syncEditButtons();
    }

    // Update harmony visuals: forward harmony frequencies to the visualizer
    if (pitchVisualizer != nullptr)
    {
        bool harmonyOn = true;
        auto* rawHarmonyEnable = processorRef.getParameters().getRawParameterValue("harmony_enable");
        if (rawHarmonyEnable != nullptr)
            harmonyOn = (rawHarmonyEnable->load() > 0.5f);

        juce::Array<float> freqsToSend;
        processorRef.copyHarmonyFrequencies (freqsToSend);

        if (! harmonyOn)
            freqsToSend.clear();

        const juce::Array<float> liveFreqs = freqsToSend;
        if (freqsToSend.size() > 0)
        {
            // Detect DAW transport jump (loop wrap, seek) for the harmony
            // traces too. The red input trace is already cleared in
            // refreshVisualizer() the same way - without this, the blue
            // lines from the previous loop iteration remain visible after
            // the playhead wraps back to 0, because they still fall inside
            // the curve editor's timeVisible window.
            const double nowLoop = processorRef.getLoopTransportTime();
            const double loopDelta = std::abs (nowLoop - lastHarmonyTransportTime);
            if (loopDelta > 0.5 && lastHarmonyTransportTime > 0.0 && curveEditor != nullptr)
                curveEditor->clearHarmonyTraces();
            lastHarmonyTransportTime = nowLoop;

        }

        // Keep the Live visualizer on the real snapshot.
        pitchVisualizer->setHarmonyFrequencies (liveFreqs);

        // Use exactly the same timeline as the red input trace in
        // refreshVisualizer(). getInterpolatedTransportTime() may use the
        // host's un-offset PPQ, while getLoopTransportTime() uses the curve
        // editor's actual wrapped/offset timeline. Mixing those clocks is the
        // source of the diagonal blue segments after reset, looping or seek.
        if (curveEditor != nullptr && curveEditor->getShowHarmoniesTrace())
        {
            juce::Array<float> sortedFreqs = freqsToSend;
            sortedFreqs.sort();
            curveEditor->addHarmonySamples (processorRef.getLoopTransportTime(), sortedFreqs);
        }



        // MIDI status label removed; debug window shows detailed MIDI log.
    }

    // === Global plugin Undo/Redo (Option 1) ===
    // Maintain the "before" baseline for discrete clicks/toggles/combos. We
    // only refresh it when NOT applying an undo (to avoid clobbering the real
    // pre-change state) and NOT during a slider drag (the drag-start snapshot
    // is authoritative). DAW automation updates params every block but never
    // fires the gesture handlers below, so it is never pushed to the history.
    if (! applyingUndo_ && ! undoGestureActive_)
        undoLiveSnapshot_ = processorRef.getParameters().copyState();

    // Reflect UndoManager state on the toolbar buttons.
    undoButton.setEnabled (processorRef.getUndoManager().canUndo());
    redoButton.setEnabled (processorRef.getUndoManager().canRedo());
}

void OpenVoxTunerAudioProcessorEditor::setWaveformDisplayType (int type)
{
    processorRef.setWaveformDisplayType (type);
    if (pitchVisualizer != nullptr)
        pitchVisualizer->setDisplayType (type);
    if (curveEditor != nullptr)
        curveEditor->setDisplayType (type);
}

void OpenVoxTunerAudioProcessorEditor::refreshVisualizer()
{
    if (pitchVisualizer == nullptr) return;
    const float hzIn  = processorRef.getCurrentInputPitch();
    const float hzOut = processorRef.getCurrentOutputPitch();
    pitchVisualizer->pushInputPitch  (hzIn);
    pitchVisualizer->pushOutputPitch (hzOut);
    const bool midiActive = processorRef.isMidiTargetActive();
    pitchVisualizer->setMidiFollowActive (midiActive);
    if (curveEditor != nullptr)
    {
        curveEditor->setMidiFollowActive (midiActive);
        curveEditor->setMidiTargetHz (processorRef.getCurrentMidiTargetHz());
    }
    if (curveEditor != nullptr)
    {
        curveEditor->getPianoKeyboard().setCurrentPitches (hzIn, hzOut);
        // Detect DAW transport jump (loop, seek) and clear trace
        const double now = processorRef.getLoopTransportTime();
        const double delta = std::abs (now - lastTransportTime);
        if (delta > 0.5 && lastTransportTime > 0.0) // >0.5s jump = not normal playback
            curveEditor->clearInputTrace();
        lastTransportTime = now;
        if (hzIn > 0.0f && curveEditor->getShowInputTrace())
            curveEditor->addInputTraceSample (now, hzIn);
    }

    // Note info for the header display.
    const ovtdsp::NoteInfo info = ovtdsp::describePitch (hzIn, hzOut);
    pitchVisualizer->setNoteInfo (info);

    // Update the scale intervals (for the background lines).
    // We read them from the parameter tree -> ScaleQuantizer.
    // Note: the quantizer is rebuilt in syncParameters() every block,
    // so we can just retrieve its current intervals.
    if (curveEditor != nullptr)
    {
        // Use the processor's ScaleQuantizer directly for authoritative intervals.
        // This avoids duplicating the scale table and prevents mismatches.
        juce::Array<int> intervals;
        processorRef.copyScaleIntervals (intervals);
        pitchVisualizer->setScaleIntervals (intervals);
        curveEditor->setScaleIntervals (intervals);
        scaleKeyboard.setActiveScaleIntervals (intervals);

        const int keyIdx = processorRef.getKeyIndex();
        const int scaleIdx = processorRef.getScaleIndex();
        
        if (scaleIdx == 13)
        {
            // Custom scale: also sync custom intervals from the scale keyboard
            juce::Array<int> customIntervals;
            for (int i = 0; i < 12; ++i)
                if (scaleKeyboard.getButton(i).getToggleState())
                    customIntervals.add (i);
            curveEditor->setCustomIntervals (customIntervals);
            curveEditor->setKeyAndScale (keyIdx, ovtdsp::Scale::Custom);
        }
        else
        {
            curveEditor->setKeyAndScale (keyIdx, static_cast<ovtdsp::Scale> (juce::jlimit (0, 13, scaleIdx)));
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::pitchCurveChanged()
{
    // The curve editor notified a change: we copy the curve to
    // the processor (thread-safe via getter).
    if (curveEditor == nullptr) return;
    processorRef.getPitchCurve() = curveEditor->getCurve();
}

void OpenVoxTunerAudioProcessorEditor::toggleHelpOverlay()
{
    helpOverlayVisible = ! helpOverlayVisible;
    helpOverlay.setVisible (helpOverlayVisible);
    if (helpOverlayVisible)
        helpOverlay.setBounds (getLocalBounds());
    repaint();
}

void OpenVoxTunerAudioProcessorEditor::mouseDown (const juce::MouseEvent& event)
{
    // Clicking the "Key/Scale Detection" label toggles its power button.
    if (event.eventComponent == &keySourceLabel)
    {
        keyDetectPowerButton.triggerClick();
        return;
    }

    // Right-click on morph slider opens context menu
    if (event.mods.isRightButtonDown())
    {
        auto* source = event.eventComponent;
        if (source == &morphSlider)
        {
            showMorphContextMenu();
        }
    }
    // Double-click on morph slider snaps to center (0.5)
    else if (event.getNumberOfClicks() >= 2)
    {
        auto* source = event.eventComponent;
        if (source == &morphSlider)
        {
            processorRef.setMorphAmount (0.5f);
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::syncTransportButtons()
{
    const bool playing = processorRef.isTransportPlaying();
    // Single Play/Pause toggle: shows the Stop glyph (and tooltip) while playing,
    // the Play glyph (and tooltip) while stopped.
    playButton.setToggleState (playing, juce::dontSendNotification);
    playButton.setTooltip (playing ? ovt::tr (ovt::Keys::kTooltipStop)
                                   : ovt::tr (ovt::Keys::kTooltipPlay));
}








