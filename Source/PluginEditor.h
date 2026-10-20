// PluginEditor.h
// OpenVoxTuner DSP module
// Copyright (C) 2026 EiffelBS. Licensed under AGPLv3.



#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <map>
#include <memory>
#include <functional>
#include <vector>
#include "PluginProcessor.h"
#include "ui/PitchVisualizer.h"
#include "ui/PitchCurveEditor.h"
#include "ui/ScaleKeyboardComponent.h"
#include "dsp/NoteUtils.h"
#include "dsp/MidiImporter.h"
#include <eiffelbs/eiffelbs.h>
#include "ui/OVTTheme.h"
#include "ui/OVTFonts.h"
#include "ui/OVTSpineCaption.h"  // captions in the frame-spine typography: vertical knob captions, power-switch names
#include "ui/OVTHarmonyLayout.h"   // the Harmony frame's two zone widths (policy + measurement)
#include "dsp/PresetMorpher.h"

struct OpenVoxTunerUpdateCheckState;

class PresetGallery; // defined in ui/PResetGallery.h

class CorrectionModeSwitch; // graphic Modern/Transparent console switch (defined in PluginEditor.cpp)


/** Local host around the shared eiffelbs-ui LookAndFeel. Everything is
    stock ebs:: rendering; this subclass only replays OpenVoxTuner's legacy
    chrome through the library's theme hook so the cutover stays
    pixel-identical: the active tab uses a SOLID accent fill with a white
    caption (the shared default is a softer accent fill + accent caption). */
struct OvtLookAndFeel final : ebs::LookAndFeel
{
    juce::Colour widgetThemeColour (int id) override
    {
        switch (id)
        {
            // Only the legacy solid-accent tab pill stays app-specific;
            // checkbox wells now come from the library default (v0.4.1+).
            case tabActiveFillColourId:   return ebs::accent();
            case tabActiveTextColourId:   return juce::Colours::white;
            default:                      return {};
        }
    }
};

class OpenVoxTunerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::FileDragAndDropTarget,
                                         public ui::PitchCurveEditor::Listener,
                                         public juce::Slider::Listener,
                                         public juce::Button::Listener,
                                         public juce::ComboBox::Listener,
                                         public juce::Timer,
                                         private ebs::ThemeSubscriber
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

    // === Global plugin Undo/Redo (Option 1) - gesture listeners ===
    void sliderValueChanged (juce::Slider*) override;
    void sliderDragStarted (juce::Slider*) override;
    void sliderDragEnded (juce::Slider*) override;
    void buttonClicked (juce::Button*) override;
    void comboBoxChanged (juce::ComboBox*) override;

    // === Voice Type === (combo box in Correction block advanced area)
    juce::ComboBox voiceTypeBox;
    juce::Label   voiceTypeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> voiceTypeAttachment;

    // === Formant Strategy === (combo box selecting Current / P0 / P1 / P2)
    juce::ComboBox formantStrategyBox;
    juce::Label   formantStrategyLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> formantStrategyAttachment;

    // === PitchCurveEditor::Listener ===
    void pitchCurveChanged() override;

    /** Re-apply all component colours after a theme change. */
    void applyThemeToAllComponents();

private:
    // === Custom Look And Feel ===
    // MUST be declared BEFORE all GUI components (Slider, Label, TooltipWindow,
    // TabbedComponent, etc.) that hold a LookAndFeel pointer set via
    // setLookAndFeel(&customLookAndFeel). C++ destroys members in REVERSE order
    // of declaration, so placing customLookAndFeel at the top of the private
    // section guarantees it is the LAST member destroyed - after every
    // component that may still reference it has been torn down. Otherwise, on
    // editor destruction (Standalone quit, VST3 unload, debug-mode close), the
    // LookAndFeel is deleted while components still hold a pointer/weak-ref
    // to it -> assertion / crash with
    // "refCount.value = 2" reported by Copilot.
    /** Theme broadcast (v0.4): the editor subscribes to ebs::setTheme() and
        re-applies every per-instance colour here, including regenerating the
        toolbar icon drawables whose stroke tints are baked at creation. */
    void themeChanged() override;

    struct IconSkin
    {
        juce::DrawableButton* button;
        const char* svgNormal;
        const char* svgOn = nullptr;  ///< second glyph for transport-style pairs
        bool isToggle = false;        ///< ON-state drawables must be regenerated too
    };
    void restyleIconButtons();

    // Bottom-row width model: each block preferred width, and the floor below which its
    // widgets collide. Shared by the band registration and the per-block layout code.
    static constexpr int kKnobBlockW    = 220;  // Speed + Amount big knobs
    static constexpr int kAdvancedW     = 160;  // revealed correction zone when expanded
    static constexpr int kCorrPad       = 10;   // frame interior padding
    static constexpr int kEffectsPad    = 10;   // frame interior padding
    // One cell of the Effects grid: the power glyph (10.8 px), the gap (8 px) and the switch name
    // in the ONE caption size of the row (11 pt). Measured over all six shipped locales, the
    // longest name in a cell is the Japanese "フォルマント" (61.1 px at 11 pt), so 82 px is the
    // floor where every name fits without clipping.
    static constexpr int kEffectsColW   = 82;
    static constexpr int kEffectsColGap = 8;
    static constexpr int kScalePrefW    = 236;  // narrowed to fit the Root combo
    static constexpr int kScaleMinW     = 200;  // floor for the 180 px keyboard strip
    static constexpr int kHarmonyPad    = 10;   // frame interior padding
    // The Harmony frame's two zones. The main zone (enable, type, use-voice, voices, tone, two
    // toggles - visible in BOTH fold states) is measured: it is exactly as wide as its widest
    // control needs, which is the type combo's longest entry in the CURRENT language, so the
    // frame is no wider than its content and no entry ever truncates (ovt::harmonyMainZoneW()).
    // The expanded zone (the four dials) takes everything else, because two dials with air
    // between them read as two controls (ovt::kHarmonyKnobZoneW holds the policy).
    static constexpr int kHarmonyZoneGap = 8;   // between the main zone and the knob zone

    /** Width one voice name needs inside its power toggle, measured in the CURRENT language: the tick well,
     *  a gap and the text drawn in ebs::fontToggleButton(). The Reverb frame's columns are sized by this
     *  measurement because JUCE clips a ToggleButton label with an ellipsis instead of wrapping it. Measured over
     *  all six shipped locales (lead <= 43 px, harmony <= 67 px in Spanish "Armonías"), every name rides the
     *  kEffectsColW = 82 px cell floor, so no label ever clips; keeping the measurement live is what lets a future
     *  rename change the frame width instead of silently truncating a word. */
    int reverbHeaderNeed (const juce::String& text) const;

    /** The Harmony frame's width in each fold state, built from the zone policy in
     *  ui/OVTHarmonyLayout.h plus the frame's own chrome (chevron strip, padding, title spine):
     *  folded it is the MEASURED main zone alone, open it is that zone plus the knob zone the four
     *  dials need. The width model and the layout both read these, so the two cannot disagree
     *  about what the frame asks for - and the folded frame is never wider than its content. */
    int harmonyFoldedW() const;
    int harmonyOpenW() const;
    int harmonyOpenFloorW() const;

    /** The frame chrome the three widths above share: chevron strip, interior padding, title spine. */
    int harmonyChromeW() const;

    /** Re-state every bottom-row block widths from each frame own fold flag. Called at
     *  construction, on every fold, at the start of resized(), and after a language change (the
     *  measured zones - the room names and the Harmony main zone - are measured IN that language,
     *  so a stale width would clip a freshly translated label). */
    void updateBottomBandWidths();

    std::vector<IconSkin> iconSkins;

    OvtLookAndFeel customLookAndFeel;

    // === GUI Components ===
    ebs::Knob speedSlider, amountSlider;
    // The Formant dial belongs to the Effects block's Formant cell, which owns exactly ONE knob:
    // its activity dot is carried by the knob itself (ebs::ActivityKnob), not by a strip.
    ebs::ActivityKnob formantSlider;
    ovt::HorizontalCaption speedLabel, amountLabel;   // spines' caption typography, horizontal

    // Morph slider (visible only when A and B are both filled)
    ebs::MorphSlider morphSlider;   // component name set to "Morph" in the ctor
    juce::Label morphSliderLabel;

    // Formant Toggle
    ovt::PowerToggleCaps formantEnableButton;
    juce::ComboBox formantModeBox;

    // Key and Scale are discrete values -> ComboBox.
    juce::ComboBox keyBox, scaleBox;
    ovt::HorizontalCaption keyLabel, scaleLabel;      // spines' caption typography, horizontal
    juce::ComboBox latencyModeBox;
    juce::Label    latencyModeLabel;
    // Pitch detector (YIN only).
    juce::ComboBox detectorBox;
    juce::Label    detectorLabel;

    // Key detection source + companion group (automatic key detection).
    juce::ComboBox keySourceBox, companionGroupBox;
    ovt::HorizontalCaption keySourceLabel;            // the detection switch's name, in caption typography
    juce::Label    companionGroupLabel;
    ovt::PowerToggleCaps keyDetectPowerButton;   // Key/Scale detection on/off (power-icon style)

    // Bypass Button (power-style)
    juce::DrawableButton bypassButton { "Bypass", juce::DrawableButton::ImageOnButtonBackground };
    // MIDI Out toggle next to bypass
    juce::DrawableButton midiOutButton { "MIDI Out", juce::DrawableButton::ImageOnButtonBackground };
    // Debug window toggle
    juce::TextButton debugWindowButton {"Debug"};

    // === Keyboard shortcuts help overlay (separate component for correct z-order) ===
    bool helpOverlayVisible = false;
    bool showWaveform = true; // Waveform overlay toggle (active by default)
    bool showHarmoniesTrace = true; // Harmony blue lines toggle (active by default)
    // 0=Slow (1500ms), 1=Normal (700ms, JUCE default), 2=Fast (200ms)
    int tooltipDelayChoice = 1;
    void toggleHelpOverlay();

    /// Apply the user's choice (0=Slow/1500ms, 1=Normal/700ms, 2=Fast/200ms)
    /// to the global TooltipWindow so the change is immediate on the next hover.
    void setTooltipDelayChoice (int idx)
    {
        constexpr int delaysMs[3] = { 1500, 700, 200 };
        tooltipDelayChoice = juce::jlimit (0, 2, idx);
        if (tooltipWindow != nullptr)
            tooltipWindow->setMillisecondsBeforeTipAppears (delaysMs[tooltipDelayChoice]);
    }

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
    // The left title spine of a frame carries the same i18n rule: its text is refreshed with
    // the interface language, so it joins the refresh list next to the labels.
    struct TranslatableBand { ebs::FrameTitleBand* band; const char* key; };
    std::vector<TranslatableBand> translatableBands;
    void refreshLabels();

    /** A caption follows its control: at full strength while the control is live, at the caption
        level (50 %) while it is off or disabled with it - the same rule the smallcaps captions
        follow (ovt::spine::captionColour). Called after the enable block and after a theme change. */
    void applyCaptionDimming();

    // === Export image file chooser ===
    std::unique_ptr<juce::FileChooser> exportFileChooser;

    // === MIDI Import (drag-and-drop + menu) ===
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void handleMidiFileImport (const juce::File& midiFile);
    void applyMidiImport (const ovtdsp::PitchCurve& newCurve, const juce::String& sourceName);
    void showMidiChannelDialog (const juce::File& midiFile, const ovtdsp::MidiImportInfo& info);
    std::unique_ptr<juce::FileChooser> midiImportFileChooser;

    // Hamburger menu (gear icon button, replaces all top-bar controls).
    ebs::IconButton menuButton { ebs::IconButton::Shape::settings };
    // Screen position for right-click triggered wrench menu (0,0 = use menuButton position).
    juce::Point<int> pendingMenuScreenPos;

    // Harmony controls
    ovt::PowerToggleCaps harmonyEnableButton;
    juce::ComboBox    harmonyTypeBox;
    juce::Slider      harmonyGainSlider;
    juce::Slider      harmonyBlendSlider;
    juce::Slider      harmonyAttackSlider;   // Per-voice harmony fade-in duration (ms)
    juce::Label       harmonyGainLabel;
    juce::Label       harmonyBlendLabel;
    juce::Label       harmonyAttackLabel;
    juce::Label       harmonyTypeLabel;
    // Use voice: shared iOS-style sliding switch, plus the label it needs
    // (the switch paints only the pill, so the caption is its own component).
    juce::Label       useVoiceLabel;
    ebs::ToggleSwitch useVoiceButton;
    juce::ComboBox    shiftedVoicesBox;
    juce::ComboBox    harmonyToneBox;
    juce::Slider      harmonyToneColorSlider;
    juce::Label       harmonyToneColorLabel;
    ovt::PowerToggleCaps harmonyFollowLeadButton; // Harmony voices follow the lead correction character
    ovt::PowerToggleCaps harmonyGainMatchButton; // Scale harmony by 1/sqrt(1+N) to keep total RMS ~ dry
    juce::Slider      harmonyFormantSlider;   // Formant shift for harmony voices only
    juce::Label       harmonyFormantLabel;
    // Doubler knob: stereo doubling of the corrected lead (detune + Haas offset
    // + spread). Independent of harmony_enable, so it is never greyed out.
    // The power toggle is a UI mirror of double_amount > 0 (no extra parameter):
    // clicking it writes 0 % / the last used amount, exactly like the sibling
    // effects' enable buttons in this block.
    // Doubler: one knob, so its dot is carried by the knob (see the Effects block note below).
    ebs::ActivityKnob  doubleAmountSlider;
    ovt::PowerToggleCaps  doubleEnableButton;

    // De-Esser: sibilance suppressor for the corrected lead voice. One power
    // switch (deess_enable) and ONE knob (deess_amount, intensity 0..1): that knob
    // drives the detector threshold and the depth cap together, and the band centre
    // follows voice_type, the register the user already sets for pitch detection.
    // Switching off greys the knob and keeps its value, exactly like the reverb
    // amounts behind their voice switch.
    ovt::PowerToggleCaps deessEnableButton;
    // De-Esser: one knob, so its dot is carried by the knob.
    ebs::ActivityKnob  deessAmountSlider;        // intensity (0-1)

    // Saturation: a soft tanh drive on the master output, after the formant stage and
    // before the reverb return, so the tail stays clean. One power switch
    // (saturation_enable), one drive knob (saturation_drive 0..1) and one warmth knob
    // (saturation_tone 0..1 = a warm low-pass at 8 kHz .. 5 kHz). The name is drive /
    // warmth, never "tape": the stage is a soft clipper with a warm pole, not an emulation.
    // Drive 0 is a transparent bypass, so the knob at zero changes nothing.
    ovt::PowerToggleCaps saturationEnableButton;
    // Saturation: its dot rides the Drive knob, the control that IS the effect (see the dots note).
    ebs::ActivityKnob  saturationDriveSlider;     // drive (0-1)
    juce::Slider       saturationToneSlider;      // warmth (0-1)
    juce::Label        saturationDriveLabel;       // "Drive" caption above the drive knob
    juce::Label        saturationToneLabel;        // "Warmth" caption above the warmth knob

    // Delay: a tempo-synced ping-pong delay on the master output, after saturation and
    // before the limiter so the copies ride the same tail as the dry signal. One power
    // switch (delay_enable), one division choice (delay_division, note values from 1/8
    // dotted to 1/2, synced to the host tempo), one mix knob (delay_mix 0..1) and one
    // feedback knob (delay_feedback 0..1 = how many pings the tail keeps). Switching off
    // greys the choice and the two knobs and keeps their values, exactly like saturation
    // does with its pair. The ARA source panel reads the host tempo already, so the
    // division is a musical note, never a raw millisecond dial.
    ovt::PowerToggleCaps delayEnableButton;
    // The note value is a DETENTED small knob, not a combo: the parameter is an AudioParameterChoice
    // (0..4 step 1), so a rotary SliderAttachment snaps to the five values and the popup reads them
    // as fractions. That frees the 22 px the combo strip occupied for the Mix knob above it.
    ebs::Knob        delayDivisionKnob;           // note value of one ping (host-synced)
    // Delay: its dot rides the Mix knob, the amount the echo contributes.
    ebs::ActivityKnob delayMixSlider;            // dry/wet of the delay bus (0-1)
    juce::Slider     delayFeedbackSlider;         // feedback amount (0-1)
    juce::Label      delayDivisionLabel;          // "Division" caption above the knob
    juce::Label      delayMixLabel;               // "Mix" caption above the mix knob
    juce::Label      delayFeedbackLabel;          // "Feedback" caption above the feedback knob

    // Reverb bus controls: TWO SHARED ROOMS (ROOM + HALL), four sends, one sidechain
    // ducker. Each voice column is one power switch carrying its name (Lead / Harmony)
    // with the two amounts that voice sends under it: Room (short) and Hall (long). The
    // knob IS the send -- there is no separate send control and the dry voice is never
    // attenuated -- and the switch keeps the knobs' values, so switching a voice off and
    // back on needs no re-dial. The knobs start at the "ready to sing" amounts from
    // ReverbTuning.h, so the FIRST switch-on already applies a balanced pair.
    ovt::PowerToggleCaps   leadReverbEnableButton;
    // BOTH amounts of a voice carry the dot: they watch the same path (the voice being sent),
    // and showing it on one knob only would make the other look like it carries no signal.
    // Two dots reporting one path is honest here -- each knob is a send of that same voice.
    // Their captions run VERTICALLY along the dial's left edge (ovt::VerticalCaption), the frame
    // spines' typography: the 14 px a horizontal caption took above each dial go back to the dial,
    // so the four amounts reach the same 54 px the Effects block uses instead of 49 px.
    ebs::ActivityKnob  leadHallSlider;      // lead -> HALL
    ebs::ActivityKnob  leadRoomSlider;      // lead -> ROOM
    ovt::VerticalCaption leadRoomLabel;     // "Room", rotated bottom-to-top
    ovt::VerticalCaption leadHallLabel;     // "Hall", rotated bottom-to-top
    ovt::PowerToggleCaps   harmonyReverbEnableButton;
    ebs::ActivityKnob  harmonyHallSlider;   // harmony -> HALL
    ebs::ActivityKnob  harmonyRoomSlider;   // harmony -> ROOM
    ovt::VerticalCaption harmonyRoomLabel;
    ovt::VerticalCaption harmonyHallLabel;

    // Sidechain ducker on the two rooms' returns: the key is the corrected lead, never
    // the noise gate. One knob, no switch: depth 0 is no ducking at all, so the amount IS
    // the switch, and it is one control for both rooms (a split per room would be a second
    // knob for a distinction almost nobody makes). Its caption stays HORIZONTAL above the
    // dial: the ducker has no power switch, so it is not one of the amounts a switch sends.
    juce::Slider       duckDepthSlider;
    // The Ducking depth is a frame caption like the ROOM and HALL sends beside it: tracked
    // capitals in the spines' typography, not a mixed-case label bolted above a knob.
    ovt::HorizontalCaption duckDepthLabel;

    // Output stage: trim is the pre-limiter gain (-12..+12 dB), the limiter is the
    // true-peak guard that runs last on the master bus, looking ahead by its declared
    // 8-sample group delay.
    ovt::PowerToggleCaps limiterEnableButton;
    // Output Limit: one knob, so its dot is carried by the knob.
    ebs::ActivityKnob  outputTrimSlider;
    juce::Label        outputTrimLabel;

    // The meter sits in the SAME cell as the trim knob, directly under it: the peak that
    // leaves the stage, the gain reduction the limiter is applying, and the trim the user
    // set. It is fed by the editor timer from the processor's atomic snapshot, so nothing
    // is ever painted from the audio thread.
    ebs::LevelMeter outputMeter;

    // Every activity dot in the editor is now carried by the knob of the component it reports
    // (ebs::ActivityKnob): there is no strip left anywhere in the row. The Ducking column is a
    // return ramp, not a voice, so it carries none.

    // Noise Gate
    ovt::PowerToggleCaps noiseGateEnableButton;
    // Noise Gate: one knob, so its dot is carried by the knob.
    ebs::ActivityKnob noiseGateThresholdSlider;
    juce::Label noiseGateThresholdLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> noiseGateEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseGateThresholdAttachment;

    // Upward Compressor (input, before tuning)
    ovt::PowerToggleCaps upwardCompEnableButton;
    // Upward Compressor: one knob, so its dot is carried by the knob.
    ebs::ActivityKnob  upwardCompAmountSlider;
    juce::Label        upwardCompAmountLabel;

    // Effects frame: the effect cells (Gate, Compressor, Doubler, Formant, Output Limit + meter, De-Esser, Saturation)
    // live inside this ebs::ExpansibleFrame. Closed it shows TWO columns of two cells - Gate over Compressor in column 1,
    // Doubler over Formant in column 2: the input-stage dials you set while singing. The chevron on its right edge opens
    // TWO extra columns (Output Limit + meter over De-Esser as column 3, Saturation as column 4), which the editor answers
    // by re-laying out the bottom row. The block grows by COLUMN, never by adding a third row, and folding hides the
    // limiter's control, not its processing. The two rooms and their sends are NOT here: they live in reverbFrame.
    ebs::ExpansibleFrame effectsFrame;
    // Reverb frame: the two shared rooms. Folded it is ONE column - the LEAD voice switch with its Room and Hall
    // amounts under it - and that column stays visible in BOTH states, because it is what you dial while singing.
    // The chevron opens the frame into THREE columns: the Lead column, the Harmony column (its own switch and its
    // two amounts), and the sidechain Ducking alone on the right. The columns are sized by reverbHeaderNeed(), the
    // measured width of the switch names in the current language. The two rooms are what the four amounts feed:
    // there is no per-room enable, because a send at zero is silent, and no separate send control, because the
    // amount IS the send.
    ebs::ExpansibleFrame reverbFrame;
    // The Correction block is the same component: one chevron, one fold flag, one
    // host re-layout. Both bottom-row frames are ExpansibleFrame, not two bespoke handles.
    ebs::ExpansibleFrame correctionFrame;
    // Root/Scale is a plain framed body (ebs::FramedBody): the same frame language as the
    // ExpansibleFrames, with no fold. Every bottom-row block is a component, so it repaints
    // itself when the row re-lays out.
    ebs::FramedBody scaleFrame;
    // Harmony is the third ebs::ExpansibleFrame: the chevron on its right edge folds away the two knob columns (Volume
    // over Formant, Blend over Attack) and the frame narrows to its left column, giving that room back to the band. The
    // enable button, the type combo, the voice selectors and the two power toggles stay live in both states - a folded
    // block hides what you dial, not what you choose. Both zones ride the band grid: the open knobs go through
    // placeLabelledKnob() exactly like every other labelled cell, so they share the row split, label strip, gap and knob
    // diameter with Effects and Reverb, and in the left column the height the rows do not use becomes their spacing.
    ebs::ExpansibleFrame harmonyFrame;
    // The bottom row is one ebs::FrameBand: every block states the width its content asks
    // for and the floor below which its widgets collide. Spare room goes only to blocks
    // that can use it, and below the hard floor the row scrolls instead of stretching the
    // last block. Registration order IS left to right; the band sets every frame bounds.
    ebs::FrameBand bottomBand { 10 };
    // Every bottom-row frame carries a left title spine naming its block. Each band is a child of
    // the frame it names, so it paints inside that frame's rounded corner and moves with it when
    // the row re-lays out. It costs the frame WIDTH (ebs::FrameTitleBand::bandWidth(), fixed for
    // every locale); a longer translated name pays in length along the spine, never in width.
    ebs::FrameTitleBand correctionTitleBand;
    ebs::FrameTitleBand scaleTitleBand;
    ebs::FrameTitleBand effectsTitleBand;
    ebs::FrameTitleBand reverbTitleBand;
    ebs::FrameTitleBand harmonyTitleBand;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> upwardCompEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upwardCompAmountAttachment;

    // 2026-07-24 (Deprecation): FlexTune and Attack-Aware features are
    // temporarily disabled because of persistent audio artefacts (pops,
    // clicks, warble) at small buffer sizes that could not be fully
    // eliminated after 8 successive fixes (AY, AZ, BA, BB, BC, ...).
    // The DSP code (AttackAwareEnv, FlexTune smoother) is preserved in
    // Source/dsp/ for future re-implementation. The UI elements are
    // kept as members but are not visible / not attached to parameters.
    juce::Slider      flexTuneSlider;       // DEPRECATED: hidden, no attachment
    juce::Label       flexTuneLabel;        // DEPRECATED: hidden
    juce::Slider      humanizeSlider;
    juce::Label       humanizeLabel;
    juce::Slider      vibratoPreserveSlider;
    juce::Label       vibratoPreserveLabel;
    juce::ToggleButton attackAwareButton;   // DEPRECATED: hidden, no attachment
    juce::Slider      attackReleaseSlider;  // DEPRECATED: hidden, no attachment
    juce::Label       attackReleaseLabel;   // DEPRECATED: hidden
    juce::TextButton  correctionModeButton;

    // Graphic Modern/Transparent console switch, placed below the Speed/Amount
    // knobs. Backed by correctionModeButton so the existing undo + parameter
    // wiring is reused unchanged.
    std::unique_ptr<CorrectionModeSwitch> modeSwitch;

    // iPhone-style Live / Curve Editor tab switch, replacing the standard
    // Live / Curve Editor tab switch, replacing the standard TabbedButtonBar:
    std::unique_ptr<ebs::PillTabSwitch> tabSwitch;

    // Curve Editor "Options" button: icon-only (hamburger) with a distinct
    // accent-tinted background so it stands out from the neutral zoom/scroll/
    // snap buttons in this section. The plugin's own options keep the gear.
    ebs::IconButton optionsButton { ebs::IconButton::Shape::wand };
    // Preset Gallery toolbar button (icon-only grid, opens the browsable gallery).
    ebs::IconButton presetGalleryButton { ebs::IconButton::Shape::grid };
    juce::DrawableButton snapButton {"Snap to scale", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton snapGridButton {"Snap to grid", juce::DrawableButton::ImageOnButtonBackground};
    juce::DrawableButton stepModeButton {"Step mode", juce::DrawableButton::ImageOnButtonBackground};
    // Curve editor view controls (mirror the Visualizer toolbar: zoom/scroll/reset).
    ebs::IconButton zoomInButton { ebs::IconButton::Shape::zoomIn };
    ebs::IconButton zoomOutButton { ebs::IconButton::Shape::zoomOut };
    ebs::IconButton scrollUpButton { ebs::IconButton::Shape::chevronUp };
    ebs::IconButton scrollDownButton { ebs::IconButton::Shape::chevronDown };
    ebs::IconButton resetViewButton { ebs::IconButton::Shape::cross };

    // "Measures" control (curve editor time window). Moved from the embedded editor
    // controls to the toolbar so it no longer covers the ruler.
    juce::Label measuresLabel;
    juce::ComboBox measuresComboBox;
    // Standalone tempo field (see the constructor): the register the timeline, the Curve Editor's
    // grid and the tempo-synced Delay read when no host tempo is available. Hidden in a DAW.
    juce::Label bpmLabel;
    // Standalone transport. A single play/pause toggle (Play icon when stopped,
    // Stop icon when playing) plus a "Return to start" (rewind) button.
    // Only meaningful when not driven by a DAW host.
    juce::DrawableButton playButton {"Play", juce::DrawableButton::ImageOnButtonBackground};
    ebs::IconButton rewindButton { ebs::IconButton::Shape::rewind };

    // Update checker / release notification.
    juce::TextButton updateButton { "Check updates" };
    std::shared_ptr<OpenVoxTunerUpdateCheckState> updateCheckState;

    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    // Auxiliary top-level windows are owned by the editor so they cannot
    // outlive it and remain registered in juce::Desktop during shutdown.
    std::unique_ptr<juce::DocumentWindow> debugWindow;
    std::unique_ptr<juce::DocumentWindow> galleryWindow;

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
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> harmonyEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> useVoiceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shiftedVoicesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonyToneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyToneColorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonyTypeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyBlendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyAttackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> harmonyFollowLeadAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> harmonyGainMatchAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyFormantAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> doubleAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> doubleEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deessEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> deessAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> saturationEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationDriveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationToneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> delayEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayDivisionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayMixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayFeedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> latencyModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    // Shared reverb buses: one switch and two send amounts per voice, plus the ducker depth.
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> leadReverbEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> harmonyReverbEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> leadRoomSendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> leadHallSendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyRoomSendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmonyHallSendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> duckDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputTrimAttachment;
    // 2026-07-24 (Deprecation): FlexTune and Attack-Aware attachments are
    // disabled. The APVTS parameters still exist (for preset compatibility)
    // but no longer have a UI control attached.
    // std::unique_ptr<...::SliderAttachment> flexTuneAttachment;       // DEPRECATED
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> humanizeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vibratoPreserveAttachment;
    // std::unique_ptr<...::ButtonAttachment> attackAwareAttachment;    // DEPRECATED
    // std::unique_ptr<...::SliderAttachment> attackReleaseAttachment;  // DEPRECATED
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

    // One-time flag for syncing editor controls from persisted parameters
    bool measuresSyncDone = false;

    // Last waveform display type synced to visualizer/curve editor.
    int lastWaveformDisplayType = -1;
    // Last ARA chord root drawn in the header badge (-999 = none). The badge is
    // repainted only when this changes (timerCallback), so the banner is not
    // redrawn every tick.
    int lastAraChordBadgeRoot = -999;
    // Last "chord out of scale" state drawn in the badge (drives the warning
    // colour). Tracked separately so the badge recolours when the override
    // toggles even if the root stays the same.
    bool lastAraChordBadgeOutOfScale = false;
    // Last live chord symbol drawn in the badge (non-ARA ovtchord detection).
    // The badge is repainted only when this changes.
    juce::String lastLiveChordBadgeSymbol;
    // Tracks the last Harmony enable state seen by the timer so we can
    // re-run refreshLabels() (which disables the Follow Lead / Gain Match /
    // Use Voice sub-toggles) when Harmony changes via preset load or DAW
    // automation - paths that bypass the button's onStateChange callback.
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
    ebs::IconButton undoButton { ebs::IconButton::Shape::undo };
    ebs::IconButton redoButton { ebs::IconButton::Shape::redo };

    void performGlobalUndo();
    void performGlobalRedo();
    void commitUndoGesture (const juce::ValueTree& before);
    void registerUndoGestureListeners();
    // Wrap a programmatic state change (preset load) in one undo transaction.
    void pushUndoAroundCall (std::function<void()> fn);

    // === Plugin Presets (separate from Curve Presets) ===
    // Centred selector in the top banner: [<] [Combo: name] [>] [save].
    ebs::IconButton presetPrevButton { ebs::IconButton::Shape::chevronLeft };
    ebs::IconButton presetNextButton { ebs::IconButton::Shape::chevronRight };
    ebs::IconButton presetSaveButton { ebs::IconButton::Shape::save };
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
    double lastHarmonyTransportTime = 0.0; // for DAW transport jump detection (harmony traces)
    juce::String morphSourceName = "Source";
    juce::String morphTargetName = "Target";
    std::unique_ptr<ovtdsp::MorphState> morphUndoState; // pre-morph snapshot for undo

    // Tracks the normalized value the morph last applied to each parameter.
    // Used to detect parameters that are being driven externally (DAW/UI
    // automation): if a parameter's live value differs from this baseline, the
    // morph skips it so concurrent automation lanes (e.g. speed/amount) are not
    // overwritten by the morph crossfade.
    std::map<juce::String, float> lastMorphIntendedValues;

    // Diagnostic: one-shot startup state dump (OVT_FORCE_LOG) to verify what is
    // restored on launch (active slot, harmony_type param vs stored slot value).
    bool startupDiagLogged = false;

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
                // Active slot: solid theme accent background + bright text.
                g.setColour (ebs::accent());
                g.fillRoundedRectangle (bounds.reduced (1.0f), 3.0f);
                g.setColour (juce::Colours::white);
            }
            else
            {
                // Inactive slot: panel background + green border if filled.
                g.setColour (ebs::bgPanel());
                g.fillRoundedRectangle (bounds.reduced (1.0f), 3.0f);
                g.setColour (hasValidData ? juce::Colour (0xff4CAF50) : ebs::panelBorder());
                g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.5f);
                g.setColour (ebs::textDim());
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



