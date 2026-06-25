// PluginEditor.cpp
// Implementation of the OpenVoxTuner plugin GUI editor.

#include "PluginProcessor.h"
#include "PluginEditor.h"

// === Theme colors ("autotune" style: dark + pink/purple accent) ===
const juce::Colour OpenVoxTunerAudioProcessorEditor::kBgDark     = juce::Colour::fromString("#FF101115"); // Deep background
const juce::Colour OpenVoxTunerAudioProcessorEditor::kBgPanel    = juce::Colour::fromString("#FF1A1B22"); // Dark panels
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
    juce::File getUserPresetsDirectory()
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("OpenVoxTuner")
                       .getChildFile ("Presets");
        dir.createDirectory();
        return dir;
    }

    juce::String sanitizePresetFileStem (juce::String name)
    {
        name = name.trim();
        if (name.isEmpty())
            return "Preset";

        juce::String out;
        out.preallocateBytes (name.getNumBytesAsUTF8());

        for (auto c : name)
        {
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                out << juce::String::charToString (c);
            else if (c == ' ' || c == '-' || c == '_')
                out << "_";
        }

        out = out.trimCharactersAtEnd ("_").trimCharactersAtStart ("_");
        if (out.isEmpty())
            out = "Preset";

        return out;
    }

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

OpenVoxTunerAudioProcessorEditor::OpenVoxTunerAudioProcessorEditor (OpenVoxTunerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&customLookAndFeel);
    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 100);
    tooltipWindow->setLookAndFeel (&customLookAndFeel);

    updateButton.setButtonText ("Check updates");
    updateButton.setTooltip ("Check the latest OpenVoxTuner release on GitHub.");
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
    updateButton.setColour (juce::TextButton::buttonColourId, kBgPanel);
    updateButton.setColour (juce::TextButton::textColourOffId, kText);
    updateButton.setColour (juce::TextButton::textColourOnId, kAccent);
    addAndMakeVisible (updateButton);

    startUpdateCheck();

    // === Configuration of the 4 sliders (rotary knobs) ===
    auto setupKnob = [this] (juce::Slider& s, juce::Label* l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        s.setColour (juce::Slider::rotarySliderFillColourId,    kAccent);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, kAccentSoft);
        s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
        s.setColour (juce::Slider::textBoxTextColourId,         kText);
        s.setColour (juce::Slider::textBoxOutlineColourId,      kAccentSoft);
        s.setColour (juce::Slider::textBoxBackgroundColourId,   kBgPanel);
        addAndMakeVisible (s);

        if (l != nullptr)
        {
            l->setText (name, juce::dontSendNotification);
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::textColourId, kText);
            l->setFont (juce::Font (13.0f, juce::Font::bold));
            addAndMakeVisible (*l);
        }
    };

    setupKnob (speedSlider,  &speedLabel,  "Speed (ms)");
    setupKnob (amountSlider, &amountLabel, "Amount");
    setupKnob (formantSlider, nullptr, "");

    // === Harmony UI ===
    // Harmony enable toggle (use same visual style as Formant)
    harmonyEnableButton.setButtonText ("Harmony");
    harmonyEnableButton.setName ("PowerButton");
    harmonyEnableButton.setColour (juce::ToggleButton::textColourId, kText);
    harmonyEnableButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    harmonyEnableButton.setTooltip ("Enable/disable harmony generation.");
    addAndMakeVisible (harmonyEnableButton);

    // Harmony type combo — index 3 = "3rd Below + Above" (default)
    harmonyTypeBox.addItemList (juce::StringArray {
        "None",
        "3rd Below", "3rd Above", "3rd Below + Above",
        "5th Below", "5th Above", "5th Below + Above",
        "Vocal Stack (3 voices)", "Vocal Stack (4 voices)",
        "Power Chord", "Parallel 3rd", "Drone"
    }, 1);
    harmonyTypeBox.setSelectedItemIndex (3, juce::dontSendNotification);
    harmonyTypeBox.setColour (juce::ComboBox::backgroundColourId, kBgPanel);
    harmonyTypeBox.setColour (juce::ComboBox::textColourId, kText);
    harmonyTypeBox.setColour (juce::ComboBox::outlineColourId, kAccentSoft);
    addAndMakeVisible (harmonyTypeBox);

    // Harmony knobs (Volume, Blend) — use same rotary knob style as main knobs
    setupKnob (harmonyGainSlider, &harmonyGainLabel, "Volume");
    harmonyGainSlider.setRange (0.0, 1.0, 0.01);
    harmonyGainSlider.setValue (1.0);

    setupKnob (harmonyBlendSlider, &harmonyBlendLabel, "Blend");
    harmonyBlendSlider.setRange (0.0, 1.0, 0.01);
    harmonyBlendSlider.setValue (0.5);

    // Use Voice controls
    useVoiceButton.setButtonText ("Use Voice");
    useVoiceButton.setColour (juce::ToggleButton::textColourId, kText);
    useVoiceButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (useVoiceButton);

    shiftedVoicesBox.addItemList ({ "1", "2", "3", "4" }, 1);
    shiftedVoicesBox.setSelectedId (4, juce::dontSendNotification);
    shiftedVoicesBox.setColour (juce::ComboBox::backgroundColourId, kBgPanel);
    shiftedVoicesBox.setColour (juce::ComboBox::textColourId, kText);
    shiftedVoicesBox.setColour (juce::ComboBox::outlineColourId, kAccentSoft);
    addAndMakeVisible (shiftedVoicesBox);

    harmonyToneBox.addItemList ({ "Choir", "Bright", "Synth Lead", "Strings", "Guitar", "Vocoder-like" }, 1);
    harmonyToneBox.setSelectedItemIndex (0, juce::dontSendNotification);
    harmonyToneBox.setColour (juce::ComboBox::backgroundColourId, kBgPanel);
    harmonyToneBox.setColour (juce::ComboBox::textColourId, kText);
    harmonyToneBox.setColour (juce::ComboBox::outlineColourId, kAccentSoft);
    addAndMakeVisible (harmonyToneBox);

    harmonyToneColorLabel.setText ("Tone", juce::dontSendNotification);
    harmonyToneColorLabel.setJustificationType (juce::Justification::centred);
    harmonyToneColorLabel.setColour (juce::Label::textColourId, kText);
    harmonyToneColorLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (harmonyToneColorLabel);

    harmonyToneColorSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    harmonyToneColorSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    harmonyToneColorSlider.setRange (0.0, 1.0, 0.01);
    harmonyToneColorSlider.setValue (0.5, juce::dontSendNotification);
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderOutlineColourId, kAccentSoft);
    harmonyToneColorSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    harmonyToneColorSlider.setTooltip ("Tone color for synth harmonies.");
    addAndMakeVisible (harmonyToneColorSlider);

    // Key and Scale are discrete values -> ComboBox.
    auto setupCombo = [this] (juce::ComboBox& b, juce::Label& l, const juce::String& name,
                              const juce::StringArray& items, int initialIndex)
    {
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, kText);
        l.setFont (juce::Font (13.0f, juce::Font::bold));
        addAndMakeVisible (l);

        for (int i = 0; i < items.size(); ++i)
            b.addItem (items[i], i + 1);
        b.setSelectedItemIndex (initialIndex, juce::dontSendNotification);
        b.setColour (juce::ComboBox::backgroundColourId, kBgPanel);
        b.setColour (juce::ComboBox::textColourId,       kText);
        b.setColour (juce::ComboBox::outlineColourId,    kAccentSoft);
        b.setColour (juce::ComboBox::arrowColourId,      kAccent);
        addAndMakeVisible (b);
    };

    setupCombo (keyBox,   keyLabel,   "Root", kNoteNames,  0);
    setupCombo (scaleBox, scaleLabel, "Scale", kScaleNames, 0);

    latencyModeLabel.setText ("", juce::dontSendNotification);
    latencyModeLabel.setJustificationType (juce::Justification::centred);
    latencyModeLabel.setColour (juce::Label::textColourId, kText);
    latencyModeLabel.setFont (juce::Font (12.0f, juce::Font::bold));
    latencyModeLabel.setVisible (false);

    latencyModeBox.addItemList ({ "Low Latency", "Quality", "Safe" }, 1);
    latencyModeBox.setSelectedItemIndex (1, juce::dontSendNotification);
    latencyModeBox.setColour (juce::ComboBox::backgroundColourId, kBgPanel);
    latencyModeBox.setColour (juce::ComboBox::textColourId, kText);
    latencyModeBox.setColour (juce::ComboBox::outlineColourId, kAccentSoft);
    latencyModeBox.setColour (juce::ComboBox::arrowColourId, kAccent);
    addAndMakeVisible (latencyModeBox);

    // === Slider ranges ===
    speedSlider.setRange (0.0, 200.0, 1.0);
    speedSlider.setValue (50.0);
    amountSlider.setRange (0.0, 1.0, 0.01);
    amountSlider.setValue (1.0);

    // === Manual bindings for ComboBox Key/Scale to AudioParameterInt ===
    // Using ComboBoxAttachment for perfect sync with the host
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "key", keyBox);
        
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "scale", scaleBox);
    latencyModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "latency_mode", latencyModeBox);

    // UI updates (visibility of custom buttons, etc.) are handled in timerCallback.

    // Helper for creating SVG paths for our toolbar buttons
    auto createDrawable = [](const juce::Path& p, juce::Colour strokeColor) {
        auto d = std::make_unique<juce::DrawablePath>();
        d->setPath(p);
        d->setStrokeType(juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        d->setStrokeFill(strokeColor);
        return d;
    };
    
    auto setupIconButton = [&](juce::DrawableButton& btn, const juce::Path& path, bool isToggle, const juce::String& tooltip) {
        auto normal = createDrawable(path, juce::Colours::grey);
        auto over = createDrawable(path, juce::Colours::lightgrey);
        auto down = createDrawable(path, juce::Colours::white);
        
        if (isToggle) {
            auto normalOn = createDrawable(path, kAccent);
            auto overOn = createDrawable(path, kAccent.brighter(0.2f));
            auto downOn = createDrawable(path, juce::Colours::white);
            btn.setImages(normal.get(), over.get(), down.get(), nullptr,
                          normalOn.get(), overOn.get(), downOn.get(), nullptr);
            btn.setClickingTogglesState(true);
        } else {
            btn.setImages(normal.get(), over.get(), down.get());
        }
        
        btn.setTooltip(tooltip);
        // Custom background colors for DrawableButton
        btn.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::DrawableButton::backgroundOnColourId, kAccent.withAlpha(0.2f));
        btn.setColour(juce::DrawableButton::textColourId, juce::Colours::white);
        btn.setColour(juce::DrawableButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible(btn);
    };

    // Paths definitions
    juce::Path pathScale;
    pathScale.addEllipse(2, 10, 4, 4);
    pathScale.addLineSegment(juce::Line<float>(6, 12, 6, 2), 1.5f);
    pathScale.addLineSegment(juce::Line<float>(6, 2, 14, 2), 1.5f);
    pathScale.addLineSegment(juce::Line<float>(14, 2, 14, 8), 1.5f);
    pathScale.addEllipse(10, 6, 4, 4);

    juce::Path pathGrid;
    pathGrid.addLineSegment(juce::Line<float>(4, 0, 4, 16), 1.5f);
    pathGrid.addLineSegment(juce::Line<float>(12, 0, 12, 16), 1.5f);
    pathGrid.addLineSegment(juce::Line<float>(0, 4, 16, 4), 1.5f);
    pathGrid.addLineSegment(juce::Line<float>(0, 12, 16, 12), 1.5f);

    juce::Path pathStep;
    pathStep.addLineSegment(juce::Line<float>(0, 12, 8, 12), 1.5f);
    pathStep.addLineSegment(juce::Line<float>(8, 12, 8, 4), 1.5f);
    pathStep.addLineSegment(juce::Line<float>(8, 4, 16, 4), 1.5f);
    pathStep.addEllipse(7, 3, 2, 2);
    pathStep.addEllipse(7, 11, 2, 2);

    juce::Path pathClear;
    pathClear.addLineSegment(juce::Line<float>(3, 3, 13, 13), 2.0f);
    pathClear.addLineSegment(juce::Line<float>(3, 13, 13, 3), 2.0f);

    juce::Path pathReset;
    pathReset.addLineSegment(juce::Line<float>(2, 2, 2, 14), 2.0f);
    pathReset.addTriangle(14, 2, 14, 14, 4, 8);

    juce::Path pathPresets;
    pathPresets.addLineSegment (juce::Line<float> (2, 4, 14, 4), 2.0f);
    pathPresets.addLineSegment (juce::Line<float> (2, 8, 14, 8), 2.0f);
    pathPresets.addLineSegment (juce::Line<float> (2, 12, 14, 12), 2.0f);

    // Setup Toolbar Buttons
    // Custom button: icon + text
{
    // Set up the custom PresetsButton member
    auto normal = createDrawable(pathPresets, juce::Colours::grey);
    auto over   = createDrawable(pathPresets, juce::Colours::lightgrey);
    auto down   = createDrawable(pathPresets, juce::Colours::white);

    // Assign the icon (use the first drawable for normal state)
    presetsButton.setIcon(std::move(normal));

    // Ensure proper layout and appearance

    presetsButton.setSize(80, 22);
    addAndMakeVisible(presetsButton);

    // Callbacks and tooltip
    presetsButton.onClick = [this] { showPresetsMenu(); };
    presetsButton.setTooltip ("Presets.\n"
                              "Factory: load built-in presets.\n"
                              "Custom: load/save/delete your own presets.");
}

    setupIconButton(snapButton, pathScale, true, "Snap to scale");
    snapButton.setToggleState(true, juce::dontSendNotification);
    snapButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setSnapEnabled(snapButton.getToggleState());
    };
    snapButton.setTooltip ("Snap to scale");

    setupIconButton(snapGridButton, pathGrid, true, "Snap to grid");
    snapGridButton.setToggleState(true, juce::dontSendNotification);
    snapGridButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setSnapToGridEnabled(snapGridButton.getToggleState());
    };
    snapGridButton.setTooltip ("Snap to grid");

    setupIconButton(stepModeButton, pathStep, true, "Step mode (staircase interpolation)");
    stepModeButton.setToggleState(true, juce::dontSendNotification);
    stepModeButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->setStepModeEnabled(stepModeButton.getToggleState());
    };
    stepModeButton.setTooltip ("Step mode");
    stepModeButton.setTooltip ("Step mode.\n"
                               "Holds the pitch until the next point, then jumps vertically.");

    setupIconButton(clearCurveButton, pathClear, false, "Clear all points");
    clearCurveButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->clearCurve();
        processorRef.resetTransportTime();
    };
    clearCurveButton.setTooltip ("Clear curve.\n"
                                 "Removes all points and resets the internal playhead time.");

    setupIconButton(resetTransportButton, pathReset, false, "Reset playhead");
    resetTransportButton.onClick = [this] {
        processorRef.resetTransportTime();
    };
    resetTransportButton.setTooltip ("Reset playhead.\n"
                                     "Resets the internal timeline offset (useful in Standalone / classic VST3).");
    // Power-style icons: Bypass and MIDI Out (use same glyph, Bypass larger)
    juce::Path pathPower;
    pathPower.addEllipse(2, 2, 20, 20);
    pathPower.startNewSubPath(13, 2);
    pathPower.lineTo(13, 10);

    setupIconButton(bypassButton, pathPower, true, "Bypass audio processing");
    bypassButton.setTooltip ("Bypass audio processing.\nWhen enabled, audio passes through without correction.");
    addAndMakeVisible (bypassButton);

    // MIDI Out icon (clicking toggles the attached toggle button)
    setupIconButton(midiOutButton, pathPower, true, "Enable MIDI Out");
    addAndMakeVisible (midiOutButton);

    // Toggle buttons with text (attached to parameters)
    bypassToggleButton.setButtonText ("ByPass");
    bypassToggleButton.setColour (juce::ToggleButton::textColourId, kText);
    bypassToggleButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    bypassToggleButton.setTooltip ("Bypass audio processing.");
    addAndMakeVisible (bypassToggleButton);

    midiToggleButton.setButtonText ("MIDI OUT");
    midiToggleButton.setColour (juce::ToggleButton::textColourId, kText);
    midiToggleButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    midiToggleButton.setTooltip ("Enable MIDI Out");
    addAndMakeVisible (midiToggleButton);

    // Measures selector (Feature 1) — visible only in Curve Editor mode.
    measuresLabel.setText ("Measures", juce::dontSendNotification);
    measuresLabel.setJustificationType (juce::Justification::centred);
    measuresLabel.setColour (juce::Label::textColourId, kText);
    measuresLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    addAndMakeVisible (measuresLabel);

    measuresBox.addItemList ({ "1", "2", "4", "8" }, 1);
    measuresBox.setSelectedItemIndex (2, juce::dontSendNotification);
    measuresBox.setColour (juce::ComboBox::backgroundColourId, kBgPanel);
    measuresBox.setColour (juce::ComboBox::textColourId, kText);
    measuresBox.setColour (juce::ComboBox::outlineColourId, kAccentSoft);
    measuresBox.setColour (juce::ComboBox::arrowColourId, kAccent);
    addAndMakeVisible (measuresBox);

    measuresAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "editor_measures", measuresBox);

    // Auto-Scroll toggle (Feature 2) — visible only in Curve Editor mode.
    autoScrollToggle.setButtonText ("Auto-Scroll");
    autoScrollToggle.setColour (juce::ToggleButton::textColourId, kText);
    autoScrollToggle.setColour (juce::ToggleButton::tickColourId, kAccent);
    autoScrollToggle.setTooltip ("Automatically scroll the editor view during playback");
    addAndMakeVisible (autoScrollToggle);

    autoScrollAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.getParameters(), "auto_scroll", autoScrollToggle);

    // Hide legacy icon buttons from top bar (kept for backward compatibility)
    bypassButton.setVisible (false);
    midiOutButton.setVisible (false);

    // Debug window button near bypass
   #if JUCE_DEBUG
    debugWindowButton.setButtonText ("Debug");
    debugWindowButton.setTooltip ("Open debug window: MIDI log, attack/release testing");
    addAndMakeVisible (debugWindowButton);
    debugWindowButton.onClick = [this]() {
        // Create a simple modeless window if not already
        static juce::Component::SafePointer<juce::DocumentWindow> dbgWindow;
        if (dbgWindow != nullptr)
        {
            dbgWindow->setVisible (true);
            dbgWindow->toFront (true);
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

                midiLog.setMultiLine(true);
                midiLog.setReadOnly(true);
                addAndMakeVisible(midiLog);

                startTimerHz (10);
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
                // update midi log
                juce::String s;
                for (int ch = 1; ch <= 9; ++ch)
                {
                    int note = proc.getLastSentMidiNoteForChannel(ch);
                    if (note >= 0) s += "Ch" + juce::String(ch) + ":" + juce::String(note) + "\n";
                }
                midiLog.setText(s, juce::dontSendNotification);
            }
        private:
            OpenVoxTunerAudioProcessor& proc;
            juce::Label attackSliderLabel, releaseSliderLabel;
            juce::Slider attackSlider, releaseSlider;
            juce::TextButton applyButton, clearButton;
            juce::TextButton dumpButton;
            juce::TextButton testGrainButton;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> testGrainAttachment;
            juce::TextEditor midiLog;
        };

        w->setContentOwned (new DebugContent (processorRef), true);
        w->setUsingNativeTitleBar (true);
        w->setResizable (true, false);
        w->centreWithSize (400, 320);
        w->setVisible (true);
        dbgWindow = w;
    };
   #else
    debugWindowButton.setVisible (false);
    debugWindowButton.setEnabled (false);
   #endif

    formantEnableButton.setButtonText ("Formant");
    formantEnableButton.setName ("PowerButton");
    formantEnableButton.setColour (juce::ToggleButton::textColourId, kText);
    formantEnableButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    addAndMakeVisible (formantEnableButton);

    addAndMakeVisible (scaleKeyboard);

    // === Bidirectional attachments to AudioParameters ===
    auto& tree = processorRef.getParameters();
    speedAttachment  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "speed",  speedSlider);
    amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "amount", amountSlider);
    formantAttachment= std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "formant", formantSlider);
    formantEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "formant_enable", formantEnableButton);
    // Attach the textual toggle buttons to parameters
    bypassToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "bypass", bypassToggleButton);
    midiToggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "midi_out_enable", midiToggleButton);
    // also keep legacy names for backward compatibility
    bypassAttachment = nullptr;
    midiOutAttachment = nullptr;

    // Harmony attachments
    harmonyEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (tree, "harmony_enable", harmonyEnableButton);
    harmonyTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (tree, "harmony_type", harmonyTypeBox);
    harmonyGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_gain", harmonyGainSlider);
    harmonyBlendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (tree, "harmony_blend", harmonyBlendSlider);
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
            auto* rawScale = processorRef.getParameters().getRawParameterValue("scale");
            // Switch to custom scale if not already
            if (rawScale != nullptr && static_cast<int>(std::round(rawScale->load())) != 15) {
                auto* scaleParam = processorRef.getParameters().getParameter("scale");
                if (scaleParam != nullptr)
                    scaleParam->setValueNotifyingHost(15.0f / 15.0f); // 15 is Custom (1.0 normalized)
            }
        };
    }

    // Key/scale ComboBox binding was already done above via ComboBoxAttachment.
    // === Pitch visualizer ===
    pitchVisualizer = std::make_unique<ui::PitchVisualizer>();

    curveEditor = std::make_unique<ui::PitchCurveEditor>();
    curveEditor->addListener (this);
    curveEditor->setViewRange (16.0, 50.0f, 1000.0f);
    curveEditor->onRightClick = [this] (const juce::MouseEvent& e) {
        showPresetsMenu (&e);
    };

    // Initialize tabs
      tabbedComponent.setOutline(0);
      tabbedComponent.addTab("Live", kBgPanel, pitchVisualizer.get(), false);
      tabbedComponent.addTab("Curve Editor", kBgPanel, curveEditor.get(), false);
    addAndMakeVisible(tabbedComponent);

    // Make sure tools are drawn over the tabbed component
    presetsButton.toFront(false);
    snapButton.toFront(false);
    snapGridButton.toFront(false);
    stepModeButton.toFront(false);
    clearCurveButton.toFront(false);
    resetTransportButton.toFront(false);
    
    // Initialize tab from processor parameter
    float initialMode = processorRef.getParameters().getParameter("mode")->getValue();
    tabbedComponent.setCurrentTabIndex(initialMode > 0.5f ? 1 : 0);
    
    // Tab change callback
    // Parameter update is done in timerCallback() to avoid complex loops.

    // Default size: AFTER creating children, otherwise setSize triggers
    // resized() which accesses pitchVisualizer/curveEditor still nullptr.
    setSize (900, 650);
    setResizable (true, true);
    setResizeLimits (920, 600, 1920, 1080);

    // Timer to update the visualizer (~30 fps).
    startTimerHz (30);
}

OpenVoxTunerAudioProcessorEditor::~OpenVoxTunerAudioProcessorEditor()
{
    if (updateCheckState != nullptr)
        updateCheckState->cancelled.store (true);

    stopTimer();
    if (tooltipWindow != nullptr)
    {
        tooltipWindow->setLookAndFeel (nullptr);
        tooltipWindow.reset();
    }
    setLookAndFeel (nullptr);
    if (curveEditor != nullptr) curveEditor->removeListener();
}

void OpenVoxTunerAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Main background (modern gradient)
    juce::ColourGradient bgGradient (kBgDark.brighter(0.05f), 0, 0,
                                     kBgDark.darker(0.2f), 0, (float)getHeight(), false);
    g.setGradientFill (bgGradient);
    g.fillAll();

    // Draw the bottom blocks backgrounds (left = scale, middle = knobs, right = harmony)
    g.setColour (kBgPanel.withAlpha(0.6f));
    g.fillRoundedRectangle (block1Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block2Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block3Bounds.toFloat(), 6.0f);

    g.setColour (kAccentSoft.withAlpha(0.3f));
    g.drawRoundedRectangle (block1Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block2Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block3Bounds.toFloat(), 6.0f, 1.0f);

    // Top banner with title.
    g.setColour (kBgPanel.withAlpha(0.8f));
    g.fillRect (0, 0, getWidth(), 50);

    // --- LOGO DRAWING ---
    juce::Rectangle<float> logoArea (20.0f, 10.0f, 30.0f, 30.0f);
    
    // Draw stylized O
    g.setColour(kAccent);
    g.drawEllipse(logoArea.reduced(2.0f), 3.0f);
    
    // Draw pitch curve passing through O
    juce::Path curve;
    curve.startNewSubPath(logoArea.getX() - 5.0f, logoArea.getCentreY() + 5.0f);
    curve.cubicTo(logoArea.getX() + 10.0f, logoArea.getCentreY() + 5.0f,
                  logoArea.getCentreX(), logoArea.getY() - 5.0f,
                  logoArea.getRight() + 5.0f, logoArea.getY() + 10.0f);
    g.setColour(juce::Colours::white);
    g.strokePath(curve, juce::PathStrokeType(2.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    // --- TITLE TEXT ---
    // Try to load modern sans-serif fonts, fallback to standard sans
    juce::Font titleFont ("Segoe UI", 26.0f, juce::Font::bold);
    titleFont.setTypefaceName("Helvetica Neue"); // Will be ignored on Windows if not installed, falls back properly
    if (titleFont.getTypefaceName() != "Helvetica Neue" && titleFont.getTypefaceName() != "Segoe UI")
        titleFont = juce::Font(26.0f, juce::Font::bold); // ultimate fallback

    g.setFont (titleFont);
    
    // Measure width dynamically to perfectly stick the two words together
    int openVoxWidth = titleFont.getStringWidth("OpenVox");
    
    // "OpenVox" in Accent Color
    g.setColour (kAccent);
    g.drawText ("OpenVox", 60, 8, openVoxWidth, 36, juce::Justification::centredLeft);
    
    // "Tuner" in White
    g.setColour (kText);
    int tunerWidth = titleFont.getStringWidth("Tuner");
    g.drawText ("Tuner", 60 + openVoxWidth, 8, tunerWidth, 36, juce::Justification::centredLeft);

    g.setColour (kText.withAlpha (0.5f));
    g.setFont (12.0f);
#if defined (JucePlugin_VersionString)
    g.drawText ((juce::String("v") + JucePlugin_VersionString), 60 + openVoxWidth + tunerWidth + 8, 8, 120, 36, juce::Justification::centredLeft);
#elif defined (JucePlugin_Version)
    g.drawText ((juce::String("v") + juce::String (JucePlugin_Version)), 60 + openVoxWidth + tunerWidth + 8, 8, 120, 36, juce::Justification::centredLeft);
#else
    g.drawText ("v0.1.1", 60 + openVoxWidth + tunerWidth + 8, 8, 120, 36, juce::Justification::centredLeft);
#endif
}

void OpenVoxTunerAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // === Top banner (title + tools + bypass + midi + latency + debug) ===
    auto titleArea = bounds.removeFromTop (50);

    const bool showBypass = processorRef.isStandaloneWrapper();
    const int toggleW = 98;
    const int latencyW = 110;
    const int updateW = 118;

   #if JUCE_DEBUG
    const int debugW = 72;
    auto rightTools = titleArea.removeFromRight (showBypass ? 540 : 450);
    updateButton.setBounds (rightTools.removeFromRight (updateW).reduced (4, 10));
    debugWindowButton.setBounds (rightTools.removeFromRight (debugW).reduced (4, 10));
   #else
    auto rightTools = titleArea.removeFromRight (showBypass ? 460 : 370);
    updateButton.setBounds (rightTools.removeFromRight (updateW).reduced (4, 10));
    debugWindowButton.setBounds (0, 0, 0, 0);
   #endif

    midiToggleButton.setBounds (rightTools.removeFromRight (toggleW).reduced (4, 10));

    auto latencyArea = rightTools.removeFromRight (latencyW).reduced (4, 8);
    latencyModeLabel.setBounds (0, 0, 0, 0);
    latencyModeBox.setBounds (latencyArea);

    if (showBypass)
        bypassToggleButton.setBounds (rightTools.removeFromRight (toggleW).reduced (4, 10));
    else
        bypassToggleButton.setBounds (0, 0, 0, 0);

    // Legacy icon buttons hidden
    bypassButton.setBounds (0, 0, 0, 0);
    midiOutButton.setBounds (0, 0, 0, 0);

    // === Visualizer (top) and Graphic Editor (middle) ===
    const int pad = 10;
    // Reserve 140 px for the bottom area (controls + scale keyboard)
    auto centerArea = bounds.removeFromTop (bounds.getHeight() - 140);
    tabbedComponent.setBounds (centerArea.reduced (pad));
    
    // Graphic Mode specific tools aligned to the right of the tab bar
    auto tabBounds = tabbedComponent.getBounds();
    auto toolsArea = tabBounds.removeFromTop(30).reduced(2, 4); // height is 22
    
    int iconSize = toolsArea.getHeight(); // 22

    // Left side: Measures selector + Auto-Scroll toggle (Feature 1 & 2)
    auto leftTools = toolsArea.removeFromLeft(260);
    measuresLabel.setBounds (leftTools.removeFromLeft (50));
    measuresBox.setBounds (leftTools.removeFromLeft (60));
    leftTools.removeFromLeft (8);
    autoScrollToggle.setBounds (leftTools.removeFromLeft (100));
    
    // Right side: existing toolbar icons
    resetTransportButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    clearCurveButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    stepModeButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    snapGridButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    snapButton.setBounds (toolsArea.removeFromRight(iconSize));
    toolsArea.removeFromRight(8);
    presetsButton.setBounds (toolsArea.removeFromRight(80));

    // === Bottom bar: 3 blocks ===
    auto bottomArea = bounds.reduced (pad);

    // Layout: left = knobs, middle = scale/keyboard, right = harmony controls
    const int knobBlockWidth = 300;
    const int scaleBlockWidth = 280; // reduced by ~30% from previous 400
    const int blockSpacing = 15;

    auto leftBlock = bottomArea.removeFromLeft(knobBlockWidth);
    bottomArea.removeFromLeft(blockSpacing);
    auto middleBlock = bottomArea.removeFromLeft(scaleBlockWidth);
    bottomArea.removeFromLeft(blockSpacing);
    auto rightBlock = bottomArea; // remaining area

    block2Bounds = leftBlock;   // Speed, Amount, Formant
    block1Bounds = middleBlock; // Key, Scale, Keyboard
    block3Bounds = rightBlock;  // Harmony controls

    // --- Block 2 : Correction Knobs (Left) ---
    auto b2 = block2Bounds.reduced(10);
    // Reserve a fixed area at the top of block2 for knobs
    const int knobsHeight = 100;
    auto knobArea = b2.removeFromTop (knobsHeight);

    int knobWidth = knobArea.getWidth() / 3;

    auto bSpeed = knobArea.removeFromLeft(knobWidth);
    speedLabel.setBounds(bSpeed.removeFromTop(20));
    speedSlider.setBounds(bSpeed);

    auto bAmount = knobArea.removeFromLeft(knobWidth);
    amountLabel.setBounds(bAmount.removeFromTop(20));
    amountSlider.setBounds(bAmount);

    // The Formant button acts as On/Off and displays "Formant"
    auto formantTop = knobArea.removeFromTop(20);
    formantEnableButton.setBounds(formantTop); // Takes full width, centered drawing handles alignment

    formantSlider.setBounds(knobArea);

    // Harmony controls block (rightmost block)
    {
        auto h = block3Bounds.reduced(10);
        auto harmonyArea = h;

        auto leftCol = harmonyArea.removeFromLeft ((int) std::round (h.getWidth() * 0.58f));
        harmonyArea.removeFromLeft (8);
        auto rightCol = harmonyArea;

        harmonyEnableButton.setBounds (leftCol.removeFromTop(24).removeFromLeft(130).reduced(2));
        harmonyTypeBox.setBounds (leftCol.removeFromTop(26));

        auto uvRow = leftCol.removeFromTop(22);
        useVoiceButton.setBounds (uvRow.removeFromLeft(120).reduced(2));
        harmonyToneColorLabel.setBounds (uvRow.removeFromRight(34));

        auto selectorRow = leftCol.removeFromTop(28);
        auto selectorBox = selectorRow.removeFromLeft (juce::jmax (90, leftCol.getWidth() - 34));
        shiftedVoicesBox.setBounds (selectorBox.reduced (0, 2));
        harmonyToneBox.setBounds (selectorBox.reduced (0, 2));
        harmonyToneColorSlider.setBounds (selectorRow.withSizeKeepingCentre (28, 24));

        // Knobs on the right column (two rotary knobs side-by-side)
        int knobAreaHeight = 80;
        auto knobArea = rightCol.removeFromTop(knobAreaHeight);
        int hkWidth = knobArea.getWidth() / 2;
        auto hk1 = knobArea.removeFromLeft(hkWidth);
        harmonyGainLabel.setBounds(hk1.removeFromTop(20));
        harmonyGainSlider.setBounds(hk1);
        auto hk2 = knobArea;
        harmonyBlendLabel.setBounds(hk2.removeFromTop(20));
        harmonyBlendSlider.setBounds(hk2);
    }

    // --- Block 1 : Key, Scale, Keyboard (middle) ---
    auto b1 = block1Bounds.reduced(10);

    auto topRow = b1.removeFromTop(44); // 20 label + 24 combobox

    // Left: Key
    auto bKey = topRow.removeFromLeft(80);
    keyLabel.setBounds(bKey.removeFromTop(20));
    keyBox.setBounds(bKey);

    topRow.removeFromLeft(20); // spacer

    // Right: Scale Box (fixed narrower width to match reduced block)
    int desiredScaleWidth = juce::jmin(260, topRow.getWidth());
    auto bScale = topRow.removeFromLeft(desiredScaleWidth);
    scaleLabel.setBounds(bScale.removeFromTop(20));
    scaleBox.setBounds(bScale);
    
    b1.removeFromTop(6); // spacer
    
    // Bottom: Keyboard
    // Keep piano keys proportions realistically (e.g., 160px wide for 1 octave)
    scaleKeyboard.setBounds(b1.removeFromTop(50).withSizeKeepingCentre(160, 50));
}

void OpenVoxTunerAudioProcessorEditor::timerCallback()
{
    refreshVisualizer();

    if (updateCheckState != nullptr)
    {
        if (! updateCheckState->finished.load())
        {
            updateButton.setButtonText ("Checking...");
            updateButton.setEnabled (false);
        }
        else
        {
            updateButton.setEnabled (true);
            if (updateCheckState->updateAvailable.load())
            {
                updateButton.setButtonText ("Update " + updateCheckState->latestVersion);
                updateButton.setColour (juce::TextButton::buttonColourId, kAccent.darker (0.2f));
                updateButton.setTooltip ("Open the latest OpenVoxTuner release.");
            }
            else
            {
                updateButton.setButtonText ("Up to date");
                updateButton.setColour (juce::TextButton::buttonColourId, kBgPanel);
                updateButton.setTooltip (updateCheckState->statusText.isNotEmpty() ? updateCheckState->statusText : "Open the OpenVoxTuner releases page.");
            }
        }
    }

    // Sync tab <-> "mode" parameter
    int tabIndex = tabbedComponent.getCurrentTabIndex();

    // Visibility of Curve Editor Mode specific buttons
    bool isCurveEditorMode = (tabIndex == 1);
    presetsButton.setVisible (isCurveEditorMode);
    snapButton.setVisible (isCurveEditorMode);
    snapGridButton.setVisible (isCurveEditorMode);
    stepModeButton.setVisible (isCurveEditorMode);
    clearCurveButton.setVisible (isCurveEditorMode);
    measuresLabel.setVisible (isCurveEditorMode);
    measuresBox.setVisible (isCurveEditorMode);
    autoScrollToggle.setVisible (isCurveEditorMode);

    // Gray out Formant slider if disabled
    bool isFormantEnabled = formantEnableButton.getToggleState();
    formantSlider.setEnabled (isFormantEnabled);

    // Harmony controls enable/disable
    bool isHarmonyEnabled = harmonyEnableButton.getToggleState();
    harmonyTypeBox.setEnabled (isHarmonyEnabled);
    harmonyGainSlider.setEnabled (isHarmonyEnabled);
    harmonyBlendSlider.setEnabled (isHarmonyEnabled);
    const bool useVoice = useVoiceButton.getToggleState();
    shiftedVoicesBox.setEnabled (isHarmonyEnabled && useVoice);
    harmonyToneBox.setEnabled (isHarmonyEnabled && !useVoice);
    harmonyToneColorSlider.setEnabled (isHarmonyEnabled && !useVoice);

    // Show bypass only in standalone wrapper (DAW has host bypass)
    bypassToggleButton.setVisible (processorRef.isStandaloneWrapper());

    // Show only the relevant selector to keep the UI compact and readable
    shiftedVoicesBox.setVisible (isHarmonyEnabled && useVoice);
    harmonyToneBox.setVisible (isHarmonyEnabled && !useVoice);
    harmonyToneColorSlider.setVisible (isHarmonyEnabled && !useVoice);
    harmonyToneColorLabel.setVisible (isHarmonyEnabled && !useVoice);

    // The Reset Playhead button only makes sense if we are not in ARA mode.
    // It allows to offset the plugin's timeline compared to the DAW in classic VST3.
    resetTransportButton.setVisible (isCurveEditorMode); // Only in Curve Editor mode
    resetTransportButton.setEnabled (!processorRef.isBoundToARA_custom());

    auto* modeParam = processorRef.getParameters().getParameter("mode");
    
    // If user clicked a tab
    if (tabIndex != static_cast<int>(modeParam->getValue())) {
        modeParam->setValueNotifyingHost(tabIndex == 1 ? 1.0f : 0.0f);
    }
    
    // Update edit state and playhead
    if (curveEditor != nullptr) {
        curveEditor->setEditorEnabled(tabIndex == 1);
        curveEditor->setPlayheadTime(processorRef.getTransportTime());
        // Propagate time signature and measures (Feature 1)
        int num = processorRef.getCurrentTimeSigNumerator();
        int den = processorRef.getCurrentTimeSigDenominator();
        curveEditor->setTimeSignature (num, den);
        curveEditor->setMeasuresVisible (measuresBox.getSelectedId());
        // Propagate auto-scroll state (Feature 2)
        curveEditor->setAutoScroll (autoScrollToggle.getToggleState());
    }

    // Update harmony visuals: forward harmony frequencies to the visualizer
    if (pitchVisualizer != nullptr)
    {
        bool harmonyOn = true;
        auto* rawHarmonyEnable = processorRef.getParameters().getRawParameterValue("harmony_enable");
        if (rawHarmonyEnable != nullptr)
            harmonyOn = (rawHarmonyEnable->load() > 0.5f);

        // Show harmony lines only if engine enabled and there's audible harmony output
        const float harmonyLevel = processorRef.getHarmonyOutputLevel();
        juce::Array<float> freqsToSend;
        // Use a slightly higher threshold to avoid traces from near-silent residuals
        if (harmonyOn && harmonyLevel > 0.01f)
        {
            freqsToSend = processorRef.getHarmonyFrequencies();
            pitchVisualizer->setHarmonyFrequencies(freqsToSend);
        }
        else
        {
            // send empty snapshot to advance history (so visual stops)
            pitchVisualizer->setHarmonyFrequencies(juce::Array<float>());
        }

        // Also forward harmony samples to the Curve Editor (for the Curve Editor view)
        if (curveEditor != nullptr)
        {
            curveEditor->addHarmonySamples(processorRef.getTransportTime(), freqsToSend);
        }

        // MIDI status label removed; debug window shows detailed MIDI log.
    }

    // Sync custom parameters if we are not in Custom mode
    auto* rawScale = processorRef.getParameters().getRawParameterValue("scale");
    const int scaleIdx = rawScale ? static_cast<int>(std::round(rawScale->load())) : scaleBox.getSelectedItemIndex();

    if (scaleIdx != 15) {
        auto* rawKey = processorRef.getParameters().getRawParameterValue("key");
        const int keyIdx = rawKey ? static_cast<int>(std::round(rawKey->load())) : keyBox.getSelectedItemIndex();

        atdsp::ScaleQuantizer tempQuantizer;
        tempQuantizer.setKey(keyIdx);
        tempQuantizer.setScale(static_cast<atdsp::Scale>(juce::jlimit(0, 15, scaleIdx)));
        auto intervals = tempQuantizer.getScaleIntervals();
        
        for (int i = 0; i < 12; ++i) {
            auto* p = processorRef.getParameters().getParameter("custom" + juce::String(i));
            if (p != nullptr) {
                float targetVal = intervals.contains(i) ? 1.0f : 0.0f;
                if (p->getValue() != targetVal) {
                    p->setValueNotifyingHost(targetVal);
                }
            }
        }
    }
}

void OpenVoxTunerAudioProcessorEditor::refreshVisualizer()
{
    if (pitchVisualizer == nullptr) return;
    const float hzIn  = processorRef.getCurrentInputPitch();
    const float hzOut = processorRef.getCurrentOutputPitch();
    pitchVisualizer->pushInputPitch  (hzIn);
    pitchVisualizer->pushOutputPitch (hzOut);
    if (curveEditor != nullptr)
        curveEditor->getPianoKeyboard().setCurrentPitches (hzIn, hzOut);

    // Note info for the header display.
    const atdsp::NoteInfo info = atdsp::describePitch (hzIn, hzOut);
    pitchVisualizer->setNoteInfo (info);

    // Update the scale intervals (for the background lines).
    // We read them from the parameter tree -> ScaleQuantizer.
    // Note: the quantizer is rebuilt in syncParameters() every block,
    // so we can just retrieve its current intervals.
    if (curveEditor != nullptr)
    {
        // Get the current scale via key + scale parameters.
        // We access the processor's ScaleQuantizer to get the real intervals
        // (with the key offset + potential custom notes).
        // Note: we don't have a direct accessor, we simulate it with a local ScaleQuantizer.
        // To keep it simple, we read the 12 custom booleans and the key/scale value.
        auto* rawKey = processorRef.getParameters().getRawParameterValue ("key");
        auto* rawScale = processorRef.getParameters().getRawParameterValue ("scale");
        const int keyIdx = rawKey ? static_cast<int> (std::round (rawKey->load())) : keyBox.getSelectedItemIndex();
        const int scaleIdx = rawScale ? static_cast<int> (std::round (rawScale->load())) : scaleBox.getSelectedItemIndex();
        juce::Array<int> intervals;

        if (scaleIdx == 13)
        {
            // Custom: read the booleans.
            for (int i = 0; i < 12; ++i)
                if (scaleKeyboard.getButton(i).getToggleState())
                    intervals.add (i);

            curveEditor->setCustomIntervals (intervals);
            curveEditor->setKeyAndScale (keyIdx, atdsp::Scale::Custom);
        }
        else
        {
            // Presets: reproduce the ScaleQuantizer table.
            static const juce::Array<int> presets[] = {
                { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },  // Chromatic
                { 0, 2, 4, 5, 7, 9, 11 },                  // Major
                { 0, 2, 3, 5, 7, 9, 11 },                  // Melodic Minor
                { 0, 2, 3, 5, 7, 8, 11 },                  // Harmonic Minor
                { 0, 2, 3, 5, 7, 8, 10 },                  // Natural Minor
                { 0, 2, 4, 7, 9 },                         // Major Pentatonic
                { 0, 3, 5, 7, 10 },                        // Minor Pentatonic
                { 0, 3, 5, 6, 7, 10 },                     // Blues
                { 0, 2, 3, 5, 7, 9, 10 },                  // Dorian
                { 0, 1, 3, 5, 7, 8, 10 },                  // Phrygian
                { 0, 2, 4, 6, 7, 9, 11 },                  // Lydian
                { 0, 2, 4, 5, 7, 9, 10 },                  // Mixolydian
                { 0, 1, 3, 5, 6, 8, 10 }                   // Locrian
            };
            const int idx = juce::jlimit (0, 12, scaleIdx);
            for (int i : presets[idx])
                intervals.add ((i + keyIdx) % 12);

            curveEditor->setKeyAndScale (keyIdx, static_cast<atdsp::Scale> (juce::jlimit (0, 13, scaleIdx)));
        }

        pitchVisualizer->setScaleIntervals (intervals);
        curveEditor->setScaleIntervals (intervals);
    }
}

void OpenVoxTunerAudioProcessorEditor::pitchCurveChanged()
{
    // The curve editor notified a change: we copy the curve to
    // the processor (thread-safe via getter).
    if (curveEditor == nullptr) return;
    processorRef.getPitchCurve() = curveEditor->getCurve();
}

void OpenVoxTunerAudioProcessorEditor::applyPresetUiStateFromXml (const juce::XmlElement& xml)
{
    const int keyIdx = xml.getIntAttribute ("key", keyBox.getSelectedItemIndex());
    const int scaleIdx = xml.getIntAttribute ("scale", scaleBox.getSelectedItemIndex());
    const bool snapScale = xml.getBoolAttribute ("snapScale", snapButton.getToggleState());
    const bool snapGrid = xml.getBoolAttribute ("snapGrid", snapGridButton.getToggleState());
    const bool stepMode = xml.getBoolAttribute ("stepMode", stepModeButton.getToggleState());

    keyBox.setSelectedItemIndex (juce::jlimit (0, 11, keyIdx), juce::sendNotificationSync);
    scaleBox.setSelectedItemIndex (juce::jlimit (0, 13, scaleIdx), juce::sendNotificationSync);

    snapButton.setToggleState (snapScale, juce::dontSendNotification);
    snapGridButton.setToggleState (snapGrid, juce::dontSendNotification);
    stepModeButton.setToggleState (stepMode, juce::dontSendNotification);

    if (curveEditor != nullptr)
    {
        curveEditor->setSnapEnabled (snapScale);
        curveEditor->setSnapToGridEnabled (snapGrid);
        curveEditor->setStepModeEnabled (stepMode);
    }
}

void OpenVoxTunerAudioProcessorEditor::loadCustomPresetFromFile (const juce::File& file)
{
    if (curveEditor == nullptr) return;
    if (! file.existsAsFile()) return;

    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument (file).getDocumentElement());
    if (xml == nullptr) return;

    const juce::XmlElement* root = xml.get();
    const juce::XmlElement* curveXml = nullptr;

    if (xml->hasTagName ("OVT_PRESET"))
        curveXml = xml->getChildByName ("PITCH_CURVE");
    else if (xml->hasTagName ("PITCH_CURVE"))
        curveXml = xml.get();

    if (curveXml == nullptr) return;

    atdsp::PitchCurve newCurve;
    newCurve.fromXml (*curveXml);
    curveEditor->setCurve (newCurve);

    if (root->hasTagName ("OVT_PRESET"))
        applyPresetUiStateFromXml (*root);
    else
        stepModeButton.setToggleState (newCurve.isStepMode(), juce::dontSendNotification);
}

void OpenVoxTunerAudioProcessorEditor::promptSaveCustomPreset()
{
    if (curveEditor == nullptr) return;

    auto* w = new juce::AlertWindow ("Save Preset",
                                     "Save the current Curve Editor configuration as a custom preset.",
                                     juce::AlertWindow::NoIcon);
    w->addTextEditor ("name", "", "Name:");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        std::unique_ptr<juce::AlertWindow> cleanup (w);
        if (result == 0 || curveEditor == nullptr)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isEmpty())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                         "Invalid name",
                                                         "Preset name can't be empty.",
                                                         this);
            return;
        }

        const auto dir = getUserPresetsDirectory();
        const auto fileStem = sanitizePresetFileStem (name);
        const auto file = dir.getChildFile (fileStem + ".xml");

        if (file.existsAsFile())
        {
            auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                                   "Overwrite preset?",
                                                                   "A preset with this name already exists.\nOverwrite it?",
                                                                   "Cancel", "Overwrite",
                                                                   this);

            juce::NativeMessageBox::showAsync (opts, [this, file, name] (int result)
            {
                if (result != 2)
                    return;
                writeCustomPresetFile (name, file);
            });
            return;
        }

        writeCustomPresetFile (name, file);
    }), true);
}

void OpenVoxTunerAudioProcessorEditor::writeCustomPresetFile (const juce::String& name, const juce::File& file)
{
    if (curveEditor == nullptr || name.isEmpty() || file == juce::File())
        return;

    auto root = std::make_unique<juce::XmlElement> ("OVT_PRESET");
    root->setAttribute ("name", name);
    root->setAttribute ("key", keyBox.getSelectedItemIndex());
    root->setAttribute ("scale", scaleBox.getSelectedItemIndex());
    root->setAttribute ("snapScale", snapButton.getToggleState() ? 1 : 0);
    root->setAttribute ("snapGrid", snapGridButton.getToggleState() ? 1 : 0);
    root->setAttribute ("stepMode", stepModeButton.getToggleState() ? 1 : 0);

    auto curveXml = curveEditor->getCurve().toXml();
    if (curveXml != nullptr)
        root->addChildElement (curveXml.release());

    const bool ok = file.replaceWithText (root->toString());
    if (ok)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                     "Preset saved",
                                                     "Saved custom preset:\n" + name,
                                                     this);
    }
    else
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     "Save failed",
                                                     "Couldn't write the preset file.",
                                                     this);
    }
}

void OpenVoxTunerAudioProcessorEditor::deleteCustomPresetFile (const juce::File& file)
{
    if (! file.existsAsFile()) return;

    auto opts = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::WarningIcon,
                                                           "Delete preset?",
                                                           "Delete this custom preset permanently?\n" + file.getFileNameWithoutExtension(),
                                                           "Cancel", "Delete",
                                                           this);

    juce::NativeMessageBox::showAsync (opts, [this, file] (int result)
    {
        // Debug: display the result value
        // Debug: show the actual result value
        if (result != 1) // Delete button id is 1 in makeOptionsYesNo
            return;

        // Ensure file is writable before attempting deletion
        file.setReadOnly(false);
        const bool deleted = file.deleteFile();
        if (!deleted || file.existsAsFile())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                             "Delete failed",
                                                             "Couldn't delete the preset file. Check permissions.",
                                                             this);
        }
        else
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                             "Preset deleted",
                                                             "Deleted custom preset:\n" + file.getFileNameWithoutExtension(),
                                                             this);
            // Refresh the presets menu immediately so the entry disappears
            showPresetsMenu();
        }
    });
}

void OpenVoxTunerAudioProcessorEditor::showPresetsMenu (const juce::MouseEvent* mouseEvent)
{
    if (curveEditor == nullptr)
        return;

    juce::PopupMenu factory;
    struct Action
    {
        enum class Type { LoadFactory, LoadCustom, SaveCustom, DeleteCustom };
        Type type {};
        juce::String name;
        juce::File file;
    };

    juce::Array<Action> actions;

    auto addAction = [&] (juce::PopupMenu& menu, Action a, const juce::String& label, bool enabled)
    {
        actions.add (std::move (a));
        menu.addItem (actions.size(), label, enabled);
    };

    addAction (factory, { Action::Type::LoadFactory, "default", {} }, "Default", true);
    factory.addSeparator();
    addAction (factory, { Action::Type::LoadFactory, "robot_c3", {} }, "Robot (C3)", true);
    addAction (factory, { Action::Type::LoadFactory, "robot_c4", {} }, "Robot (C4)", true);
    factory.addSeparator();
    addAction (factory, { Action::Type::LoadFactory, "spoken_male", {} }, "Spoken Voice (Male)", true);
    addAction (factory, { Action::Type::LoadFactory, "spoken_female", {} }, "Spoken Voice (Female)", true);
    factory.addSeparator();
    addAction (factory, { Action::Type::LoadFactory, "bass", {} }, "Bass", true);
    addAction (factory, { Action::Type::LoadFactory, "baritone", {} }, "Baritone", true);
    addAction (factory, { Action::Type::LoadFactory, "tenor", {} }, "Tenor", true);
    addAction (factory, { Action::Type::LoadFactory, "alto", {} }, "Alto", true);
    addAction (factory, { Action::Type::LoadFactory, "mezzo", {} }, "Mezzo", true);
    addAction (factory, { Action::Type::LoadFactory, "soprano", {} }, "Soprano", true);

    juce::PopupMenu custom;
    const auto dir = getUserPresetsDirectory();
    const auto files = dir.findChildFiles (juce::File::findFiles, false, "*.xml");
    for (const auto& f : files)
        addAction (custom, { Action::Type::LoadCustom, {}, f }, f.getFileNameWithoutExtension(), true);

    custom.addSeparator();
    addAction (custom, { Action::Type::SaveCustom, {}, {} }, "Save Preset As...", true);

    juce::PopupMenu deleteMenu;
    for (const auto& f : files)
        addAction (deleteMenu, { Action::Type::DeleteCustom, {}, f }, f.getFileNameWithoutExtension(), true);

    custom.addSubMenu ("Delete...", deleteMenu, ! files.isEmpty());

    juce::PopupMenu menu;
    menu.addSubMenu ("Factory", factory);
    menu.addSubMenu ("Custom", custom);

    auto opts = juce::PopupMenu::Options();
    if (mouseEvent != nullptr)
    {
        opts = opts.withTargetComponent (mouseEvent->eventComponent)
                   .withTargetScreenArea (juce::Rectangle<int> (mouseEvent->getScreenX(), mouseEvent->getScreenY(), 1, 1));
    }
    else
    {
        opts = opts.withTargetComponent (&presetsButton)
                   .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards);
    }

    menu.showMenuAsync (opts, [this, actions] (int result) mutable
    {
        if (result <= 0 || curveEditor == nullptr)
            return;

        if (result > actions.size())
            return;

        const auto& a = actions.getReference (result - 1);
        if (a.type == Action::Type::LoadFactory)
        {
            atdsp::PitchCurve newCurve;
            newCurve.loadPreset (a.name);
            curveEditor->setCurve (newCurve);
        }
        else if (a.type == Action::Type::LoadCustom)
        {
            loadCustomPresetFromFile (a.file);
        }
        else if (a.type == Action::Type::SaveCustom)
        {
            promptSaveCustomPreset();
        }
        else if (a.type == Action::Type::DeleteCustom)
        {
            deleteCustomPresetFile (a.file);
        }
    });
}





