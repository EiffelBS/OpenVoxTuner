// OVTLanguages.h
// Modular internationalization (i18n) system for OpenVoxTuner.
// To add a new language: add a new map in the languages() function
// and a new Language enum value.

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>

namespace ovt
{
    /** Supported languages. */
    enum class Language { English, French, German, Spanish, Japanese };

    /** Get/set the current active language. */
    inline Language& currentLanguage()
    {
        static Language lang = Language::English;
        return lang;
    }

    /** Get the current language code (e.g. "en", "fr"). */
    inline juce::String currentLangCode()
    {
        switch (currentLanguage())
        {
            case Language::English: return "en";
            case Language::French:  return "fr";
            case Language::German:  return "de";
            case Language::Spanish: return "es";
            case Language::Japanese:return "ja";
        }
        return "en";
    }

    /** Translation key constants. */
    namespace Keys
    {
        // Menu keys
        static const char* kMenuTheme          = "menu.theme";
        static const char* kMenuDarkTheme      = "menu.dark_theme";
        static const char* kMenuLightTheme     = "menu.light_theme";
        static const char* kMenuLanguage       = "menu.language";
        static const char* kMenuLatency        = "menu.latency";
        static const char* kMenuMidiOut        = "menu.midi_out";
        static const char* kMenuTuningType     = "menu.tuning_type";
        static const char* kMenuModern         = "menu.modern";
        static const char* kMenuTransparent    = "menu.transparent";
        static const char* kMenuPitchDetection = "menu.pitch_detection";
        static const char* kMenuCheckUpdates   = "menu.check_updates";
        static const char* kMenuResetDefault   = "menu.reset_default";
        static const char* kMenuBypass         = "menu.bypass";
        static const char* kMenuExportImage    = "menu.export_image";
        static const char* kMenuLowLatency     = "menu.low_latency";
        static const char* kMenuQuality        = "menu.quality";
        static const char* kMenuSafe           = "menu.safe";
        static const char* kMenuDirectMonitoring = "menu.direct_monitoring";
        static const char* kMenuYinActive      = "menu.yin_active";
        static const char* kMenuShowWaveform   = "menu.show_waveform";
        static const char* kMenuWaveformDisplay = "menu.waveform_display";
        static const char* kMenuWaveformLine   = "menu.waveform_line";
        static const char* kMenuWaveformMirror = "menu.waveform_mirror";
        static const char* kMenuMidiLearn      = "menu.midi_learn";
        static const char* kMenuKeyboardShortcuts = "menu.keyboard_shortcuts";
        static const char* kMenuConfirmReset   = "menu.confirm_reset";
        static const char* kMenuCancel         = "menu.cancel";
        static const char* kMenuDebugWindow    = "menu.debug_window";
        static const char* kMenuMorphSetSource = "menu.morph_set_source";
        static const char* kMenuMorphSetTargetA = "menu.morph_set_target_a";
        static const char* kMenuMorphSetTargetB = "menu.morph_set_target_b";
        static const char* kMenuMorphAtoB      = "menu.morph_a_to_b";
        static const char* kMenuMorphUndo      = "menu.morph_undo";
        static const char* kMenuMorphReset     = "menu.morph_reset";
        static const char* kMenuSavePresetAs   = "menu.save_preset_as";
        static const char* kMenuDeletePreset   = "menu.delete_preset";
        static const char* kMenuFactory        = "menu.factory";
        static const char* kMenuCustom         = "menu.custom";
        static const char* kMenuCleanCurves    = "menu.clean_curves";
        static const char* kMenuResetPlayhead  = "menu.reset_playhead";
        static const char* kMenuCurvePresets   = "menu.curve_presets";
        static const char* kMenuAutoScroll     = "menu.auto_scroll";
        static const char* kMenuTempo          = "menu.tempo";
        static const char* kTooltipCurveOptions = "tooltip.curve_options";
        static const char* kTooltipPlay        = "tooltip.play";
        static const char* kTooltipStop        = "tooltip.stop";
        static const char* kTooltipRewind      = "tooltip.rewind";

        // Tab keys
        static const char* kTabLive            = "tab.live";
        static const char* kTabCurveEditor     = "tab.curve_editor";

        // Label keys
        static const char* kLabelSpeed         = "label.speed";
        static const char* kLabelAmount        = "label.amount";
        static const char* kLabelScale         = "label.scale";
        static const char* kLabelRoot          = "label.root";
        static const char* kLabelVolume        = "label.volume";
        static const char* kLabelBlend         = "label.blend";
        static const char* kLabelFlex          = "label.flex";
        static const char* kLabelHumanize      = "label.humanize";
        static const char* kLabelFormant       = "label.formant";
        static const char* kLabelReverb        = "label.reverb";
        static const char* kLabelMix           = "label.mix";
        static const char* kLabelHarmony       = "label.harmony";
        static const char* kLabelCorrectionMode = "label.correction_mode";
        static const char* kLabelModern         = "label.modern";
        static const char* kLabelTransparent    = "label.transparent";
        static const char* kLabelBypass         = "label.bypass";
        static const char* kLabelMidiOut        = "label.midi_out";
        static const char* kLabelUseVoice       = "label.use_voice";
        static const char* kLabelSnap           = "label.snap";
        static const char* kLabelSnapGrid       = "label.snap_grid";
        static const char* kLabelStepMode       = "label.step_mode";
        static const char* kLabelClear          = "label.clear";
        static const char* kLabelPresets        = "label.presets";
        static const char* kLabelHelp           = "label.help";
        static const char* kLabelUpdates        = "label.updates";
        static const char* kUseVoice            = "label.use_voice";
        static const char* kLabelHarmonyBtn     = "label.harmony_btn";
        static const char* kLabelFormantBtn     = "label.formant_btn";
        static const char* kLabelNoiseGate     = "label.noise_gate";
        static const char* kTooltipNoiseGate   = "tooltip.noise_gate";
        static const char* kLabelThreshold     = "label.threshold";
        static const char* kTooltipThreshold   = "tooltip.threshold";
        static const char* kLabelReverbBtn      = "label.reverb_btn";
        static const char* kLabelTone           = "label.tone";
        static const char* kLabelModernBtn      = "label.modern_btn";
        static const char* kLabelTransparentBtn = "label.transparent_btn";
        static const char* kLabelBypassBtn      = "label.bypass_btn";
        static const char* kLabelMidiOutBtn     = "label.midi_out_btn";
        static const char* kLabelDebug          = "label.debug";
        static const char* kLabelAutoScroll     = "label.auto_scroll";
        static const char* kLabelMeasures       = "label.measures";
        static const char* kLabelMorph          = "label.morph";
        static const char* kLabelCpu            = "label.cpu";

        // Scale keys
        static const char* kScaleChromatic      = "scale.chromatic";
        static const char* kScaleMajor          = "scale.major";
        static const char* kScaleMelodicMinor   = "scale.melodic_minor";
        static const char* kScaleHarmonicMinor  = "scale.harmonic_minor";
        static const char* kScaleNaturalMinor   = "scale.natural_minor";
        static const char* kScaleMajorPentatonic = "scale.major_pentatonic";
        static const char* kScaleMinorPentatonic = "scale.minor_pentatonic";
        static const char* kScaleBlues          = "scale.blues";
        static const char* kScaleDorian         = "scale.dorian";
        static const char* kScalePhrygian       = "scale.phrygian";
        static const char* kScaleLydian         = "scale.lydian";
        static const char* kScaleMixolydian     = "scale.mixolydian";
        static const char* kScaleLocrian        = "scale.locrian";
        static const char* kScaleCustom         = "scale.custom";

        // Harmony keys
        static const char* kHarmonyNone            = "harmony.none";
        static const char* kHarmony3rdBelow        = "harmony.3rd_below";
        static const char* kHarmony3rdAbove        = "harmony.3rd_above";
        static const char* kHarmony3rdBelowAbove   = "harmony.3rd_below_above";
        static const char* kHarmony4thBelow        = "harmony.4th_below";
        static const char* kHarmony4thAbove        = "harmony.4th_above";
        static const char* kHarmony4thBelowAbove   = "harmony.4th_below_above";
        static const char* kHarmony5thBelow        = "harmony.5th_below";
        static const char* kHarmony5thAbove        = "harmony.5th_above";
        static const char* kHarmony5thBelowAbove   = "harmony.5th_below_above";
        static const char* kHarmony3rdBelow5thAbove = "harmony.3rd_below_5th_above";
        static const char* kHarmony5thBelow3rdAbove = "harmony.5th_below_3rd_above";
        static const char* kHarmonyOctaveBelow     = "harmony.octave_below";
        static const char* kHarmonyOctaveAbove     = "harmony.octave_above";
        static const char* kHarmonyOctaveBelowAbove = "harmony.octave_below_above";
        static const char* kHarmonyVocalStack3     = "harmony.vocal_stack_3";
        static const char* kHarmonyVocalStack4     = "harmony.vocal_stack_4";
        static const char* kHarmonyPowerChord      = "harmony.power_chord";
        static const char* kHarmonyParallel3rd     = "harmony.parallel_3rd";
        static const char* kHarmonyDrone           = "harmony.drone";
        static const char* kHarmonyUnison2         = "harmony.unison2";
        static const char* kHarmonyUnisonOctaves4  = "harmony.unison_octaves4";

        // Tooltip keys
        static const char* kTooltipZoomIn      = "tooltip.zoom_in";
        static const char* kTooltipZoomOut     = "tooltip.zoom_out";
        static const char* kTooltipScrollUp    = "tooltip.scroll_up";
        static const char* kTooltipScrollDown  = "tooltip.scroll_down";
        static const char* kTooltipResetView   = "tooltip.reset_view";
        static const char* kTooltipSnapToScale = "tooltip.snap_to_scale";
        static const char* kTooltipSnapToGrid  = "tooltip.snap_to_grid";
        static const char* kTooltipStepMode    = "tooltip.step_mode";
        static const char* kTooltipClearAll    = "tooltip.clear_all";
        static const char* kTooltipPresets     = "tooltip.presets";
        static const char* kTooltipBypass      = "tooltip.bypass";
        static const char* kTooltipMidiOut     = "tooltip.midi_out";
        static const char* kTooltipCorrection  = "tooltip.correction";
        static const char* kTooltipHarmonyEn   = "tooltip.harmony_enable";
        static const char* kTooltipReverbEn    = "tooltip.reverb_enable";
        static const char* kTooltipFormant     = "tooltip.formant_enable";
        static const char* kTooltipFlexTune    = "tooltip.flex_tune";
        static const char* kTooltipHumanize    = "tooltip.humanize";
        static const char* kTooltipSpeed       = "tooltip.speed";
        static const char* kTooltipAmount      = "tooltip.amount";
        static const char* kTooltipVolume      = "tooltip.volume";
        static const char* kTooltipBlend       = "tooltip.blend";
        static const char* kTooltipToneColor   = "tooltip.tone_color";
        static const char* kTooltipAB          = "tooltip.ab_comparison";
        static const char* kTooltipReset       = "tooltip.reset_transport";
        static const char* kTooltipCheckUpdates      = "tooltip.check_updates";
        static const char* kTooltipMorphDrag         = "tooltip.morph_drag";
        static const char* kTooltipMorphLabel        = "tooltip.morph_label";
        static const char* kTooltipResetTransportDetail = "tooltip.reset_transport_detail";
        static const char* kTooltipBypassIcon        = "tooltip.bypass_icon";
        static const char* kTooltipMidiOutIcon       = "tooltip.midi_out_icon";
        static const char* kTooltipMenuOptions       = "tooltip.menu_options";
        static const char* kTooltipDebugWindow       = "tooltip.debug_window";
        static const char* kTooltipUpdateAvailable   = "tooltip.update_available";
        static const char* kTooltipUpdateReleases    = "tooltip.update_releases";
        static const char* kTooltipAbSlotA           = "tooltip.ab_slot_a";
        static const char* kTooltipAbSlotB           = "tooltip.ab_slot_b";
        static const char* kTooltipAutoScroll        = "tooltip.auto_scroll";
        static const char* kTooltipUndo              = "tooltip.undo";
        static const char* kTooltipRedo              = "tooltip.redo";

        // Legend keys
        static const char* kLegendInput        = "legend.input";
        static const char* kLegendOutput       = "legend.output";
        static const char* kLegendHarmony      = "legend.harmony";
        static const char* kLegendScrollHint   = "legend.scroll_hint";
        static const char* kLegendZoomHint     = "legend.zoom_hint";
        static const char* kLegendInTune       = "legend.in_tune";

        // Status keys
        static const char* kStatusChecking       = "status.checking";
        static const char* kStatusUpdateAvailable = "status.update_available";
        static const char* kStatusUpToDate       = "status.up_to_date";
        static const char* kStatusUpdateFailed   = "status.update_failed";
        static const char* kStatusUpdatePrefix   = "status.update_prefix";

        // Help overlay keys
        static const char* kHelpTitle        = "help.title";
        static const char* kHelpCloseHint    = "help.close_hint";
        static const char* kHelpMouseWheel   = "help.mouse_wheel";
        static const char* kHelpCtrlWheel    = "help.ctrl_wheel";
        static const char* kHelpClickDrag    = "help.click_drag";
        static const char* kHelpDoubleClick  = "help.double_click";
        static const char* kHelpRightClick   = "help.right_click";
        static const char* kHelpCopy         = "help.copy";
        static const char* kHelpPaste        = "help.paste";
        static const char* kHelpDelete       = "help.delete";
        static const char* kHelpUndo         = "help.undo";
        static const char* kHelpRedo         = "help.redo";
        static const char* kHelpToggleHelp   = "help.toggle_help";

        // Dialog/Alert keys
        static const char* kDlgExport          = "dlg.export";
        static const char* kDlgExportPng       = "dlg.export_png";
        static const char* kDlgExportNotFound  = "dlg.export_not_found";
        static const char* kDlgImageSaved      = "dlg.image_saved";
        static const char* kDlgImageFailed     = "dlg.image_failed";
        static const char* kDlgSavePreset      = "dlg.save_preset";
        static const char* kDlgSavePresetDesc  = "dlg.save_preset_desc";
        static const char* kDlgSave            = "dlg.save";
        static const char* kDlgInvalidName     = "dlg.invalid_name";
        static const char* kDlgEmptyName       = "dlg.empty_name";
        static const char* kDlgOverwrite       = "dlg.overwrite";
        static const char* kDlgOverwriteDesc   = "dlg.overwrite_desc";
        static const char* kDlgOverwriteBtn    = "dlg.overwrite_btn";
        static const char* kDlgPresetSaved     = "dlg.preset_saved";
        static const char* kDlgPresetSavedDesc = "dlg.preset_saved_desc";
        static const char* kDlgSaveFailed      = "dlg.save_failed";
        static const char* kDlgSaveFailedDesc  = "dlg.save_failed_desc";
        static const char* kDlgDeletePreset    = "dlg.delete_preset";
        static const char* kDlgDeletePresetDesc = "dlg.delete_preset_desc";
        static const char* kDlgDelete          = "dlg.delete";
        static const char* kDlgDeleteFailed    = "dlg.delete_failed";
        static const char* kDlgDeleteFailedDesc = "dlg.delete_failed_desc";
        static const char* kDlgPresetDeleted    = "dlg.preset_deleted";
        static const char* kDlgPresetDeletedDesc = "dlg.preset_deleted_desc";
        static const char* kDlgMidiLearn       = "dlg.midi_learn";
        static const char* kDlgMidiLearnDesc   = "dlg.midi_learn_desc";
        static const char* kDlgOk              = "dlg.ok";
        static const char* kDlgDelete_         = "dlg.delete_";

        // MIDI Learn parameter name keys
        static const char* kMidiLearnSpeed         = "midi_learn.speed";
        static const char* kMidiLearnAmount        = "midi_learn.amount";
        static const char* kMidiLearnFormant       = "midi_learn.formant";
        static const char* kMidiLearnReverbMix     = "midi_learn.reverb_mix";
        static const char* kMidiLearnFlexTune      = "midi_learn.flex_tune";
        static const char* kMidiLearnHumanize      = "midi_learn.humanize";
        static const char* kMidiLearnHarmonyGain   = "midi_learn.harmony_gain";
        static const char* kMidiLearnHarmonyBlend  = "midi_learn.harmony_blend";
        static const char* kMidiLearnHarmonyTone   = "midi_learn.harmony_tone";

        // PitchCurveEditor hint keys
        static const char* kHintScrollZoom  = "hint.scroll_zoom";
        static const char* kHintZoom        = "hint.zoom";
        static const char* kHintAddPoint    = "hint.add_point";
        static const char* kHintLiveMode    = "hint.live_mode";
    }

    /** Get the translation map for a given language. */
    inline const std::unordered_map<std::string, juce::String>& getTranslations (Language lang)
    {
        using namespace Keys;

        static const std::unordered_map<std::string, juce::String> english = {
            { kMenuTheme,          "Theme" },
            { kMenuDarkTheme,      "Dark Theme" },
            { kMenuLightTheme,     "Light Theme" },
            { kMenuLanguage,       "Language" },
            { kMenuLatency,        "Latency" },
            { kMenuMidiOut,        "MIDI Out" },
            { kMenuTuningType,     "Tuning Type" },
            { kMenuModern,         "Modern" },
            { kMenuTransparent,    "Transparent" },
            { kMenuPitchDetection, "Pitch Detection" },
            { kMenuCheckUpdates,   "Check for Updates" },
            { kMenuResetDefault,   "Reset to Default" },
            { kMenuBypass,         "Bypass" },
            { kMenuExportImage,    "Export as Image..." },
            { kMenuLowLatency,     "Low Latency" },
            { kMenuQuality,        "Quality" },
            { kMenuSafe,           "Safe" },
            { kMenuDirectMonitoring, "Direct Monitoring" },
            { kMenuYinActive,      "YIN (active)" },
            { kMenuShowWaveform,   "Show Waveform" },
            { kMenuWaveformDisplay, "Waveform Display" },
            { kMenuWaveformLine,   "Line" },
            { kMenuWaveformMirror, "Mirror" },
            { kMenuMidiLearn,      "MIDI Learn" },
            { kMenuKeyboardShortcuts, "Keyboard Shortcuts (?)" },
            { kMenuConfirmReset,   "Confirm Reset" },
            { kMenuCancel,         "Cancel" },
            { kMenuDebugWindow,    "Debug Window" },
            { kMenuMorphSetSource, "Set Source (Current)" },
            { kMenuMorphSetTargetA, "Set Target from A/B Slot A" },
            { kMenuMorphSetTargetB, "Set Target from A/B Slot B" },
            { kMenuMorphAtoB,      "Morph A -> B" },
            { kMenuMorphUndo,      "Undo Morph" },
            { kMenuMorphReset,     "Reset Morph" },
            { kMenuSavePresetAs,   "Save Preset As..." },
            { kMenuDeletePreset,   "Delete..." },
            { kMenuFactory,        "Factory" },
            { kMenuCustom,         "Custom" },
            { kMenuCleanCurves,    "Clean Curves" },
            { kMenuResetPlayhead,  "Reset Playhead" },
            { kMenuCurvePresets,   "Curve Presets" },
            { kMenuAutoScroll,     "Auto-Scroll" },
            { kMenuTempo,          "Tempo" },
            { kTooltipCurveOptions, "Curve editor options" },
            { kTooltipPlay,        "Play (start the standalone timeline)" },
            { kTooltipStop,        "Stop (freeze the standalone timeline for editing)" },
            { kTooltipRewind,      "Return to start (reset the playhead to the beginning)" },
            { kTabLive,            "Live" },
            { kTabCurveEditor,     "Curve Editor" },
            { kLabelSpeed,         "Speed (ms)" },
            { kLabelAmount,        "Amount" },
            { kLabelScale,         "Scale" },
            { kLabelRoot,          "Root" },
            { kLabelVolume,        "Volume" },
            { kLabelBlend,         "Blend" },
            { kTooltipSpeed,        "Correction speed in milliseconds. Lower = faster pitch tracking." },
            { kTooltipAmount,       "Correction amount (0% = natural, 100% = fully corrected to target)." },
            { kTooltipVolume,       "Harmony voices output volume." },
            { kTooltipBlend,        "Balance between the lead vocal and the generated harmony voices." },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humanize" },
            { kLabelFormant,       "Formant" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Harmony" },
            { kLabelCorrectionMode, "Correction Mode" },
            { kLabelModern,         "Modern" },
            { kLabelTransparent,    "Transparent" },
            { kLabelBypass,         "Bypass" },
            { kLabelMidiOut,        "MIDI Out" },
            { kLabelUseVoice,       "Use Voice" },
            { kLabelSnap,           "Snap to Scale" },
            { kLabelSnapGrid,       "Snap to Grid" },
            { kLabelStepMode,       "Step Mode" },
            { kLabelClear,          "Clear" },
            { kLabelPresets,        "Presets" },
            { kLabelHelp,           "Help" },
            { kLabelUpdates,        "Updates" },
            { kUseVoice,            "Use Voice" },
            { kLabelHarmonyBtn,     "Harmony" },
            { kLabelFormantBtn,     "Formant" },
            { kLabelNoiseGate,   "Gate" },
            { kTooltipNoiseGate, "Enable/disable noise gate on input audio." },
            { kLabelThreshold,   "Threshold" },
            { kTooltipThreshold, "Noise gate threshold in dB. Signals below this level are muted." },
            { kLabelReverbBtn,      "Reverb" },
            { kLabelTone,           "Tone" },
            { kLabelModernBtn,      "Modern" },
            { kLabelTransparentBtn, "Transparent" },
            { kLabelBypassBtn,      "ByPass" },
            { kLabelMidiOutBtn,     "MIDI OUT" },
            { kLabelDebug,          "Debug" },
            { kLabelAutoScroll,     "Auto-Scroll" },
            { kLabelMeasures,       "Measures" },
            { kLabelMorph,          "Morph" },
            { kLabelCpu,            "CPU " },
            { kScaleChromatic,        "Chromatic" },
            { kScaleMajor,            "Major" },
            { kScaleMelodicMinor,     "Melodic Minor" },
            { kScaleHarmonicMinor,    "Harmonic Minor" },
            { kScaleNaturalMinor,     "Natural Minor" },
            { kScaleMajorPentatonic,  "Major Pentatonic" },
            { kScaleMinorPentatonic,  "Minor Pentatonic" },
            { kScaleBlues,            "Blues" },
            { kScaleDorian,           "Dorian" },
            { kScalePhrygian,         "Phrygian" },
            { kScaleLydian,           "Lydian" },
            { kScaleMixolydian,       "Mixolydian" },
            { kScaleLocrian,          "Locrian" },
            { kScaleCustom,           "Custom" },
            { kHarmonyNone,            "None" },
            { kHarmony3rdBelow,        "3rd Below" },
            { kHarmony3rdAbove,        "3rd Above" },
            { kHarmony3rdBelowAbove,   "3rd Below + Above" },
            { kHarmony4thBelow,        "4th Below" },
            { kHarmony4thAbove,        "4th Above" },
            { kHarmony4thBelowAbove,   "4th Below + Above" },
            { kHarmony5thBelow,        "5th Below" },
            { kHarmony5thAbove,        "5th Above" },
            { kHarmony5thBelowAbove,   "5th Below + Above" },
            { kHarmony3rdBelow5thAbove, "3rd Below + 5th Above" },
            { kHarmony5thBelow3rdAbove, "5th Below + 3rd Above" },
            { kHarmonyOctaveBelow,     "Octave Below" },
            { kHarmonyOctaveAbove,     "Octave Above" },
            { kHarmonyOctaveBelowAbove, "Octave Below + Above" },
            { kHarmonyVocalStack3,     "Vocal Stack (3 voices)" },
            { kHarmonyVocalStack4,     "Vocal Stack (4 voices)" },
            { kHarmonyPowerChord,      "Power Chord" },
            { kHarmonyParallel3rd,     "Parallel 3rd" },
            { kHarmonyDrone,           "Drone" },
            { kHarmonyUnison2,         "Unison (2 voices)" },
            { kHarmonyUnisonOctaves4,  "Unison + Octaves (4 voices)" },
            { kTooltipZoomIn,      "Zoom In (narrower range)" },
            { kTooltipZoomOut,     "Zoom Out (wider range)" },
            { kTooltipScrollUp,    "Scroll Up (higher pitches)" },
            { kTooltipScrollDown,  "Scroll Down (lower pitches)" },
            { kTooltipResetView,   "Reset Zoom and Scroll" },
            { kTooltipSnapToScale,    "Snap curve points to scale notes" },
            { kTooltipSnapToGrid,     "Snap curve points to beat grid" },
            { kTooltipStepMode,       "Staircase interpolation between notes" },
            { kTooltipClearAll,       "Clear all curve points" },
            { kTooltipPresets,        "Manage curve presets" },
            { kTooltipBypass,         "Bypass audio processing." },
            { kTooltipMidiOut,        "Enable MIDI Out" },
            { kTooltipCorrection,     "Modern = aggressive correction. Transparent = gentler, preserves transitions." },
            { kTooltipHarmonyEn,      "Enable/disable harmony generation." },
            { kTooltipReverbEn,       "Enable/disable reverb effect." },
            { kTooltipFormant,        "Enable/disable formant shifting." },
            { kTooltipFlexTune,       "Deadband in cents: input pitch within this range is left uncorrected." },
            { kTooltipHumanize,       "Random pitch fluctuations in cents, added when correction is applied." },
            { kTooltipToneColor,      "Tone color for synth harmonies." },
            { kTooltipAB,             "A/B: Click to toggle between slot A and B. Right-click to save current state." },
            { kTooltipReset,          "Reset playhead." },
            { kTooltipCheckUpdates,   "Check the latest OpenVoxTuner release on GitHub." },
            { kTooltipMorphDrag,      "Drag to morph between slot A (left) and slot B (right).\nDouble-click to snap to 50%.\nRight-click for options." },
            { kTooltipMorphLabel,     "Morph between two plugin states. Click A or B to set slots, then drag the slider." },
            { kTooltipResetTransportDetail, "Reset playhead.\nResets the internal timeline offset (useful in Standalone / classic VST3)." },
            { kTooltipBypassIcon,     "Bypass audio processing.\nWhen enabled, audio passes through without correction." },
            { kTooltipMidiOutIcon,    "Enable MIDI Out" },
            { kTooltipMenuOptions,    "OpenVoxTuner options" },
            { kTooltipDebugWindow,    "Open debug window: MIDI log, attack/release testing" },
            { kTooltipUpdateAvailable, "Open the latest OpenVoxTuner release." },
            { kTooltipUpdateReleases, "Open the OpenVoxTuner releases page." },
            { kTooltipAbSlotA,        "Click: load slot A. Right-click: save current state." },
            { kTooltipAbSlotB,        "Click: load slot B. Right-click: save current state." },
            { kTooltipAutoScroll,     "Automatically scroll the editor view during playback" },
            { kTooltipUndo,           "Undo (Ctrl+Z)" },
            { kTooltipRedo,           "Redo (Ctrl+Y)" },
            { kLegendInput,           "Input" },
            { kLegendOutput,          "Output" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Wheel: Scroll" },
            { kLegendZoomHint,        "Ctrl+Whl: Zoom" },
            { kLegendInTune,          "in tune" },
            { kStatusChecking,        "Checking..." },
            { kStatusUpdateAvailable, "Update available" },
            { kStatusUpToDate,        "Up to date" },
            { kStatusUpdateFailed,    "Update check failed" },
            { kStatusUpdatePrefix,    "Update " },
            { kHelpTitle,             "Keyboard Shortcuts & Mouse Controls" },
            { kHelpCloseHint,         "Click anywhere to close" },
            { kHelpMouseWheel,        "Scroll vertically (pitch)" },
            { kHelpCtrlWheel,         "Zoom in/out" },
            { kHelpClickDrag,         "Move curve points" },
            { kHelpDoubleClick,       "Add new curve point" },
            { kHelpRightClick,        "Delete curve point" },
            { kHelpCopy,              "Copy selected points" },
            { kHelpPaste,             "Paste copied points" },
            { kHelpDelete,            "Delete selected points" },
            { kHelpUndo,              "Undo" },
            { kHelpRedo,              "Redo" },
            { kHelpToggleHelp,        "Toggle this help overlay" },
            { kDlgExport,             "Export" },
            { kDlgExportPng,          "Export Visualizer as PNG" },
            { kDlgExportNotFound,     "Could not find the visualizer component." },
            { kDlgImageSaved,         "Image saved to:\n" },
            { kDlgImageFailed,        "Failed to save image." },
            { kDlgSavePreset,         "Save Preset" },
            { kDlgSavePresetDesc,     "Save the current Curve Editor configuration as a custom preset." },
            { kDlgSave,               "Save" },
            { kDlgInvalidName,        "Invalid name" },
            { kDlgEmptyName,          "Preset name can't be empty." },
            { kDlgOverwrite,          "Overwrite preset?" },
            { kDlgOverwriteDesc,      "A preset with this name already exists.\nOverwrite it?" },
            { kDlgOverwriteBtn,       "Overwrite" },
            { kDlgPresetSaved,        "Preset saved" },
            { kDlgPresetSavedDesc,    "Saved custom preset:\n" },
            { kDlgSaveFailed,         "Save failed" },
            { kDlgSaveFailedDesc,     "Couldn't write the preset file." },
            { kDlgDeletePreset,       "Delete preset?" },
            { kDlgDeletePresetDesc,   "Delete this custom preset permanently?\n" },
            { kDlgDelete,             "Delete" },
            { kDlgDeleteFailed,       "Delete failed" },
            { kDlgDeleteFailedDesc,   "Couldn't delete the preset file. Check permissions." },
            { kDlgPresetDeleted,      "Preset deleted" },
            { kDlgPresetDeletedDesc,  "Deleted custom preset:\n" },
            { kDlgMidiLearn,          "MIDI Learn" },
            { kDlgMidiLearnDesc,      "Move a MIDI controller to assign it to this parameter.\nPress Escape to cancel." },
            { kDlgOk,                 "OK" },
            { kDlgDelete_,            "Delete" },
            { kMidiLearnSpeed,        "Speed" },
            { kMidiLearnAmount,       "Amount" },
            { kMidiLearnFormant,      "Formant" },
            { kMidiLearnReverbMix,    "Reverb Mix" },
            { kMidiLearnFlexTune,     "FlexTune" },
            { kMidiLearnHumanize,     "Humanize" },
            { kMidiLearnHarmonyGain,  "Harmony Gain" },
            { kMidiLearnHarmonyBlend, "Harmony Blend" },
            { kMidiLearnHarmonyTone,  "Harmony Tone" },
            { kHintScrollZoom,        "MouseWheel: Scroll | " },
            { kHintZoom,              "+MouseWheel: Zoom" },
            { kHintAddPoint,          "Double-click: Add point | Right-click: Curve presets" },
            { kHintLiveMode,          "Live Mode : switch to Curve Editor to edit" },
        };

        static const std::unordered_map<std::string, juce::String> french = {
            { kMenuTheme,          "Theme" },
            { kMenuDarkTheme,      "Theme sombre" },
            { kMenuLightTheme,     "Theme clair" },
            { kMenuLanguage,       "Langue" },
            { kMenuLatency,        "Latence" },
            { kMenuMidiOut,        "Sortie MIDI" },
            { kMenuTuningType,     "Type d'accordage" },
            { kMenuModern,         "Moderne" },
            { kMenuTransparent,    "Transparent" },
            { kMenuPitchDetection, "Detection de hauteur" },
            { kMenuCheckUpdates,   "Verifier les mises a jour" },
            { kMenuResetDefault,   "Reinitialiser par defaut" },
            { kMenuBypass,         "Bypass" },
            { kMenuExportImage,    "Exporter en image..." },
            { kMenuLowLatency,     "Basse latence" },
            { kMenuQuality,        "Qualite" },
            { kMenuSafe,           "Securise" },
            { kMenuDirectMonitoring, "Surveillance directe" },
            { kMenuYinActive,      "YIN (actif)" },
            { kMenuShowWaveform,   "Afficher la forme d'onde" },
            { kMenuWaveformDisplay, "Affichage de la forme d'onde" },
            { kMenuWaveformLine,   "Ligne" },
            { kMenuWaveformMirror, "Miroir" },
            { kMenuMidiLearn,      "Apprentissage MIDI" },
            { kMenuKeyboardShortcuts, "Raccourcis clavier (?)" },
            { kMenuConfirmReset,   "Confirmer la reinitialisation" },
            { kMenuCancel,         "Annuler" },
            { kMenuDebugWindow,    "Fenetre de debug" },
            { kMenuMorphSetSource, "Definir la source (actuel)" },
            { kMenuMorphSetTargetA, "Definir la cible depuis le slot A" },
            { kMenuMorphSetTargetB, "Definir la cible depuis le slot B" },
            { kMenuMorphAtoB,      "Morpher A -> B" },
            { kMenuMorphUndo,      "Annuler le morph" },
            { kMenuMorphReset,     "Reinitialiser le morph" },
            { kMenuSavePresetAs,   "Sauvegarder le preset sous..." },
            { kMenuDeletePreset,   "Supprimer..." },
            { kMenuFactory,        "Usine" },
            { kMenuCustom,         "Personnalise" },
            { kMenuCurvePresets,   "Presets de courbe" },
            { kMenuAutoScroll,     "Auto-Scroll" },
            { kMenuTempo,          "Tempo" },
            { kTooltipCurveOptions, "Options de l'editeur de courbe" },
            { kTooltipPlay,        "Lecture (demarrer la ligne temporelle standalone)" },
            { kTooltipStop,        "Stop (figer la ligne temporelle standalone pour l'edition)" },
            { kTooltipRewind,      "Retour au debut (reinitialiser le playhead)" },
            { kTabLive,            "Live" },
            { kTabCurveEditor,     "Editeur de courbe" },
            { kLabelSpeed,         "Vitesse (ms)" },
            { kLabelAmount,        "Intensite" },
            { kLabelScale,         "Gamme" },
            { kLabelRoot,          "Tonalite" },
            { kLabelVolume,        "Volume" },
            { kLabelBlend,         "Mixage" },
            { kTooltipSpeed,        "Vitesse de correction en millisecondes. Plus bas = suivi de hauteur plus rapide." },
            { kTooltipAmount,       "Intensite de correction (0 % = naturel, 100 % = entierement corrige vers la cible)." },
            { kTooltipVolume,       "Volume de sortie des voix d'harmonie." },
            { kTooltipBlend,        "Equilibre entre la voix principale et les voix d'harmonie generees." },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humaniser" },
            { kLabelFormant,       "Formant" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Harmonie" },
            { kLabelCorrectionMode, "Mode correction" },
            { kLabelModern,         "Moderne" },
            { kLabelTransparent,    "Transparent" },
            { kLabelBypass,         "Bypass" },
            { kLabelMidiOut,        "Sortie MIDI" },
            { kLabelUseVoice,       "Utiliser voix" },
            { kLabelSnap,           "Aligner a la gamme" },
            { kLabelSnapGrid,       "Aligner a la grille" },
            { kLabelStepMode,       "Mode marche" },
            { kLabelClear,          "Effacer" },
            { kLabelPresets,        "Presets" },
            { kLabelHelp,           "Aide" },
            { kLabelUpdates,        "Mises a jour" },
            { kUseVoice,            "Utiliser voix" },
            { kLabelHarmonyBtn,     "Harmonie" },
            { kLabelFormantBtn,     "Formant" },
            { kLabelNoiseGate,   "Porte" },
            { kTooltipNoiseGate, "Activer/desactiver la porte de bruit sur l'entree audio." },
            { kLabelThreshold,   "Seuil" },
            { kTooltipThreshold, "Seuil de la porte de bruit en dB. Les signaux en dessous sont coupes." },
            { kLabelReverbBtn,      "Reverb" },
            { kLabelTone,           "Timbre" },
            { kLabelModernBtn,      "Moderne" },
            { kLabelTransparentBtn, "Transparent" },
            { kLabelBypassBtn,      "Bypass" },
            { kLabelMidiOutBtn,     "MIDI OUT" },
            { kLabelDebug,          "Debug" },
            { kLabelAutoScroll,     "Auto-Scroll" },
            { kLabelMeasures,       "Mesures" },
            { kLabelMorph,          "Morph" },
            { kLabelCpu,            "CPU " },
            { kScaleChromatic,        "Chromatique" },
            { kScaleMajor,            "Majeur" },
            { kScaleMelodicMinor,     "Mineur melodique" },
            { kScaleHarmonicMinor,    "Mineur harmonique" },
            { kScaleNaturalMinor,     "Mineur naturel" },
            { kScaleMajorPentatonic,  "Pentatonique majeure" },
            { kScaleMinorPentatonic,  "Pentatonique mineure" },
            { kScaleBlues,            "Blues" },
            { kScaleDorian,           "Dorien" },
            { kScalePhrygian,         "Phrygien" },
            { kScaleLydian,           "Lydien" },
            { kScaleMixolydian,       "Mixolydien" },
            { kScaleLocrian,          "Locrien" },
            { kScaleCustom,           "Personnalise" },
            { kHarmonyNone,            "Aucun" },
            { kHarmony3rdBelow,        "3e dessous" },
            { kHarmony3rdAbove,        "3e dessus" },
            { kHarmony3rdBelowAbove,   "3e dessous + dessus" },
            { kHarmony4thBelow,        "4e dessous" },
            { kHarmony4thAbove,        "4e dessus" },
            { kHarmony4thBelowAbove,   "4e dessous + dessus" },
            { kHarmony5thBelow,        "5e dessous" },
            { kHarmony5thAbove,        "5e dessus" },
            { kHarmony5thBelowAbove,   "5e dessous + dessus" },
            { kHarmony3rdBelow5thAbove, "3e dessous + 5e dessus" },
            { kHarmony5thBelow3rdAbove, "5e dessous + 3e dessus" },
            { kHarmonyOctaveBelow,     "Octave dessous" },
            { kHarmonyOctaveAbove,     "Octave dessus" },
            { kHarmonyOctaveBelowAbove, "Octave dessous + dessus" },
            { kHarmonyVocalStack3,     "Pile vocale (3 voix)" },
            { kHarmonyVocalStack4,     "Pile vocale (4 voix)" },
            { kHarmonyPowerChord,      "Power Chord" },
            { kHarmonyParallel3rd,     "Tierce parallele" },
            { kHarmonyDrone,           "Drone" },
            { kHarmonyUnison2,         "Unisson (2 voix)" },
            { kHarmonyUnisonOctaves4,  "Unisson + Octaves (4 voix)" },
            { kTooltipZoomIn,      "Zoom avant (plage etroite)" },
            { kTooltipZoomOut,     "Zoom arriere (plage large)" },
            { kTooltipScrollUp,    "Defiler vers le haut (aigus)" },
            { kTooltipScrollDown,  "Defiler vers le bas (graves)" },
            { kTooltipResetView,   "Reinitialiser zoom et defilement" },
            { kTooltipSnapToScale,    "Aligner les points sur les notes de la gamme" },
            { kTooltipSnapToGrid,     "Aligner les points sur la grille rythmique" },
            { kTooltipStepMode,       "Interpolation en escalier entre les notes" },
            { kTooltipClearAll,       "Effacer tous les points de courbe" },
            { kTooltipPresets,        "Gerer les presets de courbe" },
            { kTooltipBypass,         "Bypass du traitement audio." },
            { kTooltipMidiOut,        "Activer la sortie MIDI" },
            { kTooltipCorrection,     "Moderne = correction agressive. Transparent = plus doux, preserve les transitions." },
            { kTooltipHarmonyEn,      "Activer/desactiver la generation d'harmonie." },
            { kTooltipReverbEn,       "Activer/desactiver l'effet de reverberation." },
            { kTooltipFormant,        "Activer/desactiger le deplacement de formant." },
            { kTooltipFlexTune,       "Deadband en cents : les entrees dans cette plage ne sont pas corrigees." },
            { kTooltipHumanize,       "Fluctuations aleatoires de hauteur en cents, ajoutees lors de la correction." },
            { kTooltipToneColor,      "Couleur du ton pour les harmonies synthetisees." },
            { kTooltipAB,             "A/B : Cliquer pour basculer entre les slots A et B. Clic droit pour sauvegarder." },
            { kTooltipReset,          "Reinitialiser la position de lecture." },
            { kTooltipCheckUpdates,   "Verifier la derniere version d'OpenVoxTuner sur GitHub." },
            { kTooltipMorphDrag,      "Glisser pour morpher entre le slot A (gauche) et le slot B (droite).\nDouble-clic pour aller a 50%.\nClic droit pour les options." },
            { kTooltipMorphLabel,     "Morpher entre deux etats du plugin. Cliquer sur A ou B pour definir les slots, puis glisser le curseur." },
            { kTooltipResetTransportDetail, "Reinitialiser la position de lecture.\nReinitialise le decalage de la ligne temporelle (utile en Standalone / VST3 classique)." },
            { kTooltipBypassIcon,     "Bypass du traitement audio.\nLorsque active, l'audio passe sans correction." },
            { kTooltipMidiOutIcon,    "Activer la sortie MIDI" },
            { kTooltipMenuOptions,    "Options d'OpenVoxTuner" },
            { kTooltipDebugWindow,    "Ouvrir la fenetre de debug: journal MIDI, test attaque/relache" },
            { kTooltipUpdateAvailable, "Ouvrir la derniere version d'OpenVoxTuner." },
            { kTooltipUpdateReleases, "Ouvrir la page des versions d'OpenVoxTuner." },
            { kTooltipAbSlotA,        "Cliquer: charger le slot A. Clic droit: sauvegarder l'etat actuel." },
            { kTooltipAbSlotB,        "Cliquer: charger le slot B. Clic droit: sauvegarder l'etat actuel." },
            { kTooltipAutoScroll,     "Defiler automatiquement la vue de l'editeur pendant la lecture" },
            { kTooltipUndo,           "Annuler (Ctrl+Z)" },
            { kTooltipRedo,           "Refaire (Ctrl+Y)" },
            { kLegendInput,           "Entree" },
            { kLegendOutput,          "Sortie" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Molette: Defiler" },
            { kLegendZoomHint,        "Ctrl+Moll: Zoom" },
            { kLegendInTune,          "accorde" },
            { kStatusChecking,        "Verification..." },
            { kStatusUpdateAvailable, "Mise a jour disponible" },
            { kStatusUpToDate,        "A jour" },
            { kStatusUpdateFailed,    "Echec de la verification" },
            { kStatusUpdatePrefix,    "Mise a jour " },
            { kHelpTitle,             "Raccourcis clavier et controles souris" },
            { kHelpCloseHint,         "Cliquer n'importe ou pour fermer" },
            { kHelpMouseWheel,        "Defiler verticalement (hauteur)" },
            { kHelpCtrlWheel,         "Zoom avant/arriere" },
            { kHelpClickDrag,         "Deplacer les points de courbe" },
            { kHelpDoubleClick,       "Ajouter un point de courbe" },
            { kHelpRightClick,        "Supprimer un point de courbe" },
            { kHelpCopy,              "Copier les points selectionnes" },
            { kHelpPaste,             "Coller les points copies" },
            { kHelpDelete,            "Supprimer les points selectionnes" },
            { kHelpUndo,              "Annuler" },
            { kHelpRedo,              "Refaire" },
            { kHelpToggleHelp,        "Afficher/masquer cette aide" },
            { kDlgExport,             "Export" },
            { kDlgExportPng,          "Exporter le visualiseur en PNG" },
            { kDlgExportNotFound,     "Impossible de trouver le composant visualiseur." },
            { kDlgImageSaved,         "Image sauvegardee dans :\n" },
            { kDlgImageFailed,        "Echec de la sauvegarde de l'image." },
            { kDlgSavePreset,         "Sauvegarder le preset" },
            { kDlgSavePresetDesc,     "Sauvegarder la configuration de l'editeur de courbe comme preset personnalise." },
            { kDlgSave,               "Sauvegarder" },
            { kDlgInvalidName,        "Nom invalide" },
            { kDlgEmptyName,          "Le nom du preset ne peut pas etre vide." },
            { kDlgOverwrite,          "Ecraser le preset ?" },
            { kDlgOverwriteDesc,      "Un preset avec ce nom existe deja.\nL'ecraser ?" },
            { kDlgOverwriteBtn,       "Ecraser" },
            { kDlgPresetSaved,        "Preset sauvegarde" },
            { kDlgPresetSavedDesc,    "Preset personnalise sauvegardee :\n" },
            { kDlgSaveFailed,         "Echec de la sauvegarde" },
            { kDlgSaveFailedDesc,     "Impossible d'ecrire le fichier de preset." },
            { kDlgDeletePreset,       "Supprimer le preset ?" },
            { kDlgDeletePresetDesc,   "Supprimer ce preset personnalise definitivement ?\n" },
            { kDlgDelete,             "Supprimer" },
            { kDlgDeleteFailed,       "Echec de la suppression" },
            { kDlgDeleteFailedDesc,   "Impossible de supprimer le fichier de preset. Verifiez les permissions." },
            { kDlgPresetDeleted,      "Preset supprime" },
            { kDlgPresetDeletedDesc,  "Preset personnalise supprime :\n" },
            { kDlgMidiLearn,          "Apprentissage MIDI" },
            { kDlgMidiLearnDesc,      "Deplacer un controlleur MIDI pour l'assigner a ce parametre.\nAppuyer sur Echap pour annuler." },
            { kDlgOk,                 "OK" },
            { kDlgDelete_,            "Supprimer" },
            { kMidiLearnSpeed,        "Vitesse" },
            { kMidiLearnAmount,       "Intensite" },
            { kMidiLearnFormant,      "Formant" },
            { kMidiLearnReverbMix,    "Mix Reverb" },
            { kMidiLearnFlexTune,     "FlexTune" },
            { kMidiLearnHumanize,     "Humaniser" },
            { kMidiLearnHarmonyGain,  "Gain harmonie" },
            { kMidiLearnHarmonyBlend, "Mixage harmonie" },
            { kMidiLearnHarmonyTone,  "Timbre harmonie" },
            { kHintScrollZoom,        "Molette: Defiler | " },
            { kHintZoom,              "+Molette: Zoom" },
            { kHintAddPoint,          "Double-clic: Ajouter point | Clic droit: Presets de courbe" },
            { kHintLiveMode,          "Mode Live : basculer sur l'Editeur de courbe pour modifier" },
        };

        static const std::unordered_map<std::string, juce::String> german = {
            { kMenuTheme,          "Thema" },
            { kMenuDarkTheme,      "Dunkles Thema" },
            { kMenuLightTheme,     "Helles Thema" },
            { kMenuLanguage,       "Sprache" },
            { kMenuLatency,        "Latenz" },
            { kMenuMidiOut,        "MIDI-Ausgabe" },
            { kMenuTuningType,     "Stimmungsmodus" },
            { kMenuModern,         "Modern" },
            { kMenuTransparent,    "Transparent" },
            { kMenuPitchDetection, "Tonhoenerkennung" },
            { kMenuCheckUpdates,   "Nach Updates suchen" },
            { kMenuResetDefault,   "Auf Standard zuruecksetzen" },
            { kMenuBypass,         "Bypass" },
            { kMenuExportImage,    "Als Bild exportieren..." },
            { kMenuLowLatency,     "Niedrige Latenz" },
            { kMenuQuality,        "Qualitaet" },
            { kMenuSafe,           "Sicher" },
            { kMenuDirectMonitoring, "Direktueberwachung" },
            { kMenuYinActive,      "YIN (aktiv)" },
            { kMenuShowWaveform,   "Wellenform anzeigen" },
            { kMenuWaveformDisplay, "Wellenform-Anzeige" },
            { kMenuWaveformLine,   "Linie" },
            { kMenuWaveformMirror, "Spiegelung" },
            { kMenuMidiLearn,      "MIDI-Lernen" },
            { kMenuKeyboardShortcuts, "Tastaturkurzbefehle (?)" },
            { kMenuConfirmReset,   "Zuruecksetzen bestaetigen" },
            { kMenuCancel,         "Abbrechen" },
            { kMenuDebugWindow,    "Debug-Fenster" },
            { kMenuMorphSetSource, "Quelle setzen (Aktuell)" },
            { kMenuMorphSetTargetA, "Ziel aus A/B Slot A setzen" },
            { kMenuMorphSetTargetB, "Ziel aus A/B Slot B setzen" },
            { kMenuMorphAtoB,      "Morph A -> B" },
            { kMenuMorphUndo,      "Morph rueckgaengig" },
            { kMenuMorphReset,     "Morph zuruecksetzen" },
            { kMenuSavePresetAs,   "Preset speichern unter..." },
            { kMenuDeletePreset,   "Loeschen..." },
            { kMenuFactory,        "Werkseinstellung" },
            { kMenuCustom,         "Benutzerdefiniert" },
            { kMenuCurvePresets,   "Kurven-Presets" },
            { kMenuAutoScroll,     "Auto-Scroll" },
            { kMenuTempo,          "Tempo" },
            { kTooltipCurveOptions, "Kurveneditor-Optionen" },
            { kTooltipPlay,        "Wiedergabe (Standalone-Zeitleiste starten)" },
            { kTooltipStop,        "Stopp (Standalone-Zeitleiste zum Bearbeiten einfrieren)" },
            { kTooltipRewind,      "Zum Anfang (Playhead zuruecksetzen)" },
            { kTabLive,            "Live" },
            { kTabCurveEditor,     "Kurveneditor" },
            { kLabelSpeed,         "Geschwindigkeit (ms)" },
            { kLabelAmount,        "Intensitaet" },
            { kLabelScale,         "Tonleiter" },
            { kLabelRoot,          "Grundton" },
            { kLabelVolume,        "Lautstaerke" },
            { kLabelBlend,         "Mischung" },
            { kTooltipSpeed,        "Korrekturgeschwindigkeit in Millisekunden. Niedriger = schnellere Tonhohenverfolgung." },
            { kTooltipAmount,       "Korrekturmenge (0 % = natuerlich, 100 % = vollstaendig zur Zieltonhoehe korrigiert)." },
            { kTooltipVolume,       "Ausgabelautstaerke der Harmoniestimmen." },
            { kTooltipBlend,        "Balance zwischen Hauptgesang und erzeugten Harmoniestimmen." },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humanisieren" },
            { kLabelFormant,       "Formant" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Harmonie" },
            { kLabelCorrectionMode, "Korrekturmodus" },
            { kLabelModern,         "Modern" },
            { kLabelTransparent,    "Transparent" },
            { kLabelBypass,         "Bypass" },
            { kLabelMidiOut,        "MIDI-Ausgabe" },
            { kLabelUseVoice,       "Stimme verwenden" },
            { kLabelSnap,           "An Tonleiter einrasten" },
            { kLabelSnapGrid,       "Am Raster einrasten" },
            { kLabelStepMode,       "Schrittmodus" },
            { kLabelClear,          "Loeschen" },
            { kLabelPresets,        "Presets" },
            { kLabelHelp,           "Hilfe" },
            { kLabelUpdates,        "Updates" },
            { kUseVoice,            "Stimme verwenden" },
            { kLabelHarmonyBtn,     "Harmonie" },
            { kLabelFormantBtn,     "Formant" },
            { kLabelNoiseGate,   "Gate" },
            { kTooltipNoiseGate, "Rauschtor fuer Audieingang aktivieren/deaktivieren." },
            { kLabelThreshold,   "Schwelle" },
            { kTooltipThreshold, "Rauschtor-Schwelle in dB. Signale unter diesem Wert werden unterdrueckt." },
            { kLabelReverbBtn,      "Reverb" },
            { kLabelTone,           "Klangfarbe" },
            { kLabelModernBtn,      "Modern" },
            { kLabelTransparentBtn, "Transparent" },
            { kLabelBypassBtn,      "Bypass" },
            { kLabelMidiOutBtn,     "MIDI OUT" },
            { kLabelDebug,          "Debug" },
            { kLabelAutoScroll,     "Auto-Scroll" },
            { kLabelMeasures,       "Takte" },
            { kLabelMorph,          "Morph" },
            { kLabelCpu,            "CPU " },
            { kScaleChromatic,        "Chromatisch" },
            { kScaleMajor,            "Dur" },
            { kScaleMelodicMinor,     "Melodisch Moll" },
            { kScaleHarmonicMinor,    "Harmonisch Moll" },
            { kScaleNaturalMinor,     "Natuerlich Moll" },
            { kScaleMajorPentatonic,  "Major Pentatonik" },
            { kScaleMinorPentatonic,  "Minor Pentatonik" },
            { kScaleBlues,            "Blues" },
            { kScaleDorian,           "Dorisch" },
            { kScalePhrygian,         "Phrygisch" },
            { kScaleLydian,           "Lydisch" },
            { kScaleMixolydian,       "Mixolydisch" },
            { kScaleLocrian,          "Lokrisch" },
            { kScaleCustom,           "Benutzerdefiniert" },
            { kHarmonyNone,            "Keine" },
            { kHarmony3rdBelow,        "Terz unten" },
            { kHarmony3rdAbove,        "Terz oben" },
            { kHarmony3rdBelowAbove,   "Terz unten + oben" },
            { kHarmony4thBelow,        "Quarte unten" },
            { kHarmony4thAbove,        "Quarte oben" },
            { kHarmony4thBelowAbove,   "Quarte unten + oben" },
            { kHarmony5thBelow,        "Quinte unten" },
            { kHarmony5thAbove,        "Quinte oben" },
            { kHarmony5thBelowAbove,   "Quinte unten + oben" },
            { kHarmony3rdBelow5thAbove, "Terz unten + Quinte oben" },
            { kHarmony5thBelow3rdAbove, "Quinte unten + Terz oben" },
            { kHarmonyOctaveBelow,     "Oktave unten" },
            { kHarmonyOctaveAbove,     "Oktave oben" },
            { kHarmonyOctaveBelowAbove, "Oktave unten + oben" },
            { kHarmonyVocalStack3,     "Vocal-Stack (3 Stimmen)" },
            { kHarmonyVocalStack4,     "Vocal-Stack (4 Stimmen)" },
            { kHarmonyPowerChord,      "Power Chord" },
            { kHarmonyParallel3rd,     "Terz parallel" },
            { kHarmonyDrone,           "Drone" },
            { kHarmonyUnison2,         "Unisono (2 Stimmen)" },
            { kHarmonyUnisonOctaves4,  "Unisono + Oktaven (4 Stimmen)" },
            { kTooltipZoomIn,      "Hineinzoomen (schmaeler Bereich)" },
            { kTooltipZoomOut,     "Herauszoomen (breiterer Bereich)" },
            { kTooltipScrollUp,    "Nach oben scrollen (hoehere Toene)" },
            { kTooltipScrollDown,  "Nach unten scrollen (tiefere Toene)" },
            { kTooltipResetView,   "Zoom und Scroll zuruecksetzen" },
            { kTooltipSnapToScale,    "Kurvenpunkte an Tonleiter einrasten" },
            { kTooltipSnapToGrid,     "Kurvenpunkte am Raster einrasten" },
            { kTooltipStepMode,       "Treppen-Interpolation" },
            { kTooltipClearAll,       "Alle Kurvenpunkte loeschen" },
            { kTooltipPresets,        "Kurven-Presets verwalten" },
            { kTooltipBypass,         "Audioverarbeitung umgehen." },
            { kTooltipMidiOut,        "MIDI-Ausgabe aktivieren" },
            { kTooltipCorrection,     "Modern = aggressive Korrektur. Transparent = sanfter, Uebergaenge erhalten." },
            { kTooltipHarmonyEn,      "Harmonieerzeugung aktivieren/deaktivieren." },
            { kTooltipReverbEn,       "Hall-Effekt aktivieren/deaktivieren." },
            { kTooltipFormant,        "Formantenverschiebung aktivieren/deaktivieren." },
            { kTooltipFlexTune,       "Deadband in Cents: Eingaenge in diesem Bereich werden nicht korrigiert." },
            { kTooltipHumanize,       "Zufaellige Tonhoehen-Schwankungen in Cents bei Korrektur." },
            { kTooltipToneColor,      "Tonfarbe fuer Synthesizer-Harmonien." },
            { kTooltipAB,             "A/B: Klicken zum Umschalten zwischen Slot A und B. Rechtsklick zum Speichern." },
            { kTooltipReset,          "Wiedergabeposition zuruecksetzen." },
            { kTooltipCheckUpdates,   "Neueste OpenVoxTuner-Version auf GitHub pruefen." },
            { kTooltipMorphDrag,      "Ziehen, um zwischen Slot A (links) und Slot B (rechts) zu morphen.\nDoppelklick zum Einrasten auf 50%.\nRechtsklick fuer Optionen." },
            { kTooltipMorphLabel,     "Zwischen zwei Plugin-Zustaenden morphen. A oder B klicken, dann Schieberegler ziehen." },
            { kTooltipResetTransportDetail, "Wiedergabeposition zuruecksetzen.\nSetzt den internen Zeitleisten-Offset zurueck (nuetzlich in Standalone / klassischem VST3)." },
            { kTooltipBypassIcon,     "Audioverarbeitung umgehen.\nWenn aktiviert, wird Audio ohne Korrektur durchgelassen." },
            { kTooltipMidiOutIcon,    "MIDI-Ausgabe aktivieren" },
            { kTooltipMenuOptions,    "OpenVoxTuner-Optionen" },
            { kTooltipDebugWindow,    "Debug-Fenster oeffnen: MIDI-Log, Angriff/Release-Test" },
            { kTooltipUpdateAvailable, "Neueste OpenVoxTuner-Version oeffnen." },
            { kTooltipUpdateReleases, "OpenVoxTuner-Releases-Seite oeffnen." },
            { kTooltipAbSlotA,        "Klicken: Slot A laden. Rechtsklick: Aktuellen Zustand speichern." },
            { kTooltipAbSlotB,        "Klicken: Slot B laden. Rechtsklick: Aktuellen Zustand speichern." },
            { kTooltipAutoScroll,     "Editor-Ansicht waehrend der Wiedergabe automatisch scrollen" },
            { kTooltipUndo,           "Rueckgaengig (Ctrl+Z)" },
            { kTooltipRedo,           "Wiederholen (Ctrl+Y)" },
            { kLegendInput,           "Eingang" },
            { kLegendOutput,          "Ausgang" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Rad: Scrollen" },
            { kLegendZoomHint,        "Strg+Rad: Zoom" },
            { kLegendInTune,          "stimmt" },
            { kStatusChecking,        "Pruefen..." },
            { kStatusUpdateAvailable, "Update verfuegbar" },
            { kStatusUpToDate,        "Auf dem neuesten Stand" },
            { kStatusUpdateFailed,    "Update-Pruefung fehlgeschlagen" },
            { kStatusUpdatePrefix,    "Update " },
            { kHelpTitle,             "Tastaturkurzbefehle und Maussteuerung" },
            { kHelpCloseHint,         "Irgendwo klicken zum Schliessen" },
            { kHelpMouseWheel,        "Vertikal scrollen (Tonhoehe)" },
            { kHelpCtrlWheel,         "Zoom rein/raus" },
            { kHelpClickDrag,         "Kurvenpunkte verschieben" },
            { kHelpDoubleClick,       "Neuen Kurvenpunkt hinzufuegen" },
            { kHelpRightClick,        "Kurvenpunkt loeschen" },
            { kHelpCopy,              "Ausgewaehlte Punkte kopieren" },
            { kHelpPaste,             "Kopierte Punkte einfuegen" },
            { kHelpDelete,            "Ausgewaehlte Punkte loeschen" },
            { kHelpUndo,              "Rueckgaengig" },
            { kHelpRedo,              "Wiederholen" },
            { kHelpToggleHelp,        "Diese Hilfe ein-/ausblenden" },
            { kDlgExport,             "Export" },
            { kDlgExportPng,          "Visualizer als PNG exportieren" },
            { kDlgExportNotFound,     "Visualizer-Komponente nicht gefunden." },
            { kDlgImageSaved,         "Bild gespeichert unter:\n" },
            { kDlgImageFailed,        "Bildspeicherung fehlgeschlagen." },
            { kDlgSavePreset,         "Preset speichern" },
            { kDlgSavePresetDesc,     "Aktuelle Kurveneditor-Konfiguration als benutzerdefiniertes Preset speichern." },
            { kDlgSave,               "Speichern" },
            { kDlgInvalidName,        "Ungueltiger Name" },
            { kDlgEmptyName,          "Preset-Name darf nicht leer sein." },
            { kDlgOverwrite,          "Preset ueberschreiben?" },
            { kDlgOverwriteDesc,      "Ein Preset mit diesem Namen existiert bereits.\nUeberschreiben?" },
            { kDlgOverwriteBtn,       "Ueberschreiben" },
            { kDlgPresetSaved,        "Preset gespeichert" },
            { kDlgPresetSavedDesc,    "Benutzerdefiniertes Preset gespeichert:\n" },
            { kDlgSaveFailed,         "Speichern fehlgeschlagen" },
            { kDlgSaveFailedDesc,     "Preset-Datei konnte nicht geschrieben werden." },
            { kDlgDeletePreset,       "Preset loeschen?" },
            { kDlgDeletePresetDesc,   "Dieses benutzerdefinierte Preset dauerhaft loeschen?\n" },
            { kDlgDelete,             "Loeschen" },
            { kDlgDeleteFailed,       "Loeschen fehlgeschlagen" },
            { kDlgDeleteFailedDesc,   "Preset-Datei konnte nicht geloescht werden. Berechtigungen pruefen." },
            { kDlgPresetDeleted,      "Preset geloescht" },
            { kDlgPresetDeletedDesc,  "Benutzerdefiniertes Preset geloescht:\n" },
            { kDlgMidiLearn,          "MIDI-Lernen" },
            { kDlgMidiLearnDesc,      "MIDI-Controller bewegen, um diesen Parameter zuzuweisen.\nEscape zum Abbrechen." },
            { kDlgOk,                 "OK" },
            { kDlgDelete_,            "Loeschen" },
            { kMidiLearnSpeed,        "Geschwindigkeit" },
            { kMidiLearnAmount,       "Intensitaet" },
            { kMidiLearnFormant,      "Formant" },
            { kMidiLearnReverbMix,    "Reverb-Mix" },
            { kMidiLearnFlexTune,     "FlexTune" },
            { kMidiLearnHumanize,     "Humanisieren" },
            { kMidiLearnHarmonyGain,  "Harmonie-Lautstaerke" },
            { kMidiLearnHarmonyBlend, "Harmonie-Mischung" },
            { kMidiLearnHarmonyTone,  "Harmonie-Klangfarbe" },
            { kHintScrollZoom,        "Rad: Scrollen | " },
            { kHintZoom,              "+Rad: Zoom" },
            { kHintAddPoint,          "Doppelklick: Punkt hinzufuegen | Rechtsklick: Kurven-Presets" },
            { kHintLiveMode,          "Live-Modus: Zum Kurveneditor wechseln zum Bearbeiten" },
        };

        static const std::unordered_map<std::string, juce::String> spanish = {
            { kMenuTheme,          "Tema" },
            { kMenuDarkTheme,      "Tema oscuro" },
            { kMenuLightTheme,     "Tema claro" },
            { kMenuLanguage,       "Idioma" },
            { kMenuLatency,        "Latencia" },
            { kMenuMidiOut,        "Salida MIDI" },
            { kMenuTuningType,     "Tipo de afinacion" },
            { kMenuModern,         "Moderno" },
            { kMenuTransparent,    "Transparente" },
            { kMenuPitchDetection, "Deteccion de tono" },
            { kMenuCheckUpdates,   "Buscar actualizaciones" },
            { kMenuResetDefault,   "Restablecer valores predeterminados" },
            { kMenuBypass,         "Bypass" },
            { kMenuExportImage,    "Exportar como imagen..." },
            { kMenuLowLatency,     "Baja latencia" },
            { kMenuQuality,        "Calidad" },
            { kMenuSafe,           "Seguro" },
            { kMenuDirectMonitoring, "Monitorizacion directa" },
            { kMenuYinActive,      "YIN (activo)" },
            { kMenuShowWaveform,   "Mostrar forma de onda" },
            { kMenuWaveformDisplay, "Visualizacion de forma de onda" },
            { kMenuWaveformLine,   "Linea" },
            { kMenuWaveformMirror, "Espejo" },
            { kMenuMidiLearn,      "Aprendizaje MIDI" },
            { kMenuKeyboardShortcuts, "Atajos de teclado (?)" },
            { kMenuConfirmReset,   "Confirmar reinicio" },
            { kMenuCancel,         "Cancelar" },
            { kMenuDebugWindow,    "Ventana de debug" },
            { kMenuMorphSetSource, "Establecer origen (actual)" },
            { kMenuMorphSetTargetA, "Establecer destino desde ranura A" },
            { kMenuMorphSetTargetB, "Establecer destino desde ranura B" },
            { kMenuMorphAtoB,      "Morph A -> B" },
            { kMenuMorphUndo,      "Deshacer morph" },
            { kMenuMorphReset,     "Reiniciar morph" },
            { kMenuSavePresetAs,   "Guardar preset como..." },
            { kMenuDeletePreset,   "Eliminar..." },
            { kMenuFactory,        "Fabrica" },
            { kMenuCustom,         "Personalizado" },
            { kMenuCurvePresets,   "Presets de curva" },
            { kMenuAutoScroll,     "Auto-Scroll" },
            { kMenuTempo,          "Tempo" },
            { kTooltipCurveOptions, "Opciones del editor de curva" },
            { kTooltipPlay,        "Reproducir (iniciar la linea de tiempo standalone)" },
            { kTooltipStop,        "Detener (congelar la linea de tiempo standalone para editar)" },
            { kTooltipRewind,      "Volver al inicio (reiniciar el playhead)" },
            { kTabLive,            "En vivo" },
            { kTabCurveEditor,     "Editor de curva" },
            { kLabelSpeed,         "Velocidad (ms)" },
            { kLabelAmount,        "Intensidad" },
            { kLabelScale,         "Escala" },
            { kLabelRoot,          "Tono base" },
            { kLabelVolume,        "Volumen" },
            { kLabelBlend,         "Mezcla" },
            { kTooltipSpeed,        "Velocidad de correccion en milisegundos. Mas bajo = seguimiento de tono mas rapido." },
            { kTooltipAmount,       "Cantidad de correccion (0 % = natural, 100 % = totalmente corregido al objetivo)." },
            { kTooltipVolume,       "Volumen de salida de las voces de armonia." },
            { kTooltipBlend,        "Equilibrio entre la voz principal y las voces de armonia generadas." },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humanizar" },
            { kLabelFormant,       "Formante" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Armonia" },
            { kLabelCorrectionMode, "Modo de correccion" },
            { kLabelModern,         "Moderno" },
            { kLabelTransparent,    "Transparente" },
            { kLabelBypass,         "Bypass" },
            { kLabelMidiOut,        "Salida MIDI" },
            { kLabelUseVoice,       "Usar voz" },
            { kLabelSnap,           "Ajustar a escala" },
            { kLabelSnapGrid,       "Ajustar a cuadricula" },
            { kLabelStepMode,       "Modo escalera" },
            { kLabelClear,          "Borrar" },
            { kLabelPresets,        "Presets" },
            { kLabelHelp,           "Ayuda" },
            { kLabelUpdates,        "Actualizaciones" },
            { kUseVoice,            "Usar voz" },
            { kLabelHarmonyBtn,     "Armonia" },
            { kLabelFormantBtn,     "Formante" },
            { kLabelNoiseGate,   "Puerta" },
            { kTooltipNoiseGate, "Activar/desactivar la puerta de ruido en la entrada de audio." },
            { kLabelThreshold,   "Umbral" },
            { kTooltipThreshold, "Umbral de la puerta de ruido en dB. Las senales por debajo se silencian." },
            { kLabelReverbBtn,      "Reverb" },
            { kLabelTone,           "Tono" },
            { kLabelModernBtn,      "Moderno" },
            { kLabelTransparentBtn, "Transparente" },
            { kLabelBypassBtn,      "Bypass" },
            { kLabelMidiOutBtn,     "MIDI OUT" },
            { kLabelDebug,          "Debug" },
            { kLabelAutoScroll,     "Auto-Scroll" },
            { kLabelMeasures,       "Compases" },
            { kLabelMorph,          "Morph" },
            { kLabelCpu,            "CPU " },
            { kScaleChromatic,        "Cromatica" },
            { kScaleMajor,            "Mayor" },
            { kScaleMelodicMinor,     "Menor melodica" },
            { kScaleHarmonicMinor,    "Menor armonica" },
            { kScaleNaturalMinor,     "Menor natural" },
            { kScaleMajorPentatonic,  "Pentatonica mayor" },
            { kScaleMinorPentatonic,  "Pentatonica menor" },
            { kScaleBlues,            "Blues" },
            { kScaleDorian,           "Dorico" },
            { kScalePhrygian,         "Frigio" },
            { kScaleLydian,           "Lidio" },
            { kScaleMixolydian,       "Mixolidio" },
            { kScaleLocrian,          "Locrio" },
            { kScaleCustom,           "Personalizada" },
            { kHarmonyNone,            "Ninguno" },
            { kHarmony3rdBelow,        "3a abajo" },
            { kHarmony3rdAbove,        "3a arriba" },
            { kHarmony3rdBelowAbove,   "3a abajo + arriba" },
            { kHarmony4thBelow,        "4a abajo" },
            { kHarmony4thAbove,        "4a arriba" },
            { kHarmony4thBelowAbove,   "4a abajo + arriba" },
            { kHarmony5thBelow,        "5a abajo" },
            { kHarmony5thAbove,        "5a arriba" },
            { kHarmony5thBelowAbove,   "5a abajo + arriba" },
            { kHarmony3rdBelow5thAbove, "3a abajo + 5a arriba" },
            { kHarmony5thBelow3rdAbove, "5a abajo + 3a arriba" },
            { kHarmonyOctaveBelow,     "Octava abajo" },
            { kHarmonyOctaveAbove,     "Octava arriba" },
            { kHarmonyOctaveBelowAbove, "Octava abajo + arriba" },
            { kHarmonyVocalStack3,     "Pila vocal (3 voces)" },
            { kHarmonyVocalStack4,     "Pila vocal (4 voces)" },
            { kHarmonyPowerChord,      "Power Chord" },
            { kHarmonyParallel3rd,     "Tercia paralela" },
            { kHarmonyDrone,           "Drone" },
            { kHarmonyUnison2,         "Unisono (2 voces)" },
            { kHarmonyUnisonOctaves4,  "Unisono + Octavas (4 voces)" },
            { kTooltipZoomIn,      "Acercar (rango estrecho)" },
            { kTooltipZoomOut,     "Alejar (rango amplio)" },
            { kTooltipScrollUp,    "Desplazar arriba (agudos)" },
            { kTooltipScrollDown,  "Desplazar abajo (graves)" },
            { kTooltipResetView,   "Restablecer zoom y desplazamiento" },
            { kTooltipSnapToScale,    "Ajustar puntos a la escala" },
            { kTooltipSnapToGrid,     "Ajustar puntos a la cuadricula" },
            { kTooltipStepMode,       "Interpolacion escalonada" },
            { kTooltipClearAll,       "Borrar todos los puntos" },
            { kTooltipPresets,        "Gestionar presets" },
            { kTooltipBypass,         "Omitir procesamiento de audio." },
            { kTooltipMidiOut,        "Activar salida MIDI" },
            { kTooltipCorrection,     "Moderno = correccion agresiva. Transparente = mas suave, preserva transiciones." },
            { kTooltipHarmonyEn,      "Activar/desactivar generacion de armonia." },
            { kTooltipReverbEn,       "Activar/desactivar efecto de reverberacion." },
            { kTooltipFormant,        "Activar/desactivar desplazamiento de formante." },
            { kTooltipFlexTune,       "Deadband en cents: las entradas en este rango no se corrigen." },
            { kTooltipHumanize,       "Fluctuaciones aleatorias de tono en cents al aplicar correccion." },
            { kTooltipToneColor,      "Color de tono para armonias sintetizadas." },
            { kTooltipAB,             "A/B: Clic para alternar entre ranura A y B. Clic derecho para guardar." },
            { kTooltipReset,          "Reiniciar posicion de reproduccion." },
            { kTooltipCheckUpdates,   "Verificar la ultima version de OpenVoxTuner en GitHub." },
            { kTooltipMorphDrag,      "Arrastrar para morph entre ranura A (izquierda) y ranura B (derecha).\nDoble clic para ajustar al 50%.\nClic derecho para opciones." },
            { kTooltipMorphLabel,     "Morph entre dos estados del plugin. Hacer clic en A o B para configurar las ranuras, luego arrastrar el deslizador." },
            { kTooltipResetTransportDetail, "Reiniciar posicion de reproduccion.\nReinicia el offset de la linea de tiempo (util en Standalone / VST3 clasico)." },
            { kTooltipBypassIcon,     "Omitir procesamiento de audio.\nCuando esta activado, el audio pasa sin correccion." },
            { kTooltipMidiOutIcon,    "Activar salida MIDI" },
            { kTooltipMenuOptions,    "Opciones de OpenVoxTuner" },
            { kTooltipDebugWindow,    "Abrir ventana de debug: registro MIDI, prueba ataque/liberacion" },
            { kTooltipUpdateAvailable, "Abrir la ultima version de OpenVoxTuner." },
            { kTooltipUpdateReleases, "Abrir la pagina de versiones de OpenVoxTuner." },
            { kTooltipAbSlotA,        "Clic: cargar ranura A. Clic derecho: guardar estado actual." },
            { kTooltipAbSlotB,        "Clic: cargar ranura B. Clic derecho: guardar estado actual." },
            { kTooltipAutoScroll,     "Desplazar automaticamente la vista del editor durante la reproduccion" },
            { kTooltipUndo,           "Deshacer (Ctrl+Z)" },
            { kTooltipRedo,           "Rehacer (Ctrl+Y)" },
            { kLegendInput,           "Entrada" },
            { kLegendOutput,          "Salida" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Rueda: Desplazar" },
            { kLegendZoomHint,        "Ctrl+Rueda: Zoom" },
            { kLegendInTune,          "afinado" },
            { kStatusChecking,        "Verificando..." },
            { kStatusUpdateAvailable, "Actualizacion disponible" },
            { kStatusUpToDate,        "Actualizado" },
            { kStatusUpdateFailed,    "Error en la verificacion" },
            { kStatusUpdatePrefix,    "Actualizar " },
            { kHelpTitle,             "Atajos de teclado y controles de raton" },
            { kHelpCloseHint,         "Hacer clic en cualquier lugar para cerrar" },
            { kHelpMouseWheel,        "Desplazar verticalmente (tono)" },
            { kHelpCtrlWheel,         "Acercar/alejar" },
            { kHelpClickDrag,         "Mover puntos de curva" },
            { kHelpDoubleClick,       "Agregar nuevo punto de curva" },
            { kHelpRightClick,        "Eliminar punto de curva" },
            { kHelpCopy,              "Copiar puntos seleccionados" },
            { kHelpPaste,             "Pegar puntos copiados" },
            { kHelpDelete,            "Eliminar puntos seleccionados" },
            { kHelpUndo,              "Deshacer" },
            { kHelpRedo,              "Rehacer" },
            { kHelpToggleHelp,        "Mostrar/ocultar esta ayuda" },
            { kDlgExport,             "Exportar" },
            { kDlgExportPng,          "Exportar visualizador como PNG" },
            { kDlgExportNotFound,     "No se pudo encontrar el componente visualizador." },
            { kDlgImageSaved,         "Imagen guardada en:\n" },
            { kDlgImageFailed,        "Error al guardar la imagen." },
            { kDlgSavePreset,         "Guardar preset" },
            { kDlgSavePresetDesc,     "Guardar la configuracion actual del editor de curva como un preset personalizado." },
            { kDlgSave,               "Guardar" },
            { kDlgInvalidName,        "Nombre invalido" },
            { kDlgEmptyName,          "El nombre del preset no puede estar vacio." },
            { kDlgOverwrite,          "Sobrescribir preset?" },
            { kDlgOverwriteDesc,      "Ya existe un preset con este nombre.\nSobrescribirlo?" },
            { kDlgOverwriteBtn,       "Sobrescribir" },
            { kDlgPresetSaved,        "Preset guardado" },
            { kDlgPresetSavedDesc,    "Preset personalizado guardado:\n" },
            { kDlgSaveFailed,         "Error al guardar" },
            { kDlgSaveFailedDesc,     "No se pudo escribir el archivo de preset." },
            { kDlgDeletePreset,       "Eliminar preset?" },
            { kDlgDeletePresetDesc,   "Eliminar este preset personalizado permanentemente?\n" },
            { kDlgDelete,             "Eliminar" },
            { kDlgDeleteFailed,       "Error al eliminar" },
            { kDlgDeleteFailedDesc,   "No se pudo eliminar el archivo de preset. Verifique los permisos." },
            { kDlgPresetDeleted,      "Preset eliminado" },
            { kDlgPresetDeletedDesc,  "Preset personalizado eliminado:\n" },
            { kDlgMidiLearn,          "Aprendizaje MIDI" },
            { kDlgMidiLearnDesc,      "Mover un controlador MIDI para asignarlo a este parametro.\nPresionar Escape para cancelar." },
            { kDlgOk,                 "OK" },
            { kDlgDelete_,            "Eliminar" },
            { kMidiLearnSpeed,        "Velocidad" },
            { kMidiLearnAmount,       "Intensidad" },
            { kMidiLearnFormant,      "Formante" },
            { kMidiLearnReverbMix,    "Mix Reverb" },
            { kMidiLearnFlexTune,     "FlexTune" },
            { kMidiLearnHumanize,     "Humanizar" },
            { kMidiLearnHarmonyGain,  "Ganancia de armonia" },
            { kMidiLearnHarmonyBlend, "Mezcla de armonia" },
            { kMidiLearnHarmonyTone,  "Tono de armonia" },
            { kHintScrollZoom,        "Rueda: Desplazar | " },
            { kHintZoom,              "+Rueda: Zoom" },
            { kHintAddPoint,          "Doble clic: Agregar punto | Clic derecho: Presets de curva" },
            { kHintLiveMode,          "Modo En vivo: cambiar al Editor de curva para editar" },
        };

        static const std::unordered_map<std::string, juce::String> japanese = {
            { kMenuTheme,          "テーマ" },
            { kMenuDarkTheme,      "ダークテーマ" },
            { kMenuLightTheme,     "ライトテーマ" },
            { kMenuLanguage,       "言語" },
            { kMenuLatency,        "レイテンシ" },
            { kMenuMidiOut,        "MIDI出力" },
            { kMenuTuningType,     "チューニングタイプ" },
            { kMenuModern,         "モダン" },
            { kMenuTransparent,    "トランズペアレント" },
            { kMenuPitchDetection, "ピッチ検出" },
            { kMenuCheckUpdates,   "アップデートを確認" },
            { kMenuResetDefault,   "デフォルトに戻す" },
            { kMenuBypass,         "バイパス" },
            { kMenuExportImage,    "画像としてエクスポート..." },
            { kMenuLowLatency,     "低レイテンシ" },
            { kMenuQuality,        "クオリティ" },
            { kMenuSafe,           "セーフ" },
            { kMenuDirectMonitoring, "ダイレクトモニタリング" },
            { kMenuYinActive,      "YIN (アクティブ)" },
            { kMenuShowWaveform,   "波形を表示" },
            { kMenuWaveformDisplay, "波形表示" },
            { kMenuWaveformLine,   "ライン" },
            { kMenuWaveformMirror, "ミラー" },
            { kMenuMidiLearn,      "MIDI学習" },
            { kMenuKeyboardShortcuts, "キーボードショートカット (?)" },
            { kMenuConfirmReset,   "リセットを確認" },
            { kMenuCancel,         "キャンセル" },
            { kMenuDebugWindow,    "デバッグウィンドウ" },
            { kMenuMorphSetSource, "ソースを設定（現在）" },
            { kMenuMorphSetTargetA, "A/BスロットAからターゲットを設定" },
            { kMenuMorphSetTargetB, "A/BスロットBからターゲットを設定" },
            { kMenuMorphAtoB,      "モーフ A -> B" },
            { kMenuMorphUndo,      "モーフを元に戻す" },
            { kMenuMorphReset,     "モーフをリセット" },
            { kMenuSavePresetAs,   "プリセットを名前を付けて保存..." },
            { kMenuDeletePreset,   "削除..." },
            { kMenuFactory,        "ファクトリ" },
            { kMenuCustom,         "カスタム" },
            { kMenuCurvePresets,   "カーブプリセット" },
            { kMenuAutoScroll,     "自動スクロール" },
            { kMenuTempo,          "テンポ" },
            { kTooltipCurveOptions, "カーブエディタのオプション" },
            { kTooltipPlay,        "再生（スタンドアロンのタイムラインを開始）" },
            { kTooltipStop,        "停止（編集のためにスタンドアロンのタイムラインを停止）" },
            { kTooltipRewind,      "最初に戻る（再生位置をリセット）" },
            { kTabLive,            "ライブ" },
            { kTabCurveEditor,     "カーブエディタ" },
            { kLabelSpeed,         "スピード (ms)" },
            { kLabelAmount,        "量" },
            { kLabelScale,         "スケール" },
            { kLabelRoot,          "キー" },
            { kLabelVolume,        "音量" },
            { kLabelBlend,         "ブレンド" },
            { kTooltipSpeed,        "補正速度（ミリ秒）。低いほどピッチ追従が速くなります。" },
            { kTooltipAmount,       "補正量（0% = 自然、100% = 目標音程に完全補正）。" },
            { kTooltipVolume,       "ハーモニーボイスの出力音量。" },
            { kTooltipBlend,        "メインボーカルと生成されたハーモニーボイスのバランス。" },
            { kLabelFlex,          "フレックス" },
            { kLabelHumanize,      "人性化" },
            { kLabelFormant,       "フォルマント" },
            { kLabelReverb,        "リバーブ" },
            { kLabelMix,           "ミックス" },
            { kLabelHarmony,       "ハーモニー" },
            { kLabelCorrectionMode, "補正モード" },
            { kLabelModern,         "モダン" },
            { kLabelTransparent,    "トランズペアレント" },
            { kLabelBypass,         "バイパス" },
            { kLabelMidiOut,        "MIDI出力" },
            { kLabelUseVoice,       "ボイス使用" },
            { kLabelSnap,           "スケールにスナップ" },
            { kLabelSnapGrid,       "グリッドにスナップ" },
            { kLabelStepMode,       "ステップモード" },
            { kLabelClear,          "クリア" },
            { kLabelPresets,        "プリセット" },
            { kLabelHelp,           "ヘルプ" },
            { kLabelUpdates,        "アップデート" },
            { kUseVoice,            "ボイス使用" },
            { kLabelHarmonyBtn,     "ハーモニー" },
            { kLabelFormantBtn,     "フォルマント" },
            { kLabelNoiseGate,   "ゲート" },
            { kTooltipNoiseGate, "入力オーディオのノイズゲートを有効/無効にする。" },
            { kLabelThreshold,   "しきい値" },
            { kTooltipThreshold, "ノイズゲートのしきい値（dB）。このレベル以下の信号はミュートされます。" },
            { kLabelReverbBtn,      "リバーブ" },
            { kLabelTone,           "音色" },
            { kLabelModernBtn,      "モダン" },
            { kLabelTransparentBtn, "トランスペアレント" },
            { kLabelBypassBtn,      "バイパス" },
            { kLabelMidiOutBtn,     "MIDI OUT" },
            { kLabelDebug,          "デバッグ" },
            { kLabelAutoScroll,     "自動スクロール" },
            { kLabelMeasures,       "小節" },
            { kLabelMorph,          "モーフ" },
            { kLabelCpu,            "CPU " },
            { kScaleChromatic,        "クロマティック" },
            { kScaleMajor,            "メジャー" },
            { kScaleMelodicMinor,     "メロディックマイナー" },
            { kScaleHarmonicMinor,    "ハーモニックマイナー" },
            { kScaleNaturalMinor,     "ナチュラルマイナー" },
            { kScaleMajorPentatonic,  "メジャーペンタトニック" },
            { kScaleMinorPentatonic,  "マイナーペンタトニック" },
            { kScaleBlues,            "ブルース" },
            { kScaleDorian,           "ドリアン" },
            { kScalePhrygian,         "フリギアン" },
            { kScaleLydian,           "リディアン" },
            { kScaleMixolydian,       "ミクソリディアン" },
            { kScaleLocrian,          "ロクリアン" },
            { kScaleCustom,           "カスタム" },
            { kHarmonyNone,            "なし" },
            { kHarmony3rdBelow,        "3度下" },
            { kHarmony3rdAbove,        "3度上" },
            { kHarmony3rdBelowAbove,   "3度下+上" },
            { kHarmony4thBelow,        "4度下" },
            { kHarmony4thAbove,        "4度上" },
            { kHarmony4thBelowAbove,   "4度下+上" },
            { kHarmony5thBelow,        "5度下" },
            { kHarmony5thAbove,        "5度上" },
            { kHarmony5thBelowAbove,   "5度下+上" },
            { kHarmony3rdBelow5thAbove, "3度下+5度上" },
            { kHarmony5thBelow3rdAbove, "5度下+3度上" },
            { kHarmonyOctaveBelow,     "オクターブ下" },
            { kHarmonyOctaveAbove,     "オクターブ上" },
            { kHarmonyOctaveBelowAbove, "オクターブ下+上" },
            { kHarmonyVocalStack3,     "ボーカルスタック(3声)" },
            { kHarmonyVocalStack4,     "ボーカルスタック(4声)" },
            { kHarmonyPowerChord,      "パワーコード" },
            { kHarmonyParallel3rd,     "並行3度" },
            { kHarmonyDrone,           "ドローン" },
            { kHarmonyUnison2,         "ユニゾン (2声)" },
            { kHarmonyUnisonOctaves4,  "ユニゾン+オクターブ (4声)" },
            { kTooltipZoomIn,      "ズームイン" },
            { kTooltipZoomOut,     "ズームアウト" },
            { kTooltipScrollUp,    "上にスクロール" },
            { kTooltipScrollDown,  "下にスクロール" },
            { kTooltipResetView,   "ビューをリセット" },
            { kTooltipSnapToScale,    "スケールにスナップ" },
            { kTooltipSnapToGrid,     "グリッドにスナップ" },
            { kTooltipStepMode,       "ステップモード" },
            { kTooltipClearAll,       "すべてクリア" },
            { kTooltipPresets,        "プリセット管理" },
            { kTooltipBypass,         "オーディオ処理をバイパス。" },
            { kTooltipMidiOut,        "MIDI出力を有効にする" },
            { kTooltipCorrection,     "モダン = 積極的な補正。トランスペアレント = やわらかく、推移を保持。" },
            { kTooltipHarmonyEn,      "ハーモニー生成の有効/無効。" },
            { kTooltipReverbEn,       "リバーブ効果の有効/無効。" },
            { kTooltipFormant,        "フォルマントシフトの有効/無効。" },
            { kTooltipFlexTune,       "デッドバンド(セント): この範囲内のピッチは補正されません。" },
            { kTooltipHumanize,       "補正時に追加されるランダムなピッチ変動(セント)。" },
            { kTooltipToneColor,      "シンセハーモニーの音色。" },
            { kTooltipAB,             "A/B: スロットAとBを切り替え。右クリックで保存。" },
            { kTooltipReset,          "再生位置をリセット。" },
            { kTooltipCheckUpdates,   "GitHubで最新のOpenVoxTunerリリースを確認。" },
            { kTooltipMorphDrag,      "スロットA（左）とスロットB（右）の間でモーフするにはドラッグ。\nダブルクリックで50%にスナップ。\n右クリックでオプション。" },
            { kTooltipMorphLabel,     "2つのプラグイン状態の間でモーフ。AまたはBをクリックしてスロットを設定し、スライダーをドラッグ。" },
            { kTooltipResetTransportDetail, "再生位置をリセット。\n内部のタイムラインオフセットをリセット（スタンドアロン/クラシックVST3で有用）。" },
            { kTooltipBypassIcon,     "オーディオ処理をバイパス。\n有効時、補正なしでオーディオが通過。" },
            { kTooltipMidiOutIcon,    "MIDI出力を有効にする" },
            { kTooltipMenuOptions,    "OpenVoxTunerオプション" },
            { kTooltipDebugWindow,    "デバッグウィンドウを開く：MIDIログ、アタック/リリーステスト" },
            { kTooltipUpdateAvailable, "最新のOpenVoxTunerリリースを開く。" },
            { kTooltipUpdateReleases, "OpenVoxTunerリリースページを開く。" },
            { kTooltipAbSlotA,        "クリック：スロットAをロード。右クリック：現在の状態を保存。" },
            { kTooltipAbSlotB,        "クリック：スロットBをロード。右クリック：現在の状態を保存。" },
            { kTooltipAutoScroll,     "再生中にエディタビューを自動スクロール" },
            { kTooltipUndo,           "元に戻す (Ctrl+Z)" },
            { kTooltipRedo,           "やり直す (Ctrl+Y)" },
            { kLegendInput,           "入力" },
            { kLegendOutput,          "出力" },
            { kLegendHarmony,         "ハーモニー" },
            { kLegendScrollHint,      "ホイール: スクロール" },
            { kLegendZoomHint,        "Ctrl+ホイール: ズーム" },
            { kLegendInTune,          "音程正確" },
            { kStatusChecking,        "確認中..." },
            { kStatusUpdateAvailable, "アップデート可能" },
            { kStatusUpToDate,        "最新です" },
            { kStatusUpdateFailed,    "アップデート確認失敗" },
            { kStatusUpdatePrefix,    "アップデート " },
            { kHelpTitle,             "キーボードショートカットとマウス操作" },
            { kHelpCloseHint,         "クリックして閉じる" },
            { kHelpMouseWheel,        "縦にスクロール（ピッチ）" },
            { kHelpCtrlWheel,         "ズームイン/アウト" },
            { kHelpClickDrag,         "カーブポイントを移動" },
            { kHelpDoubleClick,       "新しいカーブポイントを追加" },
            { kHelpRightClick,        "カーブポイントを削除" },
            { kHelpCopy,              "選択ポイントをコピー" },
            { kHelpPaste,             "コピーしたポイントを貼り付け" },
            { kHelpDelete,            "選択ポイントを削除" },
            { kHelpUndo,              "元に戻す" },
            { kHelpRedo,              "やり直す" },
            { kHelpToggleHelp,        "このヘルプを表示/非表示" },
            { kDlgExport,             "エクスポート" },
            { kDlgExportPng,          "ビジュアライザーをPNGにエクスポート" },
            { kDlgExportNotFound,     "ビジュアライザーコンポーネントが見つかりません。" },
            { kDlgImageSaved,         "画像を保存しました:\n" },
            { kDlgImageFailed,        "画像の保存に失敗しました。" },
            { kDlgSavePreset,         "プリセットを保存" },
            { kDlgSavePresetDesc,     "現在のカーブエディタ設定をカスタムプリセットとして保存。" },
            { kDlgSave,               "保存" },
            { kDlgInvalidName,        "無効な名前" },
            { kDlgEmptyName,          "プリセット名は空にできません。" },
            { kDlgOverwrite,          "プリセットを上書きしますか？" },
            { kDlgOverwriteDesc,      "この名前のプリセットが既に存在します。\n上書きしますか？" },
            { kDlgOverwriteBtn,       "上書き" },
            { kDlgPresetSaved,        "プリセットを保存しました" },
            { kDlgPresetSavedDesc,    "カスタムプリセットを保存しました:\n" },
            { kDlgSaveFailed,         "保存失敗" },
            { kDlgSaveFailedDesc,     "プリセットファイルを書き込めませんでした。" },
            { kDlgDeletePreset,       "プリセットを削除しますか？" },
            { kDlgDeletePresetDesc,   "このカスタムプリセットを完全に削除しますか？\n" },
            { kDlgDelete,             "削除" },
            { kDlgDeleteFailed,       "削除失敗" },
            { kDlgDeleteFailedDesc,   "プリセットファイルを削除できませんでした。権限を確認してください。" },
            { kDlgPresetDeleted,      "プリセットを削除しました" },
            { kDlgPresetDeletedDesc,  "カスタムプリセットを削除しました:\n" },
            { kDlgMidiLearn,          "MIDI学習" },
            { kDlgMidiLearnDesc,      "MIDIコントローラーを動かしてこのパラメータに割り当て。\nEscapeでキャンセル。" },
            { kDlgOk,                 "OK" },
            { kDlgDelete_,            "削除" },
            { kMidiLearnSpeed,        "スピード" },
            { kMidiLearnAmount,       "量" },
            { kMidiLearnFormant,      "フォルマント" },
            { kMidiLearnReverbMix,    "リバーブミックス" },
            { kMidiLearnFlexTune,     "FlexTune" },
            { kMidiLearnHumanize,     "人性化" },
            { kMidiLearnHarmonyGain,  "ハーモニー音量" },
            { kMidiLearnHarmonyBlend, "ハーモニーブレンド" },
            { kMidiLearnHarmonyTone,  "ハーモニー音色" },
            { kHintScrollZoom,        "ホイール: スクロール | " },
            { kHintZoom,              "+ホイール: ズーム" },
            { kHintAddPoint,          "ダブルクリック: ポイント追加 | 右クリック: カーブプリセット" },
            { kHintLiveMode,          "ライブモード: カーブエディタに切り替えて編集" },
        };

        switch (lang)
        {
            case Language::English:  return english;
            case Language::French:   return french;
            case Language::German:   return german;
            case Language::Spanish:  return spanish;
            case Language::Japanese: return japanese;
        }
        return english;
    }

    /** Translate a key to the current language. Falls back to English. */
    inline juce::String tr (const char* key)
    {
        const auto& map = getTranslations (currentLanguage());
        auto it = map.find (key);
        if (it != map.end()) return it->second;
        // Fallback to English
        const auto& en = getTranslations (Language::English);
        auto enIt = en.find (key);
        if (enIt != en.end()) return enIt->second;
        return key;
    }
}
