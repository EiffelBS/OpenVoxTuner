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
        static const char* kTabLive            = "tab.live";
        static const char* kTabCurveEditor     = "tab.curve_editor";
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
        static const char* kTooltipZoomIn      = "tooltip.zoom_in";
        static const char* kTooltipZoomOut     = "tooltip.zoom_out";
        static const char* kTooltipScrollUp    = "tooltip.scroll_up";
        static const char* kTooltipScrollDown  = "tooltip.scroll_down";
        static const char* kTooltipResetView   = "tooltip.reset_view";
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
        static const char* kTooltipToneColor   = "tooltip.tone_color";
        static const char* kTooltipAB          = "tooltip.ab_comparison";
        static const char* kTooltipReset       = "tooltip.reset_transport";
        static const char* kLegendInput        = "legend.input";
        static const char* kLegendOutput       = "legend.output";
        static const char* kLegendHarmony      = "legend.harmony";
        static const char* kLegendScrollHint   = "legend.scroll_hint";
        static const char* kLegendZoomHint     = "legend.zoom_hint";
        static const char* kLegendInTune       = "legend.in_tune";
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
            { kTabLive,            "Live" },
            { kTabCurveEditor,     "Curve Editor" },
            { kLabelSpeed,         "Speed (ms)" },
            { kLabelAmount,        "Amount" },
            { kLabelScale,         "Scale" },
            { kLabelRoot,          "Root" },
            { kLabelVolume,        "Volume" },
            { kLabelBlend,         "Blend" },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humanize" },
            { kLabelFormant,       "Formant" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Harmony" },
            { kTooltipZoomIn,      "Zoom In (narrower range)" },
            { kTooltipZoomOut,     "Zoom Out (wider range)" },
            { kTooltipScrollUp,    "Scroll Up (higher pitches)" },
            { kTooltipScrollDown,  "Scroll Down (lower pitches)" },
            { kTooltipResetView,   "Reset Zoom and Scroll" },
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
            { kUseVoice,              "Use Voice" },
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
            { kLegendInput,           "Input" },
            { kLegendOutput,          "Output" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Wheel: Scroll" },
            { kLegendZoomHint,        "Ctrl+Whl: Zoom" },
            { kLegendInTune,          "in tune" },
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
            { kTabLive,            "Live" },
            { kTabCurveEditor,     "Editeur de courbe" },
            { kLabelSpeed,         "Vitesse (ms)" },
            { kLabelAmount,        "Intensite" },
            { kLabelScale,         "Gamme" },
            { kLabelRoot,          "Tonalite" },
            { kLabelVolume,        "Volume" },
            { kLabelBlend,         "Mixage" },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humaniser" },
            { kLabelFormant,       "Formant" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Harmonie" },
            { kTooltipZoomIn,      "Zoom avant (plage etroite)" },
            { kTooltipZoomOut,     "Zoom arriere (plage large)" },
            { kTooltipScrollUp,    "Defiler vers le haut (aigus)" },
            { kTooltipScrollDown,  "Defiler vers le bas (graves)" },
            { kTooltipResetView,   "Reinitialiser zoom et defilement" },
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
            { kUseVoice,              "Utiliser voix" },
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
            { kLegendInput,           "Entree" },
            { kLegendOutput,          "Sortie" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Molette: Defiler" },
            { kLegendZoomHint,        "Ctrl+Moll: Zoom" },
            { kLegendInTune,          "accorde" },
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
            { kTabLive,            "Live" },
            { kTabCurveEditor,     "Kurveneditor" },
            { kLabelSpeed,         "Geschwindigkeit (ms)" },
            { kLabelAmount,        "Intensitaet" },
            { kLabelScale,         "Tonleiter" },
            { kLabelRoot,          "Grundton" },
            { kLabelVolume,        "Lautstaerke" },
            { kLabelBlend,         "Mischung" },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humanisieren" },
            { kLabelFormant,       "Formant" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Harmonie" },
            { kTooltipZoomIn,      "Hineinzoomen (schmaeler Bereich)" },
            { kTooltipZoomOut,     "Herauszoomen (breiterer Bereich)" },
            { kTooltipScrollUp,    "Nach oben scrollen (hoehere Toene)" },
            { kTooltipScrollDown,  "Nach unten scrollen (tiefere Toene)" },
            { kTooltipResetView,   "Zoom und Scroll zuruecksetzen" },
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
            { kUseVoice,              "Stimme verwenden" },
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
            { kLegendInput,           "Eingang" },
            { kLegendOutput,          "Ausgang" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Rad: Scrollen" },
            { kLegendZoomHint,        "Strg+Rad: Zoom" },
            { kLegendInTune,          "stimmt" },
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
            { kTabLive,            "En vivo" },
            { kTabCurveEditor,     "Editor de curva" },
            { kLabelSpeed,         "Velocidad (ms)" },
            { kLabelAmount,        "Intensidad" },
            { kLabelScale,         "Escala" },
            { kLabelRoot,          "Tono base" },
            { kLabelVolume,        "Volumen" },
            { kLabelBlend,         "Mezcla" },
            { kLabelFlex,          "Flex" },
            { kLabelHumanize,      "Humanizar" },
            { kLabelFormant,       "Formante" },
            { kLabelReverb,        "Reverb" },
            { kLabelMix,           "Mix" },
            { kLabelHarmony,       "Armonia" },
            { kTooltipZoomIn,      "Acercar (rango estrecho)" },
            { kTooltipZoomOut,     "Alejar (rango amplio)" },
            { kTooltipScrollUp,    "Desplazar arriba (agudos)" },
            { kTooltipScrollDown,  "Desplazar abajo (graves)" },
            { kTooltipResetView,   "Restablecer zoom y desplazamiento" },
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
            { kUseVoice,              "Usar voz" },
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
            { kLegendInput,           "Entrada" },
            { kLegendOutput,          "Salida" },
            { kLegendHarmony,         "Harm." },
            { kLegendScrollHint,      "Rueda: Desplazar" },
            { kLegendZoomHint,        "Ctrl+Rueda: Zoom" },
            { kLegendInTune,          "afinado" },
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
            { kTabLive,            "ライブ" },
            { kTabCurveEditor,     "カーブエディタ" },
            { kLabelSpeed,         "スピード (ms)" },
            { kLabelAmount,        "量" },
            { kLabelScale,         "スケール" },
            { kLabelRoot,          "キー" },
            { kLabelVolume,        "音量" },
            { kLabelBlend,         "ブレンド" },
            { kLabelFlex,          "フレックス" },
            { kLabelHumanize,      "人性化" },
            { kLabelFormant,       "フォルマント" },
            { kLabelReverb,        "リバーブ" },
            { kLabelMix,           "ミックス" },
            { kLabelHarmony,       "ハーモニー" },
            { kTooltipZoomIn,      "ズームイン" },
            { kTooltipZoomOut,     "ズームアウト" },
            { kTooltipScrollUp,    "上にスクロール" },
            { kTooltipScrollDown,  "下にスクロール" },
            { kTooltipResetView,   "ビューをリセット" },
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
            { kUseVoice,              "ボイス使用" },
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
            { kLegendInput,           "入力" },
            { kLegendOutput,          "出力" },
            { kLegendHarmony,         "ハーモニー" },
            { kLegendScrollHint,      "ホイール: スクロール" },
            { kLegendZoomHint,        "Ctrl+ホイール: ズーム" },
            { kLegendInTune,          "音程正確" },
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
