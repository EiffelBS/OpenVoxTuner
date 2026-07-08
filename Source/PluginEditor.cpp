// PluginEditor.cpp
// Implementation of the OpenVoxTuner plugin GUI editor.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/OVTFonts.h"
#include "ui/OVTTheme.h"
#include "ui/OVTLanguages.h"

// === Theme colors ("autotune" style: dark + pink/purple accent) ===

// Helper: ensure a PopupMenu uses our custom LookAndFeel for correct background colours
static void applyMenuLookAndFeel (juce::PopupMenu& m, ui::AutotuneLookAndFeel& lf)
{
    m.setLookAndFeel (&lf);
}

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
    const int boxW = 440;
    const int boxH = 380;
    const int boxX = centerX - boxW / 2;
    const int boxY = centerY - boxH / 2;

    // Panel background
    g.setColour (ovt::bgPanel());
    g.fillRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 8.0f);
    g.setColour (ovt::accentSoft());
    g.drawRoundedRectangle ((float) boxX, (float) boxY, (float) boxW, (float) boxH, 8.0f, 1.5f);

    // Title
    g.setColour (juce::Colours::white);
    g.setFont (ovt::fontLabel());
    g.drawText ("Keyboard Shortcuts & Mouse Controls", boxX + 16, boxY + 12, boxW - 32, 24,
                juce::Justification::centred);

    // Divider
    g.setColour (ovt::accentSoft());
    g.fillRect ((float) (boxX + 16), (float) (boxY + 40), (float) (boxW - 32), 1.0f);

    // Shortcuts list
    struct Shortcut { const char* key; const char* desc; };
    const Shortcut shortcuts[] = {
        { "Mouse Wheel",           "Scroll vertically (pitch)" },
        { "Ctrl / Cmd + Wheel",    "Zoom in/out" },
        { "Click + Drag",          "Move curve points" },
        { "Double-click",          "Add new curve point" },
        { "Right-click / Alt+Click","Delete curve point" },
        { "Ctrl / Cmd + C",        "Copy selected points" },
        { "Ctrl / Cmd + V",        "Paste copied points" },
        { "Delete / Backspace",    "Delete selected points" },
        { "Ctrl / Cmd + Z",        "Undo" },
        { "Ctrl / Cmd + Y",        "Redo" },
        { "Ctrl / Cmd + Shift + Z","Redo" },
        { "?",                     "Toggle this help overlay" },
    };

    g.setFont (ovt::fontLegend());
    int sy = boxY + 52;
    const int colW = boxW / 2 - 16;
    for (int i = 0; i < 12; ++i)
    {
        const int col = i / 6;
        const int row = i % 6;
        const int x = boxX + 24 + col * colW;
        const int y = sy + row * 22;

        g.setColour (ovt::accent());
        g.drawText (shortcuts[i].key, x, y, colW / 2 - 8, 18, juce::Justification::centredLeft);
        g.setColour (ovt::text());
        g.drawText (shortcuts[i].desc, x + colW / 2, y, colW / 2 - 8, 18, juce::Justification::centredLeft);
    }

    // Close hint
    g.setColour (ovt::textDim());
    g.setFont (ovt::fontLegendHint());
    g.drawText ("Click anywhere to close", boxX, boxY + boxH - 24, boxW, 20,
                juce::Justification::centred);
}

OpenVoxTunerAudioProcessorEditor::OpenVoxTunerAudioProcessorEditor (OpenVoxTunerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&customLookAndFeel);

    // Restore theme preference from plugin state
    auto* themeParam = processorRef.getParameters().getParameter ("ui_theme");
    if (themeParam != nullptr)
        ovt::currentTheme() = (themeParam->getValue() > 0.5f) ? ovt::Theme::Light : ovt::Theme::Dark;

    applyThemeToAllComponents();

    // Restore language preference from plugin state
    auto* langParam = processorRef.getParameters().getParameter ("ui_language");
    if (langParam != nullptr)
    {
        const int langIdx = juce::roundToInt (langParam->getValue() * 4.0f);
        switch (langIdx)
        {
            case 0: ovt::currentLanguage() = ovt::Language::English;  break;
            case 1: ovt::currentLanguage() = ovt::Language::French;   break;
            case 2: ovt::currentLanguage() = ovt::Language::German;   break;
            case 3: ovt::currentLanguage() = ovt::Language::Spanish;  break;
            case 4: ovt::currentLanguage() = ovt::Language::Japanese; break;
            default: ovt::currentLanguage() = ovt::Language::English; break;
        }
    }

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
    updateButton.setColour (juce::TextButton::buttonColourId, ovt::bgPanel());
    updateButton.setColour (juce::TextButton::textColourOffId, ovt::text());
    updateButton.setColour (juce::TextButton::textColourOnId, ovt::accent());
    addAndMakeVisible (updateButton);

    startUpdateCheck();

    // === Configuration of the 4 sliders (rotary knobs) ===
    auto setupKnob = [this] (juce::Slider& s, juce::Label* l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
        s.setColour (juce::Slider::rotarySliderFillColourId,    ovt::accent());
        s.setColour (juce::Slider::rotarySliderOutlineColourId, ovt::accentSoft());
        s.setColour (juce::Slider::thumbColourId,               juce::Colours::white);
        s.setColour (juce::Slider::textBoxTextColourId,         ovt::text());
        s.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        if (l != nullptr)
        {
            l->setText (name, juce::dontSendNotification);
            l->setJustificationType (juce::Justification::centred);
            l->setColour (juce::Label::textColourId, ovt::text());
            l->setFont (ovt::fontLabel());
            addAndMakeVisible (*l);
        }
    };

    setupKnob (speedSlider,  &speedLabel,  "Speed (ms)");
    translatableLabels.push_back ({ &speedLabel, ovt::Keys::kLabelSpeed });
    setupKnob (amountSlider, &amountLabel, "Amount");
    translatableLabels.push_back ({ &amountLabel, ovt::Keys::kLabelAmount });
    setupKnob (formantSlider, nullptr, "");

    // === Harmony UI ===
    // Harmony enable toggle (use same visual style as Formant)
    harmonyEnableButton.setButtonText ("Harmony");
    harmonyEnableButton.setName ("PowerButton");
    harmonyEnableButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    harmonyEnableButton.setColour (juce::ToggleButton::tickColourId, ovt::accent());
    harmonyEnableButton.setTooltip ("Enable/disable harmony generation.");
    addAndMakeVisible (harmonyEnableButton);

    // Harmony type combo — index 3 = "3rd Below + Above" (default)
    harmonyTypeBox.addItemList (juce::StringArray {
        "None",
        "3rd Below", "3rd Above", "3rd Below + Above",
        "4th Below", "4th Above", "4th Below + Above",
        "5th Below", "5th Above", "5th Below + Above",
        "3rd Below + 5th Above", "5th Below + 3rd Above",
        "Octave Below", "Octave Above", "Octave Below + Above",
        "Vocal Stack (3 voices)", "Vocal Stack (4 voices)",
        "Power Chord", "Parallel 3rd", "Drone"
    }, 1);
    harmonyTypeBox.setSelectedItemIndex (3, juce::dontSendNotification);
    harmonyTypeBox.setColour (juce::ComboBox::backgroundColourId, ovt::bgPanel());
    harmonyTypeBox.setColour (juce::ComboBox::textColourId, ovt::text());
    harmonyTypeBox.setColour (juce::ComboBox::outlineColourId, ovt::accentSoft());
    addAndMakeVisible (harmonyTypeBox);

    // Harmony knobs (Volume, Blend) — use same rotary knob style as main knobs
    setupKnob (harmonyGainSlider, &harmonyGainLabel, "Volume");
    translatableLabels.push_back ({ &harmonyGainLabel, ovt::Keys::kLabelVolume });
    harmonyGainSlider.setRange (0.0, 1.0, 0.01);
    harmonyGainSlider.setValue (1.0);

    setupKnob (harmonyBlendSlider, &harmonyBlendLabel, "Blend");
    translatableLabels.push_back ({ &harmonyBlendLabel, ovt::Keys::kLabelBlend });
    harmonyBlendSlider.setRange (0.0, 1.0, 0.01);
    harmonyBlendSlider.setValue (0.5);

    // Use Voice controls
    useVoiceButton.setButtonText ("Use Voice");
    useVoiceButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    useVoiceButton.setColour (juce::ToggleButton::tickColourId, ovt::accent());
    addAndMakeVisible (useVoiceButton);

    shiftedVoicesBox.addItemList ({ "1", "2", "3", "4" }, 1);
    shiftedVoicesBox.setSelectedId (4, juce::dontSendNotification);
    shiftedVoicesBox.setColour (juce::ComboBox::backgroundColourId, ovt::bgPanel());
    shiftedVoicesBox.setColour (juce::ComboBox::textColourId, ovt::text());
    shiftedVoicesBox.setColour (juce::ComboBox::outlineColourId, ovt::accentSoft());
    addAndMakeVisible (shiftedVoicesBox);

    harmonyToneBox.addItemList ({ "Choir", "Bright", "Synth Lead", "Strings", "Guitar", "Vocoder-like" }, 1);
    harmonyToneBox.setSelectedItemIndex (0, juce::dontSendNotification);
    harmonyToneBox.setColour (juce::ComboBox::backgroundColourId, ovt::bgPanel());
    harmonyToneBox.setColour (juce::ComboBox::textColourId, ovt::text());
    harmonyToneBox.setColour (juce::ComboBox::outlineColourId, ovt::accentSoft());
    addAndMakeVisible (harmonyToneBox);

    harmonyToneColorLabel.setText ("Tone", juce::dontSendNotification);
    harmonyToneColorLabel.setJustificationType (juce::Justification::centred);
    harmonyToneColorLabel.setColour (juce::Label::textColourId, ovt::text());
    harmonyToneColorLabel.setFont (ovt::fontLabelSmall());
    addAndMakeVisible (harmonyToneColorLabel);

    harmonyToneColorSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    harmonyToneColorSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    harmonyToneColorSlider.setRange (0.0, 1.0, 0.01);
    harmonyToneColorSlider.setValue (0.5, juce::dontSendNotification);
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderFillColourId, ovt::accent());
    harmonyToneColorSlider.setColour (juce::Slider::rotarySliderOutlineColourId, ovt::accentSoft());
    harmonyToneColorSlider.setColour (juce::Slider::thumbColourId, juce::Colours::white);
    harmonyToneColorSlider.setTooltip ("Tone color for synth harmonies.");
    addAndMakeVisible (harmonyToneColorSlider);

    // Key and Scale are discrete values -> ComboBox.
    auto setupCombo = [this] (juce::ComboBox& b, juce::Label& l, const juce::String& name,
                              const juce::StringArray& items, int initialIndex)
    {
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, ovt::text());
        l.setFont (ovt::fontLabel());
        addAndMakeVisible (l);

        for (int i = 0; i < items.size(); ++i)
            b.addItem (items[i], i + 1);
        b.setSelectedItemIndex (initialIndex, juce::dontSendNotification);
        b.setColour (juce::ComboBox::backgroundColourId, ovt::bgPanel());
        b.setColour (juce::ComboBox::textColourId,       ovt::text());
        b.setColour (juce::ComboBox::outlineColourId,    ovt::accentSoft());
        b.setColour (juce::ComboBox::arrowColourId,      ovt::accent());
        addAndMakeVisible (b);
    };

    setupCombo (keyBox,   keyLabel,   "Root", kNoteNames,  0);
    translatableLabels.push_back ({ &keyLabel, ovt::Keys::kLabelRoot });
    setupCombo (scaleBox, scaleLabel, "Scale", kScaleNames, 0);
    translatableLabels.push_back ({ &scaleLabel, ovt::Keys::kLabelScale });

    latencyModeLabel.setText ("", juce::dontSendNotification);
    latencyModeLabel.setJustificationType (juce::Justification::centred);
    latencyModeLabel.setColour (juce::Label::textColourId, ovt::text());
    latencyModeLabel.setFont (ovt::fontVersion());
    latencyModeLabel.setVisible (false);

    latencyModeBox.addItemList ({ "Low Latency", "Quality", "Safe" }, 1);
    latencyModeBox.setSelectedItemIndex (1, juce::dontSendNotification);
    latencyModeBox.setColour (juce::ComboBox::backgroundColourId, ovt::bgPanel());
    latencyModeBox.setColour (juce::ComboBox::textColourId, ovt::text());
    latencyModeBox.setColour (juce::ComboBox::outlineColourId, ovt::accentSoft());
    latencyModeBox.setColour (juce::ComboBox::arrowColourId, ovt::accent());
    addAndMakeVisible (latencyModeBox);

    // === Hamburger menu button (gear icon) ===
    menuButton.onClick = [this]
    {
        juce::PopupMenu menu;

        // 1. Latency submenu
        juce::PopupMenu latencyMenu;
        latencyMenu.addItem ("Low Latency", true, latencyModeBox.getSelectedId() == 1, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (0.0f);
        });
        latencyMenu.addItem ("Quality", true, latencyModeBox.getSelectedId() == 2, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (1.0f / 2.0f);
        });
        latencyMenu.addItem ("Safe", true, latencyModeBox.getSelectedId() == 3, [this] {
            if (auto* p = processorRef.getParameters().getParameter ("latency_mode"))
                p->setValueNotifyingHost (2.0f / 2.0f);
        });
        menu.addSubMenu (ovt::tr(ovt::Keys::kMenuLatency), latencyMenu);

        menu.addSeparator();

        // 2. MIDI OUT toggle
        {
            bool midiOn = processorRef.getParameters().getParameter ("midi_out_enable")->getValue() > 0.5f;
            menu.addItem (ovt::tr(ovt::Keys::kMenuMidiOut), true, midiOn, [this] {
                if (auto* p = processorRef.getParameters().getParameter ("midi_out_enable"))
                    p->setValueNotifyingHost (1.0f - p->getValue());
            });
        }

        // 3. Tuning Type submenu (Modern / Transparent)
        {
            juce::PopupMenu tuningMenu;
            auto* modeParam = processorRef.getParameters().getParameter ("correction_mode");
            const bool isTransparent = (modeParam != nullptr) ? (modeParam->getValue() > 0.5f) : false;
            tuningMenu.addItem (ovt::tr(ovt::Keys::kMenuModern),      true, !isTransparent, [modeParam] {
                if (modeParam != nullptr) modeParam->setValueNotifyingHost (0.0f);
            });
            tuningMenu.addItem (ovt::tr(ovt::Keys::kMenuTransparent), true,  isTransparent, [modeParam] {
                if (modeParam != nullptr) modeParam->setValueNotifyingHost (1.0f);
            });
            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuTuningType), tuningMenu);
        }

        menu.addSeparator();

        // 4. Pitch Detection submenu (YIN only)
        {
            juce::PopupMenu pitchMenu;
            pitchMenu.addItem ("YIN (active)", false, true, nullptr);
            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuPitchDetection), pitchMenu);
        }

        menu.addSeparator();

        // 5. Theme toggle
        {
            juce::PopupMenu themeMenu;
            const bool isDark = ovt::isDark();
            themeMenu.addItem (ovt::tr(ovt::Keys::kMenuDarkTheme), true, isDark, [this] {
                ovt::currentTheme() = ovt::Theme::Dark;
                if (auto* p = processorRef.getParameters().getParameter ("ui_theme"))
                    p->setValueNotifyingHost (0.0f);
                applyThemeToAllComponents();
            });
            themeMenu.addItem (ovt::tr(ovt::Keys::kMenuLightTheme), true, !isDark, [this] {
                ovt::currentTheme() = ovt::Theme::Light;
                if (auto* p = processorRef.getParameters().getParameter ("ui_theme"))
                    p->setValueNotifyingHost (1.0f);
                applyThemeToAllComponents();
            });
            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuTheme), themeMenu);
        }

        menu.addSeparator();

        // 6. Language selector
        {
            juce::PopupMenu langMenu;
            const auto currentLang = ovt::currentLanguage();
            auto addLangItem = [&](const char* label, ovt::Language lang, int langIdx)
            {
                langMenu.addItem (label, true, (currentLang == lang), [this, lang, langIdx] {
                    ovt::currentLanguage() = lang;
                    if (auto* p = processorRef.getParameters().getParameter ("ui_language"))
                        p->setValueNotifyingHost ((float) langIdx / 4.0f);
                    repaint();
                    refreshLabels();
                });
            };
            addLangItem ("English",  ovt::Language::English,  0);
            addLangItem ("Francais", ovt::Language::French,   1);
            addLangItem ("Deutsch",  ovt::Language::German,   2);
            addLangItem ("Espanol",  ovt::Language::Spanish,  3);
            addLangItem ("Nihongo",  ovt::Language::Japanese, 4);
            menu.addSubMenu (ovt::tr(ovt::Keys::kMenuLanguage), langMenu);
        }

        menu.addSeparator();

        // Waveform overlay toggle
        menu.addItem ("Show Waveform", true, showWaveform, [this] {
            showWaveform = ! showWaveform;
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
            waveformMenu.addItem ("Line", true, currentType == 0, [this] { setWaveformDisplayType (0); });
            waveformMenu.addItem ("Mirror", true, currentType == 1, [this] { setWaveformDisplayType (1); });
            menu.addSubMenu ("Waveform Display", waveformMenu, true);
        }

        menu.addSeparator();

        // 7. Export visualizer as image
        menu.addItem (ovt::tr(ovt::Keys::kMenuExportImage), [this] {
            // Find the PitchVisualizer from the Live tab
            auto* vizContent = tabbedComponent.getTabContentComponent (0);
            auto* pitchViz = dynamic_cast<ui::PitchVisualizer*> (vizContent);
            if (pitchViz == nullptr)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                    "Export", "Could not find the visualizer component.");
                return;
            }

            // Use a lambda to keep the FileChooser alive via shared_ptr
            // Default to Downloads folder, PNG only
            auto defaultDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
            auto downloadsDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                    .getChildFile ("Downloads");
            if (downloadsDir.isDirectory())
                defaultDir = downloadsDir;

            auto chooserPtr = std::make_shared<juce::FileChooser>(
                "Export Visualizer as PNG",
                defaultDir,
                "*.png");
            chooserPtr->launchAsync (
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [chooserPtr, pitchViz] (const juce::FileChooser& fc) {
                    auto file = fc.getResult();
                    if (file != juce::File{})
                    {
                        // Ensure .png extension
                        if (file.getFileExtension() != ".png")
                            file = file.withFileExtension (".png");

                        if (pitchViz->exportAsImage (file))
                            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                "Export", "Image saved to:\n" + file.getFullPathName());
                        else
                            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                "Export", "Failed to save image.");
                    }
                });
        });

        menu.addSeparator();

        // 7b. MIDI Learn submenu
        {
            juce::PopupMenu midiLearnMenu;
            struct ParamEntry { const char* id; const char* name; };
            const ParamEntry params[] = {
                {"speed", "Speed"}, {"amount", "Amount"}, {"formant", "Formant"},
                {"reverb_mix", "Reverb Mix"}, {"flex_tune", "FlexTune"},
                {"humanize", "Humanize"}, {"harmony_gain", "Harmony Gain"},
                {"harmony_blend", "Harmony Blend"}, {"harmony_tone_color", "Harmony Tone"}
            };
            for (const auto& p : params)
            {
                midiLearnMenu.addItem (juce::String (p.name), [this, id = juce::String (p.id)] {
                    startMidiLearn (id);
                });
            }
            menu.addSubMenu ("MIDI Learn", midiLearnMenu);
        }

        menu.addSeparator();

        // Help overlay
        menu.addItem ("Keyboard Shortcuts (?)", [this] { toggleHelpOverlay(); });

        menu.addSeparator();

        // 8. Check for Updates
        menu.addItem (ovt::tr(ovt::Keys::kMenuCheckUpdates), [this] {
            updateButton.onClick();
        });

        menu.addSeparator();

        // 6. Reset to Default — restore all parameters to their factory defaults
        menu.addItem (ovt::tr(ovt::Keys::kMenuResetDefault), [this] {
            juce::PopupMenu confirmMenu;
            confirmMenu.addItem ("Cancel", []{});
            confirmMenu.addSeparator();
            confirmMenu.addItem ("Confirm Reset", [this] {
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

        menu.addSeparator();

        // 6. Bypass (standalone only)
        if (processorRef.isStandaloneWrapper())
        {
            bool bypassOn = processorRef.getParameters().getParameter ("bypass")->getValue() > 0.5f;
            menu.addItem (ovt::tr(ovt::Keys::kMenuBypass), true, bypassOn, [this] {
                if (auto* p = processorRef.getParameters().getParameter ("bypass"))
                    p->setValueNotifyingHost (1.0f - p->getValue());
            });
        }

       #if JUCE_DEBUG
        menu.addItem ("Debug Window", [this] { debugWindowButton.onClick(); });
       #endif


        applyMenuLookAndFeel (menu, customLookAndFeel);
        menu.showMenuAsync (juce::PopupMenu::Options()
            .withTargetComponent (&menuButton)
            .withPreferredPopupDirection (juce::PopupMenu::Options::PopupDirection::downwards));
    };
    addAndMakeVisible (menuButton);

    // === A/B Comparison button ===
    abButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x331A9AF0));
    abButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffcccccc));
    abButton.setTooltip ("A/B: Click to toggle between slot A and B. Right-click to save current state.");
    abButton.onClick = [this] { toggleAB(); };
    abButton.onRightClick = [this] {
        juce::PopupMenu menu;
        menu.addItem ("Save to Slot A", [this] { saveSlot (slotA); });
        menu.addItem ("Save to Slot B", [this] { saveSlot (slotB); });

        applyMenuLookAndFeel (menu, customLookAndFeel);
        menu.showMenuAsync (juce::PopupMenu::Options());
    };
    addAndMakeVisible (abButton);

    // === Slider ranges ===
    speedSlider.setRange (0.0, 200.0, 1.0);
    speedSlider.setValue (50.0);
    amountSlider.setRange (0.0, 1.0, 0.01);
    amountSlider.setValue (1.0);

    // === Manual bindings for ComboBox Key/Scale to AudioParameterInt ===
    // Using ComboBoxAttachment for perfect sync with the host
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "key", keyBox);
    // When the user selects a new key, update the piano keys for the current preset scale.
    keyBox.onChange = [this] {
        const int scaleIdx = scaleBox.getSelectedItemIndex();
        if (scaleIdx >= 0 && scaleIdx != 13)
            scaleBox.onChange(); // Re-run the scale onChange to recompute intervals with the new key
    };
        
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "scale", scaleBox);
    // When the user selects a preset scale, update the piano keys immediately.
    scaleBox.onChange = [this] {
        const int idx = scaleBox.getSelectedItemIndex();
        if (idx >= 0 && idx != 13) // Not Custom
        {
            auto* rawKey = processorRef.getParameters().getRawParameterValue ("key");
            const int keyIdx = rawKey ? static_cast<int> (std::round (rawKey->load())) : 0;

            atdsp::ScaleQuantizer tempQuantizer;
            tempQuantizer.setKey (keyIdx);
            tempQuantizer.setScale (static_cast<atdsp::Scale> (juce::jlimit (0, 13, idx)));
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
        }
    };
    latencyModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "latency_mode", latencyModeBox);
    detectorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.getParameters(), "pitch_detector", detectorBox);

    // Reverb attachments
    reverbEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "reverb_enable", reverbEnableButton);
    reverbMixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "reverb_mix", reverbMixSlider);

    // FlexTune / Humanize / Correction Mode attachments
    flexTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "flex_tune", flexTuneSlider);
    humanizeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.getParameters(), "humanize", humanizeSlider);
    correctionModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processorRef.getParameters(), "correction_mode", correctionModeButton);

    // UI updates (visibility of custom buttons, etc.) are handled in timerCallback.

    // Helper for creating Drawables from full SVG XML (with viewBox).
    // Uses a placeholder color (#010101) that gets replaced by the desired state color.
    auto createDrawableSVG = [](const juce::String& svgXml, juce::Colour strokeColor) -> std::unique_ptr<juce::Drawable> {
        auto baseXml = juce::XmlDocument::parse(svgXml);
        if (baseXml == nullptr) return std::make_unique<juce::DrawablePath>();
        auto d = juce::Drawable::createFromSVG(*baseXml);
        if (d == nullptr) return std::make_unique<juce::DrawablePath>();
        // Replace placeholder color with the desired state color
        d->replaceColour(juce::Colour(0xff010101), strokeColor);
        return d;
    };

    auto setupIconButton = [&](juce::DrawableButton& btn, const juce::String& svgXml, bool isToggle, const juce::String& tooltip) {
        auto normal   = createDrawableSVG(svgXml, juce::Colours::grey);
        auto over     = createDrawableSVG(svgXml, juce::Colours::lightgrey);
        auto down     = createDrawableSVG(svgXml, juce::Colours::white);

        if (isToggle) {
            auto normalOn = createDrawableSVG(svgXml, ovt::accent());
            auto overOn   = createDrawableSVG(svgXml, ovt::accent().brighter(0.2f));
            auto downOn   = createDrawableSVG(svgXml, juce::Colours::white);
            btn.setImages(normal.get(), over.get(), down.get(), nullptr,
                          normalOn.get(), overOn.get(), downOn.get(), nullptr);
            btn.setClickingTogglesState(true);
        } else {
            btn.setImages(normal.get(), over.get(), down.get());
        }

        btn.setTooltip(tooltip);
        btn.setColour(juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        btn.setColour(juce::DrawableButton::backgroundOnColourId, ovt::accent().withAlpha(0.2f));
        btn.setColour(juce::DrawableButton::textColourId, juce::Colours::white);
        btn.setColour(juce::DrawableButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible(btn);
    };

    // === SVG icons as full XML strings (Lucide-style, 24x24 viewBox) ===
    // Each uses stroke="#010101" as a placeholder for per-state coloring.

    static const char* svgPresets = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20h16a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.5l-2-3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2z"/></svg>)";

    static const char* svgScale = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="7" cy="18" r="3"/><path d="M10 18V4l11-2v15"/></svg>)";

    static const char* svgGrid = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/></svg>)";

    static const char* svgStep = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 20v-5h6v-6h6V4h4"/></svg>)";

    static const char* svgClear = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 6h18"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/><line x1="10" y1="11" x2="10" y2="17"/><line x1="14" y1="11" x2="14" y2="17"/></svg>)";

    static const char* svgReset = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><polyline points="1 4 1 10 7 10"/><path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"/></svg>)";

    static const char* svgPower = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2v10"/><path d="M18.36 6.64a9 9 0 1 1-12.73 0"/></svg>)";

    static const char* svgGear = R"(<svg viewBox="0 0 24 24" fill="none" stroke="#010101" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z"/></svg>)";

    // Setup Toolbar Buttons
    // Custom button: icon + text (PresetsButton uses setIcon path).
{
    auto normal   = createDrawableSVG(svgPresets, juce::Colours::grey);
    auto over     = createDrawableSVG(svgPresets, juce::Colours::lightgrey);
    auto down     = createDrawableSVG(svgPresets, juce::Colours::white);

    presetsButton.setIcon(std::move(normal));

    presetsButton.setSize(80, 22);
    addAndMakeVisible(presetsButton);

    presetsButton.onClick = [this] { showPresetsMenu(); };
    presetsButton.setTooltip (ovt::tr(ovt::Keys::kTooltipPresets));
}

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

    setupIconButton(clearCurveButton, svgClear, false, "Clear all points");
    clearCurveButton.onClick = [this] {
        if (curveEditor != nullptr) curveEditor->clearCurve();
        processorRef.resetTransportTime();
    };
    clearCurveButton.setTooltip(ovt::tr(ovt::Keys::kTooltipClearAll));

    setupIconButton(resetTransportButton, svgReset, false, "Reset playhead");
    resetTransportButton.onClick = [this] {
        processorRef.resetTransportTime();
    };
    resetTransportButton.setTooltip ("Reset playhead.\n"
                                     "Resets the internal timeline offset (useful in Standalone / classic VST3).");
    setupIconButton(bypassButton, svgPower, true, "Bypass audio processing");
    bypassButton.setTooltip ("Bypass audio processing.\nWhen enabled, audio passes through without correction.");
    addAndMakeVisible (bypassButton);

    // MIDI Out icon (clicking toggles the attached toggle button)
    setupIconButton(midiOutButton, svgPower, true, "Enable MIDI Out");
    addAndMakeVisible (midiOutButton);

    // Menu button (gear icon for options)
    setupIconButton (menuButton, svgGear, false, "OpenVoxTuner options");

    // Toggle buttons with text (attached to parameters)
    bypassToggleButton.setButtonText ("ByPass");
    bypassToggleButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    bypassToggleButton.setColour (juce::ToggleButton::tickColourId, ovt::accent());
    bypassToggleButton.setTooltip ("Bypass audio processing.");
    addAndMakeVisible (bypassToggleButton);

    midiToggleButton.setButtonText ("MIDI OUT");
    midiToggleButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    midiToggleButton.setColour (juce::ToggleButton::tickColourId, ovt::accent());
    midiToggleButton.setTooltip ("Enable MIDI Out");
    addAndMakeVisible (midiToggleButton);

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
    formantEnableButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    formantEnableButton.setColour (juce::ToggleButton::tickColourId, ovt::accent());
    addAndMakeVisible (formantEnableButton);

    // Reverb controls (post-processing effect, same style as Formant)
    setupKnob (reverbMixSlider, &reverbMixLabel, "Mix");
    reverbMixSlider.setRange (0.0, 1.0, 0.01);
    reverbMixSlider.setEnabled (false); // disabled until reverb is toggled on

    reverbEnableButton.setButtonText ("Reverb");
    reverbEnableButton.setName ("PowerButton");
    reverbEnableButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    reverbEnableButton.setColour (juce::ToggleButton::tickColourId, ovt::accent());
    reverbEnableButton.setTooltip ("Enable/disable reverb effect.");
    addAndMakeVisible (reverbEnableButton);

    // FlexTune / Humanize knobs
    setupKnob (flexTuneSlider, &flexTuneLabel, "FlexTune");
    translatableLabels.push_back ({ &flexTuneLabel, ovt::Keys::kLabelFlex });
    flexTuneSlider.setRange (0.0, 100.0, 1.0);
    flexTuneSlider.setTooltip ("Deadband in cents: input pitch within this range of the target is left uncorrected.");

    setupKnob (humanizeSlider, &humanizeLabel, "Humanize");
    translatableLabels.push_back ({ &humanizeLabel, ovt::Keys::kLabelHumanize });
    humanizeSlider.setRange (0.0, 50.0, 1.0);
    humanizeSlider.setTooltip ("Random pitch fluctuations in cents, added when correction is applied.");

    // FlexTune and Humanize: no textbox, smaller inline labels
    flexTuneLabel.setText ("Flex", juce::dontSendNotification);
    flexTuneSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    humanizeLabel.setText ("Humanize", juce::dontSendNotification);
    humanizeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);

    // Correction Mode toggle button
    correctionModeButton.setButtonText ("Modern");
    correctionModeButton.setClickingTogglesState (true);
    correctionModeButton.setColour (juce::TextButton::buttonColourId, ovt::accentSoft());
    correctionModeButton.setColour (juce::TextButton::buttonOnColourId, ovt::accent());
    correctionModeButton.setColour (juce::TextButton::textColourOffId, ovt::text());
    correctionModeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    correctionModeButton.setTooltip ("Modern = aggressive correction. Transparent = gentler, preserves transitions.");
    correctionModeButton.onClick = [this] {
        bool isTransparent = correctionModeButton.getToggleState();
        correctionModeButton.setButtonText (isTransparent ? "Transparent" : "Modern");
    };
    addAndMakeVisible (correctionModeButton);

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
            // Switch to Custom mode silently: write the atomic value and
            // update the combo box display without triggering any JUCE
            // notification cascade. The ButtonAttachment's setValueNotifyingHost
            // on custom_i already happened during setToggleState, and that
            // host re-sync read the old scale value (harmless). Now we just
            // store the new scale value for the next audio callback.
            auto* rawScale = processorRef.getParameters().getRawParameterValue ("scale");
            if (rawScale != nullptr && std::abs (rawScale->load() - 1.0f) > 0.01f)
            {
                rawScale->store (1.0f);
                scaleBox.setSelectedItemIndex (scaleBox.getNumItems() - 1, juce::dontSendNotification);
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
      // Tab content areas always use dark background regardless of theme
      const juce::Colour tabContentColour = juce::Colour (0xff14151c);
      tabbedComponent.addTab("Live", tabContentColour, pitchVisualizer.get(), false);
      tabbedComponent.addTab("Curve Editor", tabContentColour, curveEditor.get(), false);
    addAndMakeVisible(tabbedComponent);
    tabbedComponent.setColour (juce::TabbedComponent::backgroundColourId, ovt::bgDark());
    tabbedComponent.setColour (juce::TabbedComponent::outlineColourId, ovt::accentSoft());

    // Make sure tools are drawn over the tabbed component
    presetsButton.toFront(false);
    snapButton.toFront(false);
    snapGridButton.toFront(false);
    stepModeButton.toFront(false);
    clearCurveButton.toFront(false);
    resetTransportButton.toFront(false);

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
    // Main background (flat, no gradient)
    g.setColour (ovt::bgDark());
    g.fillAll();

    // Draw the bottom blocks backgrounds (left = scale, middle = knobs, right = harmony)
    g.setColour (ovt::bgPanel());
    g.fillRoundedRectangle (block1Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block2Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block3Bounds.toFloat(), 6.0f);
    g.fillRoundedRectangle (block4Bounds.toFloat(), 6.0f);

    g.setColour (ovt::accentSoft().withAlpha(0.3f));
    g.drawRoundedRectangle (block1Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block2Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block3Bounds.toFloat(), 6.0f, 1.0f);
    g.drawRoundedRectangle (block4Bounds.toFloat(), 6.0f, 1.0f);

    // Top banner with title (opaque, matches tab bar background).
    g.setColour (ovt::bgDark());
    g.fillRect (0, 0, getWidth(), 50);

    // --- LOGO DRAWING ---
    juce::Rectangle<float> logoArea (20.0f, 10.0f, 30.0f, 30.0f);
    
    // Draw stylized O
    g.setColour(ovt::accent());
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
    juce::Font titleFont = ovt::fontTitle();

    g.setFont (titleFont);
    
    // Measure width dynamically to perfectly stick the two words together
    int openVoxWidth = titleFont.getStringWidth("OpenVox");
    
    // "OpenVox" in Accent Color
    g.setColour (ovt::accent());
    g.drawText ("OpenVox", 60, 8, openVoxWidth, 36, juce::Justification::centredLeft);
    
    // "Tuner" in White
    g.setColour (ovt::text());
    int tunerWidth = titleFont.getStringWidth("Tuner");
    g.drawText ("Tuner", 60 + openVoxWidth, 8, tunerWidth, 36, juce::Justification::centredLeft);

    g.setColour (ovt::textDim());
    g.setFont (ovt::fontVersion());
#if defined (JucePlugin_VersionString)
    g.drawText ((juce::String("v") + JucePlugin_VersionString), 60 + openVoxWidth + tunerWidth + 8, 8, 120, 36, juce::Justification::centredLeft);
#elif defined (JucePlugin_Version)
    g.drawText ((juce::String("v") + juce::String (JucePlugin_Version)), 60 + openVoxWidth + tunerWidth + 8, 8, 120, 36, juce::Justification::centredLeft);
#else
    g.drawText ("v0.1.1", 60 + openVoxWidth + tunerWidth + 8, 8, 120, 36, juce::Justification::centredLeft);
#endif

    // CPU usage meter (top-right of header)
    {
        const juce::Rectangle<int> headerArea (0, 0, getWidth(), 50);
        const int cpuW = 64;
        const int cpuH = 16;
        // Position to the left of the A/B button (which is at menuX - 38)
        const int cpuX = headerArea.getRight() - cpuW - 86;
        const int cpuY = headerArea.getY() + (headerArea.getHeight() - cpuH) / 2;

        const int cpuPct = static_cast<int> (currentCpuUsage * 100.0f);
        const juce::String cpuText = "CPU " + juce::String (cpuPct) + "%";

        // Background
        g.setColour (ovt::cpuBg());
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

        g.setColour (ovt::cpuText());
        g.setFont (ovt::fontLegendHint());
        g.drawText (cpuText, cpuX, cpuY, cpuW, cpuH, juce::Justification::centred);
    }
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
    abButton.setBounds (menuArea.getX() - 38, 10, 30, 30);

    // Hide all old top-bar controls (they still work via attachments/handlers).
    latencyModeLabel.setBounds (0, 0, 0, 0);
    latencyModeBox.setBounds (0, 0, 0, 0);
    detectorLabel.setBounds (0, 0, 0, 0);
    detectorBox.setBounds (0, 0, 0, 0);
    midiToggleButton.setBounds (0, 0, 0, 0);
    bypassToggleButton.setBounds (0, 0, 0, 0);
    bypassButton.setBounds (0, 0, 0, 0);
    midiOutButton.setBounds (0, 0, 0, 0);
    updateButton.setBounds (0, 0, 0, 0);
    debugWindowButton.setBounds (0, 0, 0, 0);

    // === Visualizer (top) and Graphic Editor (middle) ===
    const int pad = 10;
    // Reserve 170 px for the bottom area (controls + scale keyboard)
    auto centerArea = bounds.removeFromTop (bounds.getHeight() - 170);
    tabbedComponent.setBounds (centerArea.reduced (pad));
    
    // Graphic Mode specific tools aligned to the right of the tab bar
    auto tabBounds = tabbedComponent.getBounds();
    auto toolsArea = tabBounds.removeFromTop(30).reduced(2, 4); // height is 22
    
    int iconSize = toolsArea.getHeight(); // 22

    // Existing toolbar icons (from right to left)
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

    // === Bottom bar: 4 blocks ===
    auto bottomArea = bounds.reduced (pad);

    // Layout: Correction (knobs) | Effects (Formant+Reverb) | Scale/Keyboard | Harmony
    const int knobBlockWidth = 280;
    const int effectBlockWidth = 200;
    const int scaleBlockWidth = 260;
    const int blockSpacing = 10;

    auto leftBlock = bottomArea.removeFromLeft(knobBlockWidth);
    bottomArea.removeFromLeft(blockSpacing);
    auto effectBlock = bottomArea.removeFromLeft(effectBlockWidth);
    bottomArea.removeFromLeft(blockSpacing);
    auto middleBlock = bottomArea.removeFromLeft(scaleBlockWidth);
    bottomArea.removeFromLeft(blockSpacing);
    auto rightBlock = bottomArea; // remaining area

    block2Bounds = leftBlock;     // Correction: Speed, Amount, FlexTune, Humanize
    block4Bounds = effectBlock;   // Effects: Formant (toggle+knob), Reverb (toggle+knob)
    block1Bounds = middleBlock;   // Key, Scale, Keyboard
    block3Bounds = rightBlock;    // Harmony controls

    // --- Block 2 : Correction Knobs (Left) ---
    auto b2 = block2Bounds.reduced(10);
    // Top row: 2 big knobs (Speed, Amount) — 3x bigger than before
    const int bigKnobsHeight = 90;
    auto bigArea = b2.removeFromTop (bigKnobsHeight);

    const int knobPadding = 8;
    int bigKnobWidth = (bigArea.getWidth() - knobPadding) / 2;

    // Speed (big)
    auto bSpeed = bigArea.removeFromLeft(bigKnobWidth);
    speedLabel.setBounds(bSpeed.removeFromTop(18));
    speedSlider.setBounds(bSpeed);
    bigArea.removeFromLeft(knobPadding);

    // Amount (big)
    auto bAmount = bigArea;
    amountLabel.setBounds(bAmount.removeFromTop(18));
    amountSlider.setBounds(bAmount);

    // Bottom row: FlexTune + Humanize on one line, no textbox, label + knob side by side
    b2.removeFromTop (4);
    const int smallRowHeight = 50;
    auto smallArea = b2.removeFromTop (smallRowHeight);
    int smallHalf = (smallArea.getWidth() - knobPadding) / 2;

    // FlexTune: label "Flex" tight to the left, then knob fills the rest
    auto flexCol = smallArea.removeFromLeft(smallHalf);
    flexTuneLabel.setBounds (flexCol.removeFromLeft(28));
    flexTuneSlider.setBounds (flexCol);
    smallArea.removeFromLeft(knobPadding);

    // Humanize: label "Humanize" tight to the left, then knob fills the rest
    auto humanCol = smallArea;
    humanizeLabel.setBounds (humanCol.removeFromLeft(52));
    humanizeSlider.setBounds (humanCol);

    // --- Block 4 : Effects (Formant + Reverb, side by side, same size as Speed/Amount) ---
    auto b4 = block4Bounds.reduced(10);
    // Force knob height to match Speed/Amount (90px)
    auto effectKnobArea = b4.removeFromTop (90);
    int effectHalf = (effectKnobArea.getWidth() - 6) / 2;

    // Formant: toggle + knob (left half)
    auto formantCol = effectKnobArea.removeFromLeft(effectHalf);
    formantEnableButton.setBounds(formantCol.removeFromTop(18));
    formantSlider.setBounds(formantCol);
    effectKnobArea.removeFromLeft(6);

    // Reverb: toggle + knob (right half, no label)
    auto reverbCol = effectKnobArea;
    reverbEnableButton.setBounds(reverbCol.removeFromTop(18));
    reverbMixSlider.setBounds (reverbCol);

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

    topRow.removeFromLeft(10); // spacer

    // Middle: Scale Box (fixed narrower width)
    int desiredScaleWidth = juce::jmin(140, topRow.getWidth());
    auto bScale = topRow.removeFromLeft(desiredScaleWidth);
    scaleLabel.setBounds(bScale.removeFromTop(20));
    scaleBox.setBounds(bScale);

    b1.removeFromTop(6); // spacer
    
    // Bottom: Keyboard — ensure enough height for clickable keys
    scaleKeyboard.setBounds(b1.removeFromTop(55).withSizeKeepingCentre(180, 55));
}

void OpenVoxTunerAudioProcessorEditor::timerCallback()
{
    currentCpuUsage = processorRef.getCpuUsage();

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
                updateButton.setColour (juce::TextButton::buttonColourId, ovt::accent().darker (0.2f));
                updateButton.setTooltip ("Open the latest OpenVoxTuner release.");
            }
            else
            {
                updateButton.setButtonText ("Up to date");
                updateButton.setColour (juce::TextButton::buttonColourId, ovt::bgPanel());
                updateButton.setTooltip (updateCheckState->statusText.isNotEmpty() ? updateCheckState->statusText : "Open the OpenVoxTuner releases page.");
            }
        }
    }

    // Sync tab <-> "mode" parameter
    int tabIndex = tabbedComponent.getCurrentTabIndex();

    // Visibility of Curve Editor Mode specific buttons
    bool isCurveEditorMode = (tabIndex == 1);
    
    // When switching to Curve Editor tab, clear previous harmony traces
    if (isCurveEditorMode && curveEditor != nullptr)
        curveEditor->clearHarmonyTraces();
    
    presetsButton.setVisible (isCurveEditorMode);
    snapButton.setVisible (isCurveEditorMode);
    snapGridButton.setVisible (isCurveEditorMode);
    stepModeButton.setVisible (isCurveEditorMode);
    clearCurveButton.setVisible (isCurveEditorMode);

    // Gray out Formant slider if disabled
    bool isFormantEnabled = formantEnableButton.getToggleState();
    formantSlider.setEnabled (isFormantEnabled);

    // Gray out Reverb slider if disabled
    bool isReverbEnabled = reverbEnableButton.getToggleState();
    reverbMixSlider.setEnabled (isReverbEnabled);

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
        // Auto-scroll toggle: always visible. Only functional in ARA mode.
        curveEditor->setAutoScrollVisible (true);
        curveEditor->setPlayheadTime(processorRef.getTransportTime(), processorRef.getIsPlaying());
        // Propagate time signature (Feature 1) — read from processor
        int num = processorRef.getCurrentTimeSigNumerator();
        int den = processorRef.getCurrentTimeSigDenominator();
        curveEditor->setTimeSignature (num, den);
        // Sync embedded controls with persisted parameters
        // (controls are children of PitchCurveEditor, not PluginEditor)
        if (!measuresSyncDone) {
            // First time: restore ComboBox and toggle from persisted parameters
            auto& mBox = curveEditor->getMeasuresBox();
            auto* measuresRaw = processorRef.getParameters().getRawParameterValue("editor_measures");
            float measures = measuresRaw ? measuresRaw->load() : 4.0f;
            int mIdx = mBox.getText().getIntValue();
            if (mIdx != static_cast<int>(measures))
            {
                for (int i = 0; i < mBox.getNumItems(); ++i)
                {
                    if (mBox.getItemText(i).getIntValue() == static_cast<int>(measures))
                    {
                        mBox.setSelectedItemIndex (i, juce::dontSendNotification);
                        break;
                    }
                }
            }
            curveEditor->setMeasuresVisible (static_cast<int> (measures));

            auto* scrollRaw = processorRef.getParameters().getRawParameterValue("auto_scroll");
            bool scrollOn = scrollRaw ? (scrollRaw->load() > 0.5f) : true;
            curveEditor->getAutoScrollToggle().setToggleState (scrollOn, juce::dontSendNotification);
            curveEditor->setAutoScroll (scrollOn);

            measuresSyncDone = true;
        }
        // Persist embedded control states -> AudioProcessor parameters
        // (Parameters are automatically saved/loaded by the host)
        // Measures: write back from ComboBox to parameter
        auto* measuresParam = processorRef.getParameters().getRawParameterValue("editor_measures");
        if (measuresParam && curveEditor) {
            int curVal = curveEditor->getMeasuresBox().getText().getIntValue();
            if (curVal > 0)
                const_cast<std::atomic<float>*>(measuresParam)->store(static_cast<float>(curVal));
        }
        // Auto-scroll: write back from ToggleButton to parameter
        auto* scrollParam = processorRef.getParameters().getRawParameterValue("auto_scroll");
        if (scrollParam && curveEditor) {
            const_cast<std::atomic<float>*>(scrollParam)->store(
                curveEditor->getAutoScrollToggle().getToggleState() ? 1.0f : 0.0f);
        }
        // Sync curve from processor to editor after state restore or editor recreation
        if (processorRef.getPendingCurveRestore().load() && curveEditor != nullptr)
        {
            curveEditor->setCurve (processorRef.getPitchCurve());
            processorRef.getPendingCurveRestore().store (false);
            syncEditButtons();
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



        // MIDI status label removed; debug window shows detailed MIDI log.
    }
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
        // Use the processor's ScaleQuantizer directly for authoritative intervals.
        // This avoids duplicating the scale table and prevents mismatches.
        const auto& intervals = processorRef.getScaleIntervals();
        pitchVisualizer->setScaleIntervals (intervals);
        curveEditor->setScaleIntervals (intervals);

        auto* rawKey = processorRef.getParameters().getRawParameterValue ("key");
        auto* rawScale = processorRef.getParameters().getRawParameterValue ("scale");
        const int keyIdx = rawKey ? static_cast<int> (std::round (rawKey->load())) : 0;
        const int scaleIdx = rawScale ? static_cast<int> (std::round (rawScale->load() * 13.0f)) : 0;
        
        if (scaleIdx == 13)
        {
            // Custom scale: also sync custom intervals from the scale keyboard
            juce::Array<int> customIntervals;
            for (int i = 0; i < 12; ++i)
                if (scaleKeyboard.getButton(i).getToggleState())
                    customIntervals.add (i);
            curveEditor->setCustomIntervals (customIntervals);
            curveEditor->setKeyAndScale (keyIdx, atdsp::Scale::Custom);
        }
        else
        {
            curveEditor->setKeyAndScale (keyIdx, static_cast<atdsp::Scale> (juce::jlimit (0, 13, scaleIdx)));
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

void OpenVoxTunerAudioProcessorEditor::refreshDrawableButtonIcons()
{
    // The DrawableButton colour IDs are already re-applied in
    // applyThemeToAllComponents(). This method is a hook for
    // future SVG icon recreation if needed.
    applyThemeToAllComponents();
}

bool OpenVoxTunerAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Reserved for future shortcuts. Currently overlay is toggled via menu only.
    return false;
}

void OpenVoxTunerAudioProcessorEditor::mouseDown (const juce::MouseEvent&)
{
}

void OpenVoxTunerAudioProcessorEditor::refreshLabels()
{
    // Update registered labels
    for (auto& tl : translatableLabels)
    {
        if (tl.label != nullptr)
            tl.label->setText (ovt::tr (tl.key), juce::dontSendNotification);
    }

    // Update scale combo box items
    {
        const char* scaleKeys[] = {
            ovt::Keys::kScaleChromatic, ovt::Keys::kScaleMajor,
            ovt::Keys::kScaleMelodicMinor, ovt::Keys::kScaleHarmonicMinor,
            ovt::Keys::kScaleNaturalMinor, ovt::Keys::kScaleMajorPentatonic,
            ovt::Keys::kScaleMinorPentatonic, ovt::Keys::kScaleBlues,
            ovt::Keys::kScaleDorian, ovt::Keys::kScalePhrygian,
            ovt::Keys::kScaleLydian, ovt::Keys::kScaleMixolydian,
            ovt::Keys::kScaleLocrian, ovt::Keys::kScaleCustom
        };
        const int currentScale = scaleBox.getSelectedItemIndex();
        scaleBox.clear (juce::dontSendNotification);
        for (int i = 0; i < 14; ++i)
            scaleBox.addItem (ovt::tr (scaleKeys[i]), i + 1);
        if (currentScale >= 0)
            scaleBox.setSelectedItemIndex (currentScale, juce::dontSendNotification);
    }

    // Update harmony combo box items (all 20)
    {
        const char* harmonyKeys[] = {
            ovt::Keys::kHarmonyNone, ovt::Keys::kHarmony3rdBelow,
            ovt::Keys::kHarmony3rdAbove, ovt::Keys::kHarmony3rdBelowAbove,
            ovt::Keys::kHarmony4thBelow, ovt::Keys::kHarmony4thAbove,
            ovt::Keys::kHarmony4thBelowAbove, ovt::Keys::kHarmony5thBelow,
            ovt::Keys::kHarmony5thAbove, ovt::Keys::kHarmony5thBelowAbove,
            ovt::Keys::kHarmony3rdBelow5thAbove, ovt::Keys::kHarmony5thBelow3rdAbove,
            ovt::Keys::kHarmonyOctaveBelow, ovt::Keys::kHarmonyOctaveAbove,
            ovt::Keys::kHarmonyOctaveBelowAbove, ovt::Keys::kHarmonyVocalStack3,
            ovt::Keys::kHarmonyVocalStack4, ovt::Keys::kHarmonyPowerChord,
            ovt::Keys::kHarmonyParallel3rd, ovt::Keys::kHarmonyDrone,
            ovt::Keys::kHarmonyUnison2
        };
        const int currentHarmony = harmonyTypeBox.getSelectedItemIndex();
        harmonyTypeBox.clear (juce::dontSendNotification);
        for (int i = 0; i < 21; ++i)
            harmonyTypeBox.addItem (ovt::tr (harmonyKeys[i]), i + 1);
        if (currentHarmony >= 0)
            harmonyTypeBox.setSelectedItemIndex (juce::jmin (currentHarmony, 19), juce::dontSendNotification);
    }

    // Update "Use Voice" toggle button text
    useVoiceButton.setButtonText (ovt::tr (ovt::Keys::kLabelUseVoice));

    // Refresh all translatable tooltips
    bypassButton.setTooltip (ovt::tr (ovt::Keys::kTooltipBypass));
    bypassToggleButton.setTooltip (ovt::tr (ovt::Keys::kTooltipBypass));
    midiToggleButton.setTooltip (ovt::tr (ovt::Keys::kTooltipMidiOut));
    correctionModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipCorrection));
    harmonyEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipHarmonyEn));
    reverbEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipReverbEn));
    formantEnableButton.setTooltip (ovt::tr (ovt::Keys::kTooltipFormant));
    flexTuneSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipFlexTune));
    humanizeSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipHumanize));
    harmonyToneColorSlider.setTooltip (ovt::tr (ovt::Keys::kTooltipToneColor));
    abButton.setTooltip (ovt::tr (ovt::Keys::kTooltipAB));
    resetTransportButton.setTooltip (ovt::tr (ovt::Keys::kTooltipReset));

    // Refresh drawable button tooltips (already use ovt::tr but need refresh)
    presetsButton.setTooltip (ovt::tr (ovt::Keys::kTooltipPresets));
    snapButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSnapToScale));
    snapGridButton.setTooltip (ovt::tr (ovt::Keys::kTooltipSnapToGrid));
    stepModeButton.setTooltip (ovt::tr (ovt::Keys::kTooltipStepMode));
    clearCurveButton.setTooltip (ovt::tr (ovt::Keys::kTooltipClearAll));
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
    syncEditButtons();

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

// === A/B Comparison ===
void OpenVoxTunerAudioProcessorEditor::saveSlot (ABState& slot)
{
    juce::MemoryBlock block;
    processorRef.getStateInformation (block);
    // Store the binary plugin state as a base64-encoded XML attribute
    slot.state = std::make_unique<juce::XmlElement> ("AB_SLOT");
    slot.state->setAttribute ("data", block.toBase64Encoding());
    slot.hasData = true;
    slot.name = "filled";
}

void OpenVoxTunerAudioProcessorEditor::loadSlot (const ABState& slot)
{
    if (! slot.hasData || slot.state == nullptr) return;
    const auto dataStr = slot.state->getStringAttribute ("data");
    if (dataStr.isEmpty()) return;
    juce::MemoryBlock block;
    block.fromBase64Encoding (dataStr);
    processorRef.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
}

void OpenVoxTunerAudioProcessorEditor::toggleAB()
{
    if (isSlotAActive)
    {
        saveSlot (slotA);
        loadSlot (slotB);
        isSlotAActive = false;
        abButton.setButtonText ("B");
    }
    else
    {
        saveSlot (slotB);
        loadSlot (slotA);
        isSlotAActive = true;
        abButton.setButtonText ("A");
    }
}

// === MIDI Learn ===
void OpenVoxTunerAudioProcessorEditor::startMidiLearn (const juce::String& parameterId)
{
    learnState.isLearning = true;
    learnState.parameterId = parameterId;
    learnState.assignedCc = -1;

    // Show a brief dialog prompting the user to move a MIDI controller.
    juce::AlertWindow::showMessageBoxAsync (
        juce::AlertWindow::InfoIcon,
        "MIDI Learn",
        "Move a MIDI controller to assign it to this parameter.\nPress Escape to cancel.",
        "OK");
}

void OpenVoxTunerAudioProcessorEditor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (! learnState.isLearning || ! message.isController()) return;

    const int cc = message.getControllerNumber();
    auto* param = processorRef.getParameters().getParameter (learnState.parameterId);
    if (param != nullptr)
    {
        // Store the CC assignment (could be persisted to state).
        learnState.assignedCc = cc;
        learnState.isLearning = false;
    }
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


    applyMenuLookAndFeel (menu, customLookAndFeel);
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
            syncEditButtons();
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

void OpenVoxTunerAudioProcessorEditor::applyThemeToAllComponents()
{
    // Refresh LookAndFeel colours
    customLookAndFeel.refreshThemeColours();

    // Re-apply colours to ALL sliders (knobs)
    auto applySliderColours = [] (juce::Slider& s) {
        s.setColour (juce::Slider::textBoxTextColourId,       ovt::text());
        s.setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::rotarySliderFillColourId,  ovt::accent());
        s.setColour (juce::Slider::rotarySliderOutlineColourId, ovt::accentSoft());
        s.setColour (juce::Slider::thumbColourId,             juce::Colours::white);
    };
    applySliderColours (speedSlider);
    applySliderColours (amountSlider);
    applySliderColours (formantSlider);
    applySliderColours (reverbMixSlider);
    applySliderColours (flexTuneSlider);
    applySliderColours (humanizeSlider);
    applySliderColours (harmonyGainSlider);
    applySliderColours (harmonyBlendSlider);

    // Re-apply colours to ALL labels
    auto applyLabelColours = [] (juce::Label& l) {
        l.setColour (juce::Label::textColourId, ovt::text());
    };
    applyLabelColours (speedLabel);
    applyLabelColours (amountLabel);
    applyLabelColours (flexTuneLabel);
    applyLabelColours (humanizeLabel);
    applyLabelColours (reverbMixLabel);
    applyLabelColours (harmonyGainLabel);
    applyLabelColours (harmonyBlendLabel);
    applyLabelColours (harmonyToneColorLabel);
    applyLabelColours (keyLabel);
    applyLabelColours (scaleLabel);
    applyLabelColours (latencyModeLabel);
    applyLabelColours (harmonyTypeLabel);

    // Re-apply colours to ALL combo boxes
    auto applyComboColours = [] (juce::ComboBox& c) {
        // Closed combo state: use darkest plugin background
        c.setColour (juce::ComboBox::backgroundColourId, ovt::bgDark());
        c.setColour (juce::ComboBox::textColourId,       ovt::text());
        c.setColour (juce::ComboBox::outlineColourId,    ovt::accentSoft());
        c.setColour (juce::ComboBox::arrowColourId,      ovt::accent());
        // Dropdown popup: same dark background
        c.setColour (juce::PopupMenu::backgroundColourId, ovt::bgDark());
        c.setColour (juce::PopupMenu::textColourId,       ovt::text());
        c.setColour (juce::PopupMenu::highlightedBackgroundColourId, ovt::accentSoft());
    };
    applyComboColours (keyBox);
    applyComboColours (scaleBox);
    applyComboColours (latencyModeBox);
    applyComboColours (harmonyTypeBox);
    applyComboColours (shiftedVoicesBox);
    applyComboColours (harmonyToneBox);

    // Re-apply colours to toggle buttons
    auto applyToggleColours = [] (juce::ToggleButton& b) {
        b.setColour (juce::ToggleButton::textColourId, ovt::text());
    };
    applyToggleColours (harmonyEnableButton);
    applyToggleColours (useVoiceButton);
    applyToggleColours (formantEnableButton);
    applyToggleColours (reverbEnableButton);

    // Re-apply to other buttons
    updateButton.setColour (juce::TextButton::buttonColourId,   ovt::bgPanel());
    updateButton.setColour (juce::TextButton::textColourOffId,  ovt::text());
    bypassToggleButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    midiToggleButton.setColour (juce::ToggleButton::textColourId, ovt::text());
    correctionModeButton.setColour (juce::TextButton::buttonColourId,  ovt::bgPanel());
    correctionModeButton.setColour (juce::TextButton::textColourOffId, ovt::text());
    abButton.setColour (juce::TextButton::buttonColourId,  ovt::bgPanel());
    abButton.setColour (juce::TextButton::textColourOffId, ovt::text());

    // Re-apply to curve editor buttons (only presetsButton is a TextButton)
    presetsButton.setColour (juce::TextButton::buttonColourId,  ovt::bgPanel());
    presetsButton.setColour (juce::TextButton::textColourOffId, ovt::text());
    presetsButton.setColour (juce::TextButton::textColourOnId,  ovt::accent());

    // Re-apply DrawableButton colours (background and text)
    auto applyDrawableBtnColours = [] (juce::DrawableButton& btn) {
        btn.setColour (juce::DrawableButton::backgroundColourId, juce::Colours::transparentBlack);
        btn.setColour (juce::DrawableButton::backgroundOnColourId, ovt::accent().withAlpha (0.2f));
        btn.setColour (juce::DrawableButton::textColourId, ovt::text());
    };
    applyDrawableBtnColours (snapButton);
    applyDrawableBtnColours (snapGridButton);
    applyDrawableBtnColours (stepModeButton);
    applyDrawableBtnColours (clearCurveButton);
    applyDrawableBtnColours (resetTransportButton);

    // Re-apply curve editor internal components (always dark theme)
    if (curveEditor != nullptr)
    {
        auto& mb = curveEditor->getMeasuresBox();
        mb.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff2a2a36));
        mb.setColour (juce::ComboBox::textColourId,       juce::Colour (0xffcccccc));
        mb.setColour (juce::ComboBox::outlineColourId,    juce::Colour (0x441A9AF0));
        mb.setColour (juce::ComboBox::arrowColourId,      juce::Colour (0xff1A9AF0));
        mb.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff191b1e));
        mb.setColour (juce::PopupMenu::textColourId,       juce::Colour (0xffcccccc));

        auto& ml = curveEditor->getMeasuresLabel();
        ml.setColour (juce::Label::textColourId, juce::Colour (0xffcccccc));
        ml.setFont (ovt::fontLabel());
    }

    // Re-apply to tabbed component
    tabbedComponent.setColour (juce::TabbedComponent::backgroundColourId, ovt::bgDark());
    tabbedComponent.setColour (juce::TabbedComponent::outlineColourId,   ovt::accentSoft());

    // Force repaint of tab content components
    tabbedComponent.repaint();
    if (pitchVisualizer != nullptr)
        pitchVisualizer->repaint();
    if (curveEditor != nullptr)
        curveEditor->repaint();

    repaint();
}

