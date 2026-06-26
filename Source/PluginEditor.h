// PluginEditor.h
// GUI Editor of the OpenVoxTuner plugin.
// Display: title bar, mode switcher, pitch visualizer, pitch curve editor
// (graphic mode), and row of knobs.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include "PluginProcessor.h"
#include "ui/PitchVisualizer.h"
#include "ui/PitchCurveEditor.h"
#include "ui/ScaleKeyboardComponent.h"
#include "dsp/NoteUtils.h"
#include "ui/LookAndFeel.h"

struct OpenVoxTunerUpdateCheckState;

class OpenVoxTunerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public ui::PitchCurveEditor::Listener,
                                         public juce::Timer
{
public:
    explicit OpenVoxTunerAudioProcessorEditor (OpenVoxTunerAudioProcessor&);
    ~OpenVoxTunerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // === PitchCurveEditor::Listener ===
    void pitchCurveChanged() override;

private:
    // === GUI Components ===
    juce::Slider speedSlider, amountSlider, formantSlider;
    juce::Label  speedLabel, amountLabel;

    // Formant Toggle
    juce::ToggleButton formantEnableButton;

    // Key and Scale are discrete values -> ComboBox.
    juce::ComboBox keyBox, scaleBox;
    juce::Label    keyLabel, scaleLabel;
    juce::ComboBox latencyModeBox;
    juce::Label    latencyModeLabel;

    // Bypass Button (power-style)
    juce::DrawableButton bypassButton { "Bypass", juce::DrawableButton::ImageOnButtonBackground };
    juce::ToggleButton bypassToggleButton;
    // MIDI Out toggle next to bypass
    juce::DrawableButton midiOutButton { "MIDI Out", juce::DrawableButton::ImageOnButtonBackground };
    juce::ToggleButton midiToggleButton;
    // Debug window toggle
    juce::TextButton debugWindowButton {"Debug"};

    // Harmony controls
    juce::ToggleButton harmonyEnableButton;
    juce::ComboBox    harmonyTypeBox;
    juce::Slider      harmonyGainSlider;
    juce::Slider      harmonyBlendSlider;
    juce::Label       harmonyTypeLabel;
    juce::Label       harmonyGainLabel;
    juce::Label       harmonyBlendLabel;
    // Use voice toggle and shifted voices selector
    juce::ToggleButton useVoiceButton;
    juce::ComboBox    shiftedVoicesBox;
    juce::ComboBox    harmonyToneBox;
    juce::Slider      harmonyToneColorSlider;
    juce::Label       harmonyToneColorLabel;

    // Snap Toggle.
    // Custom button that draws an icon and text
    class PresetsButton : public juce::TextButton
    {
    public:
        PresetsButton(const juce::String& name) : juce::TextButton(name) {}
        void setIcon(std::unique_ptr<juce::Drawable> d) { icon = std::move(d); }
    protected:
        void paint(juce::Graphics& g) override
        {
            TextButton::paint(g);
            if (icon)
            {
                int h = getHeight();
                auto iconBounds = icon->getBounds();
                float y = (h - iconBounds.getHeight()) * 0.5f;
                juce::AffineTransform t = juce::AffineTransform().translated(0.0f, y);
                icon->draw(g, 1.0f, t);
            }
        }
    private:
        std::unique_ptr<juce::Drawable> icon;
    };

    PresetsButton presetsButton {"Presets"};
    juce::DrawableButton snapButton {"Snap to scale", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton snapGridButton {"Snap to grid", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton stepModeButton {"Step mode", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton clearCurveButton {"Clear curve", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton resetTransportButton {"Reset playhead", juce::DrawableButton::ImageOnButtonBackground};

    // Update checker / release notification.
    juce::TextButton updateButton { "Check updates" };
    std::shared_ptr<OpenVoxTunerUpdateCheckState> updateCheckState;

    std::unique_ptr<juce::TooltipWindow> tooltipWindow;
    
    // Custom Look And Feel must be instantiated BEFORE the components that use it
    ui::AutotuneLookAndFeel customLookAndFeel;

    // Scale selection keyboard
    ui::ScaleKeyboardComponent scaleKeyboard;

    // Attachments.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formantEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiOutAttachment;
    // Toggle-based attachments for the top bar (icon mirrors toggle)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassToggleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> midiToggleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> harmonyEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> useVoiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shiftedVoicesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonyToneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyToneColorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonyTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyBlendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> latencyModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 12> customAttachments;

    // Pitch visualizer + pitch curve editor.
    std::unique_ptr<ui::PitchVisualizer>     pitchVisualizer;
    std::unique_ptr<ui::PitchCurveEditor>    curveEditor;

    // Tabs to separate "Visualizer" (Auto) and "Graphic" (Advanced) views
    juce::TabbedComponent tabbedComponent { juce::TabbedButtonBar::TabsAtTop };

    // Reference to the processor (not owned, processor owns the editor).
    OpenVoxTunerAudioProcessor& processorRef;

    void timerCallback() override;

    // Updates the visualizer with the current pitch from the processor.
    void refreshVisualizer();

    void showPresetsMenu (const juce::MouseEvent* mouseEvent = nullptr);
    void loadCustomPresetFromFile (const juce::File& file);
    void promptSaveCustomPreset();
    void writeCustomPresetFile (const juce::String& name, const juce::File& file);
    void deleteCustomPresetFile (const juce::File& file);
    void applyPresetUiStateFromXml (const juce::XmlElement& xml);
    void startUpdateCheck();
    static bool isVersionNewer (const juce::String& latest, const juce::String& current);

    // Bounds for the bottom blocks
    juce::Rectangle<int> block1Bounds;
    juce::Rectangle<int> block2Bounds;
    juce::Rectangle<int> block3Bounds; // Harmony block

    // One-time flag for syncing editor controls from persisted parameters
    bool measuresSyncDone = false;

public:
    // Atomic flag set by processor after setStateInformation restores curve.
    std::atomic<bool> needsCurveSync { false };

    // Theme colors.
    static const juce::Colour kBgDark;
    static const juce::Colour kBgPanel;
    static const juce::Colour kAccent;
    static const juce::Colour kAccentSoft;
    static const juce::Colour kText;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenVoxTunerAudioProcessorEditor)
};
