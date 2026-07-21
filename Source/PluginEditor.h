// PluginEditor.h
// GUI Editor of the OpenVoxTuner plugin.
// Display: title bar, mode switcher, pitch visualizer, pitch curve editor
// (graphic mode), and row of knobs.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <map>
#include <memory>
#include <functional>
#include "PluginProcessor.h"
#include "ui/PitchVisualizer.h"
#include "ui/PitchCurveEditor.h"
#include "ui/ScaleKeyboardComponent.h"
#include "dsp/NoteUtils.h"
#include "ui/LookAndFeel.h"
#include "dsp/PresetMorpher.h"

struct OpenVoxTunerUpdateCheckState;

class PresetGallery; // defined in ui/PResetGallery.h

class CorrectionModeSwitch; // graphic Modern/Transparent console switch (defined in PluginEditor.cpp)

// Look and feel for the Correction block's "Advanced" handle: a centred chevron
// (direction indicator) plus grip lines, drawn on a subtle background (square left
// edge, rounded right corners, no border) that fills the block's right edge.
class VerticalTextButtonLF : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

class OpenVoxTunerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public ui::PitchCurveEditor::Listener,
                                         public juce::Slider::Listener,
                                         public juce::Button::Listener,
                                         public juce::ComboBox::Listener,
                                         public juce::Timer
{
public:
    explicit OpenVoxTunerAudioProcessorEditor (OpenVoxTunerAudioProcessor&);
    ~OpenVoxTunerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Re-enable the maximise button on the Standalone window (JUCE's StandaloneFilterWindow
        only requests minimise + close by default). No-op inside a DAW host. */
    void parentHierarchyChanged() override;

    void mouseDown (const juce::MouseEvent& event) override;
    bool keyPressed (const juce::KeyPress& key) override;

    // === Global plugin Undo/Redo (Option 1) — gesture listeners ===
    void sliderValueChanged (juce::Slider*) override;
    void sliderDragStarted (juce::Slider*) override;
    void sliderDragEnded (juce::Slider*) override;
    void buttonClicked (juce::Button*) override;
    void comboBoxChanged (juce::ComboBox*) override;

    // === PitchCurveEditor::Listener ===
    void pitchCurveChanged() override;

    /** Re-apply all component colours after a theme change. */
    void applyThemeToAllComponents();

    /** Refresh DrawableButton SVG icons for the current theme. */
    void refreshDrawableButtonIcons();

private:
    // === Custom Look And Feel ===
    // MUST be declared BEFORE all GUI components (Slider, Label, TooltipWindow,
    // TabbedComponent, etc.) that hold a LookAndFeel pointer set via
    // setLookAndFeel(&customLookAndFeel). C++ destroys members in REVERSE order
    // of declaration, so placing customLookAndFeel at the top of the private
    // section guarantees it is the LAST member destroyed — after every
    // component that may still reference it has been torn down. Otherwise, on
    // editor destruction (Standalone quit, VST3 unload, debug-mode close), the
    // LookAndFeel is deleted while components still hold a pointer/weak-ref
    // to it -> assertion / crash with
    // "refCount.value = 2" reported by Copilot.
    ui::OVTLookAndFeel customLookAndFeel;

    // === GUI Components ===
    juce::Slider speedSlider, amountSlider, formantSlider;
    juce::Label  speedLabel, amountLabel;

    // Morph slider (visible only when A and B are both filled)
    juce::Slider morphSlider { "Morph" };
    juce::Label morphSliderLabel;

    // Formant Toggle
    juce::ToggleButton formantEnableButton;
    juce::ComboBox formantModeBox;

    // Key and Scale are discrete values -> ComboBox.
    juce::ComboBox keyBox, scaleBox;
    juce::Label    keyLabel, scaleLabel;
    juce::ComboBox latencyModeBox;
    juce::Label    latencyModeLabel;
    // Pitch detector (YIN only).
    juce::ComboBox detectorBox;
    juce::Label    detectorLabel;

    // Key detection source + companion group (automatic key detection).
    juce::ComboBox keySourceBox, companionGroupBox;
    juce::Label    keySourceLabel, companionGroupLabel;
    juce::ToggleButton keyDetectPowerButton;   // Key/Scale detection on/off (power-icon style)

    // Bypass Button (power-style)
    juce::DrawableButton bypassButton { "Bypass", juce::DrawableButton::ImageOnButtonBackground };
    juce::ToggleButton bypassToggleButton;
    // MIDI Out toggle next to bypass
    juce::DrawableButton midiOutButton { "MIDI Out", juce::DrawableButton::ImageOnButtonBackground };
    juce::ToggleButton midiToggleButton;
    // Debug window toggle
    juce::TextButton debugWindowButton {"Debug"};

    // === Keyboard shortcuts help overlay (separate component for correct z-order) ===
    bool helpOverlayVisible = false;
    bool showWaveform = true; // Waveform overlay toggle (active by default)
    void toggleHelpOverlay();

    class HelpOverlayComponent : public juce::Component
    {
    public:
        HelpOverlayComponent (OpenVoxTunerAudioProcessorEditor& editor);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
    private:
        OpenVoxTunerAudioProcessorEditor& owner;
    };
    HelpOverlayComponent helpOverlay { *this };

    // === i18n label refresh ===
    struct TranslatableLabel { juce::Label* label; const char* key; };
    std::vector<TranslatableLabel> translatableLabels;
    void refreshLabels();

    // === Export image file chooser ===
    std::unique_ptr<juce::FileChooser> exportFileChooser;

    // Hamburger menu (gear icon button, replaces all top-bar controls).
    juce::DrawableButton menuButton { "Options", juce::DrawableButton::ImageOnButtonBackground };

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
    juce::ToggleButton harmonyFollowLeadButton; // Harmony voices follow the lead correction character
    juce::ToggleButton harmonyGainMatchButton; // Scale harmony by 1/sqrt(1+N) to keep total RMS ~ dry

    // Reverb controls (post-processing effect)
    juce::ToggleButton reverbEnableButton;
    juce::Slider       reverbMixSlider;
    juce::Label        reverbMixLabel;

    // Noise Gate
    juce::ToggleButton noiseGateEnableButton;
    juce::Slider noiseGateThresholdSlider;
    juce::Label noiseGateThresholdLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> noiseGateEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseGateThresholdAttachment;

    // Upward Compressor (input, before tuning)
    juce::ToggleButton upwardCompEnableButton;
    juce::Slider       upwardCompAmountSlider;
    juce::Label        upwardCompAmountLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> upwardCompEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upwardCompAmountAttachment;

    // FlexTune / Humanize / Vibrato / Attack-Aware / Correction Mode
    juce::Slider      flexTuneSlider;
    juce::Label       flexTuneLabel;
    juce::Slider      humanizeSlider;
    juce::Label       humanizeLabel;
    juce::Slider      vibratoPreserveSlider;
    juce::Label       vibratoPreserveLabel;
    juce::ToggleButton attackAwareButton;
    juce::Slider      attackReleaseSlider;
    juce::Label       attackReleaseLabel;
    juce::TextButton  correctionModeButton;

    // Graphic Modern/Transparent console switch, placed below the Speed/Amount
    // knobs. Backed by correctionModeButton so the existing undo + parameter
    // wiring is reused unchanged.
    std::unique_ptr<CorrectionModeSwitch> modeSwitch;

    // Curve Editor "Options" button: icon-only (hamburger) with a distinct
    // accent-tinted background so it stands out from the neutral zoom/scroll/
    // snap buttons in this section. The plugin's own options keep the gear.
    juce::DrawableButton optionsButton {"Curve Options", juce::DrawableButton::ImageOnButtonBackground};
    // Preset Gallery toolbar button (icon-only grid, opens the browsable gallery).
    juce::DrawableButton presetGalleryButton {"Preset Gallery", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton snapButton {"Snap to scale", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton snapGridButton {"Snap to grid", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton stepModeButton {"Step mode", juce::DrawableButton::ImageOnButtonBackground};
    // Curve editor view controls (mirror the Visualizer toolbar: zoom/scroll/reset).
    juce::DrawableButton zoomInButton   {"Zoom In",   juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton zoomOutButton  {"Zoom Out",  juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton scrollUpButton {"Scroll Up", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton scrollDownButton {"Scroll Down", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton resetViewButton {"Reset View", juce::DrawableButton::ImageOnButtonBackground};

    // "Measures" control (curve editor time window). Moved from the embedded editor
    // controls to the toolbar so it no longer covers the ruler.
    juce::Label measuresLabel;
    juce::ComboBox measuresComboBox;
    // Standalone transport. A single play/pause toggle (Play icon when stopped,
    // Stop icon when playing) plus a "Return to start" (rewind) button.
    // Only meaningful when not driven by a DAW host.
    juce::DrawableButton playButton {"Play", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton rewindButton {"Rewind", juce::DrawableButton::ImageOnButtonBackground};

    // Update checker / release notification.
    juce::TextButton updateButton { "Check updates" };
    std::shared_ptr<OpenVoxTunerUpdateCheckState> updateCheckState;

    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    // Scale selection keyboard
    ui::ScaleKeyboardComponent scaleKeyboard;

    // Attachments.
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> formantEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> formantModeAttachment;
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
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> harmonyFollowLeadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> harmonyGainMatchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> latencyModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverbEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reverbMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flexTuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humanizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoPreserveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attackAwareAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackReleaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> correctionModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> detectorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keySourceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> companionGroupAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keyDetectAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 12> customAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttachment;

    // Pitch visualizer + pitch curve editor.
    std::unique_ptr<ui::PitchVisualizer>     pitchVisualizer;
    std::unique_ptr<ui::PitchCurveEditor>    curveEditor;

    // Tabs to separate "Visualizer" (Auto) and "Graphic" (Advanced) views
    juce::TabbedComponent tabbedComponent { juce::TabbedButtonBar::TabsAtTop };

    // Reference to the processor (not owned, processor owns the editor).
    OpenVoxTunerAudioProcessor& processorRef;

    void timerCallback() override;

    // CPU usage display
    float currentCpuUsage = 0.0f;

    // Updates the visualizer with the current pitch from the processor.
    void refreshVisualizer();

    void showPresetsMenu (const juce::MouseEvent* mouseEvent = nullptr);
    /// Builds the curve preset manager as a PopupMenu (factory + custom submenus),
    /// reusable as a standalone menu or as a submenu of the Options menu.
    juce::PopupMenu buildPresetsMenu();
    /// Curve Editor "Options" menu (clean curves, reset playhead, presets).
    void showCurveOptionsMenu();
    /// Opens the Preset Gallery (browsable grid of factory + custom presets
    /// with curve thumbnails / metadata) in a modeless window.
    void showPresetGallery();
    /// Applies a factory preset (by PitchCurve::loadPreset key): resets the
    /// morph, loads the curve into the editor, commits it to the active A/B
    /// slot and aligns the morph slider. Shared by the gallery and the preset menu.
    void applyFactoryPreset (const juce::String& name);
    /// Reflects the standalone transport state on the Play/Stop toolbar buttons.
    void syncTransportButtons();
    void setWaveformDisplayType (int type);
    void loadCustomPresetFromFile (const juce::File& file);
    void promptSaveCustomPreset();
    void writeCustomPresetFile (const juce::String& name, const juce::File& file);
    void deleteCustomPresetFile (const juce::File& file, std::function<void (bool)> onDone = nullptr,
                                juce::Component* parentComp = nullptr);
    void applyPresetUiStateFromXml (const juce::XmlElement& xml);
    void startUpdateCheck();

    // Plugin Presets (separate from Curve Presets): save/delete a plugin
    // preset file (parameters.state only, no pitch curve). See the
    // implementation block in PluginEditor.cpp.
    void promptSavePluginPreset();
    void writePluginPresetFile (const juce::String& name, const juce::File& file);
    void deletePluginPresetFile (const juce::File& file);
    static bool isVersionNewer (const juce::String& latest, const juce::String& current);

    // Bounds for the bottom blocks
    juce::Rectangle<int> block1Bounds;
    juce::Rectangle<int> block2Bounds;
    juce::Rectangle<int> block3Bounds; // Harmony block
    juce::Rectangle<int> block4Bounds; // Effects block (Formant + Reverb)

    // "Advanced" expand/collapse banner for the Correction block. When expanded,
    // the correction knobs (Flex / Humanize / Vibrato / Attack-Aware) are revealed
    // to the right of the Speed / Amount knobs (the block widens).
    juce::TextButton advancedButton { "Advanced" };
    VerticalTextButtonLF advancedButtonLF;
    bool advancedExpanded = false;

    // One-time flag for syncing editor controls from persisted parameters
    bool measuresSyncDone = false;

    // Last waveform display type synced to visualizer/curve editor.
    int lastWaveformDisplayType = -1;
    // Tracks the last Harmony enable state seen by the timer so we can
    // re-run refreshLabels() (which disables the Follow Lead / Gain Match /
    // Use Voice sub-toggles) when Harmony changes via preset load or DAW
    // automation — paths that bypass the button's onStateChange callback.
    bool lastHarmonyEnabled = true;

    // === Global plugin Undo/Redo (Option 1) ===
    // Snapshot bookkeeping for the processor-owned UndoManager. We do NOT
    // record DAW automation (it never fires these gesture handlers) and we
    // do NOT record curve-point edits (the Curve Editor has its own undo).
    // `undoLiveSnapshot_` is the plugin state as of the last timer tick, used
    // as the "before" image for discrete clicks/toggles/combos. During a
    // slider drag we snapshot once at drag-start into `undoBeforeDrag_`.
    juce::ValueTree undoLiveSnapshot_;
    juce::ValueTree undoBeforeDrag_;
    bool undoGestureActive_ = false;   // inside a slider drag
    bool applyingUndo_ = false;        // re-entrancy guard while undo/redo runs

    // Global Undo/Redo buttons (icon-only DrawableButtons, placed in the
    // header between button B and the Options gear).
    juce::DrawableButton undoButton { "Undo", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton redoButton { "Redo", juce::DrawableButton::ImageOnButtonBackground };

    void performGlobalUndo();
    void performGlobalRedo();
    void commitUndoGesture (const juce::ValueTree& before);
    void registerUndoGestureListeners();
    // Wrap a programmatic state change (preset load) in one undo transaction.
    void pushUndoAroundCall (std::function<void()> fn);

    // === Plugin Presets (separate from Curve Presets) ===
    // Centred selector in the top banner: [<] [Combo: name] [>] [save].
    juce::DrawableButton presetPrevButton { "Prev Preset", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton presetNextButton { "Next Preset", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton presetSaveButton { "Save Preset", juce::DrawableButton::ImageOnButtonBackground };
    juce::ComboBox       presetComboBox;
    // Ordered list of plugin-preset names (factory "Default" first, then
    // custom). Mirrors the Curve Presets pattern but never touches the curve.
    juce::StringArray    pluginPresetNames;
    juce::String         currentPluginPresetName;

    // (Re)build pluginPresetNames from the factory list + custom files.
    void refreshPluginPresetList();
    // Apply a plugin preset by name (factory or custom). Wraps the processor
    // apply in the global-undo transaction so it is undoable.
    void applyPluginPresetByName (const juce::String& name);

    // === A/B Comparison ===
    struct ABState {
        juce::String name;
        juce::String presetName; // plugin preset assigned to this slot (empty = none / "User")
        std::unique_ptr<juce::XmlElement> state; // for full state restore
        std::unique_ptr<ovtdsp::MorphState> morphState; // direct snapshot for morphing
        bool hasData = false;
    };
    // Build the expected parameters.state ValueTree for a preset name
    // (factory = defaults + overrides; custom = parsed XML file). Returns an
    // invalid tree if the name is not a known preset. Used both to apply and
    // to test whether a slot still matches its assigned preset.
    juce::ValueTree buildPluginPresetStateByName (const juce::String& name);
    // True when the live parameter values still match the preset assigned to
    // the given slot (no user edits since the preset was loaded).
    bool slotMatchesPreset (const ABState& slot);

    ABState slotA, slotB;
    bool isSlotAActive = true;  // currently active slot
    bool suppressAutoSave = false; // skip one auto-save cycle after loadSlot
    bool switchingSlot = false; // true while loadSlot is executing (suppresses morph callback)

    void saveSlot (ABState& slot, int slotIndex);
    void loadSlot (const ABState& slot);
    void updateABButtonStates();

    // === Preset Morphing ===
    std::unique_ptr<ovtdsp::MorphState> morphSource;
    std::unique_ptr<ovtdsp::MorphState> morphTarget;
    bool morphActive = false;
    float lastMorphValue = 0.0f;
    double lastTransportTime = 0.0; // for DAW transport jump detection
    juce::String morphSourceName = "Source";
    juce::String morphTargetName = "Target";
    std::unique_ptr<ovtdsp::MorphState> morphUndoState; // pre-morph snapshot for undo

    // Tracks the normalized value the morph last applied to each parameter.
    // Used to detect parameters that are being driven externally (DAW/UI
    // automation): if a parameter's live value differs from this baseline, the
    // morph skips it so concurrent automation lanes (e.g. speed/amount) are not
    // overwritten by the morph crossfade.
    std::map<juce::String, float> lastMorphIntendedValues;

    void onMorphSliderChanged (float value);
    void showMorphContextMenu();
    void resetMorph();
    void undoMorph();

    // A/B toggle button (with right-click support)
    class ABTextButton : public juce::TextButton {
    public:
        using TextButton::TextButton;
        std::function<void()> onRightClick;
        void mouseDown (const juce::MouseEvent& e) override {
            if (e.mods.isRightButtonDown() && onRightClick)
                onRightClick();
            else
                TextButton::mouseDown (e);
        }
        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            if (isActive)
            {
                // Active slot: solid accent background + bright text
                g.setColour (juce::Colour (0xff1A9AF0));
                g.fillRoundedRectangle (bounds.reduced (1.0f), 3.0f);
                g.setColour (juce::Colours::white);
            }
            else
            {
                // Inactive slot: dark background + green border if filled
                g.setColour (juce::Colour (0xff2a2a36));
                g.fillRoundedRectangle (bounds.reduced (1.0f), 3.0f);
                g.setColour (hasValidData ? juce::Colour (0xff4CAF50) : juce::Colour (0xff555555));
                g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.5f);
                g.setColour (juce::Colour (0xffaaaaaa));
            }
            // Text
            g.setFont (getHeight() * 0.55f);
            g.drawText (getButtonText(), bounds, juce::Justification::centred);
        }
        bool hasValidData = false;
        bool isActive = false;
    };
    ABTextButton buttonA { "A" };
    ABTextButton buttonB { "B" };

    // === MIDI Learn ===
    struct MidiLearnState {
        bool isLearning = false;
        juce::String parameterId;
        int assignedCc = -1;
    };
    MidiLearnState learnState;

    /** Enter MIDI CC learn mode for the given parameter.
     *  A dialog prompts the user to move a MIDI controller; the first
     *  CC message received is bound to the parameter. */
    void startMidiLearn (const juce::String& parameterId);

    /** Called when a MIDI CC message arrives (e.g. from the processor).
     *  If learn mode is active, assigns the CC to the pending parameter. */
    void handleMidiMessage (const juce::MidiMessage& message);

    // Sync UI button toggle states from CurveEditor state (snap, grid, step)
    void syncEditButtons()
    {
        if (curveEditor != nullptr)
        {
            snapButton.setToggleState (curveEditor->isSnapEnabled(), juce::dontSendNotification);
            snapGridButton.setToggleState (curveEditor->isSnapToGridEnabled(), juce::dontSendNotification);
            stepModeButton.setToggleState (curveEditor->isStepModeEnabled(), juce::dontSendNotification);
        }
    }

public:
    // Theme colors (public so LookAndFeel can access them).
    static const juce::Colour kBgDark;
    static const juce::Colour kBgPanel;
    static const juce::Colour kAccent;
    static const juce::Colour kAccentSoft;
    static const juce::Colour kText;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OpenVoxTunerAudioProcessorEditor)
};
