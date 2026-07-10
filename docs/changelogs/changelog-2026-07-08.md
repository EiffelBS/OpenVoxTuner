# Changelog - 2026-07-08

## New Features

### Waveform Display Types
- **Unified waveform rendering across Live and Curve Editor views**: Added `ovt::WaveformDisplayType` enum and `ovt::drawWaveformOverlay()` shared rendering function in OVTTheme.h. Both PitchVisualizer (Live tab) and PitchCurveEditor (Curve Editor tab) now use the same rendering function with a consistent amplitude scale (`halfH = plotArea.getHeight() * 0.35f`), replacing their separate inline implementations.
- **Four user-selectable display modes**: Bars (min/max per pixel, original visualizer style), Filled (envelope path, original curve editor style), Line (waveform outline only), and Mirror (symmetric bars around center). Selectable via hamburger menu "Waveform Display" submenu.
- **Persisted preference**: Waveform display type is saved/restored in the plugin state XML, persisting across DAW sessions.

## Bug Fixes

### Theme / Visual Consistency
- **Fixed background color mismatch between header banner and tab bar**: The header banner used `bgPanel().withAlpha(0.8f)` (semi-transparent) while the tab bar used `bgDark()` (opaque), causing a visible color difference. Now both use `bgDark()` consistently.
- **Fixed scale lines invisible in light mode**: `grid()` and `scaleLine()` in OVTTheme.h still had light-mode branches returning dark colors on a forced-dark background. Now they always return dark-mode values.
- **Fixed CPU meter invisible in light mode**: CPU meter background was semi-transparent white (`0x22ffffff`), invisible on light backgrounds. Now uses opaque dark background (`0xff222230`).
- **Fixed Measures label/combo not visible after theme switch**: The Measures controls in Curve Editor used theme-adaptive colors (`ovt::bgPanel()`, `ovt::text()`), but the Curve Editor is always dark. Now uses fixed dark-mode colors.
- **Fixed Auto-Scroll toggle invisible after ARA mode activation**: `setAutoScrollVisible(true)` made the toggle visible but didn't call `resized()` to update its bounds, leaving it at (0,0,0,0). Now `resized()` is always called after visibility change.
- **Unified dark mode gray palette**: Standardized all dark mode backgrounds to exactly 3 grays: `#26282B` (darkest - main background), `#373A3E` (medium - panels, cards, tabs), `#868686` (lighter - dimmed text, secondary text). Eliminated inconsistent gray values that caused subtle color mismatches across the UI.
- **Darkened combo dropdown menus**: PopupMenu (combo dropdown list) background now uses `bgDark()` instead of `bgPanel()`, matching the plugin background for a more cohesive look.
- **Updated visualizer header and legend background**: Changed to `#191B1E` for a consistent dark accent in the Live tab.
- **Removed background gradient in dark mode**: Replaced the vertical gradient (`bgDark().brighter(0.05f)` to `bgDark().darker(0.2f)`) with a flat `bgDark()` fill, eliminating an unwanted visual effect between header and tab bar areas.
- **Fixed semi-transparent block panel backgrounds**: Changed block panel fill from `bgPanel().withAlpha(0.6f)` to opaque `bgPanel()` for consistent, solid panel appearance.

### Export
- **Fixed "Could not find the visualizer component" error on export**: The export code tried to find PitchVisualizer inside a Viewport, but tab content is added directly to TabbedComponent without a Viewport wrapper. Now casts the tab content directly.

### Localization
- **Fixed tooltips not translated when changing language**: 16 tooltips (Bypass, MIDI Out, Correction Mode, Harmony, Reverb, Formant, FlexTune, Humanize, Tone Color, A/B, Reset, etc.) were hardcoded in English and never refreshed on language change. All are now translatable in 5 languages and refreshed via `refreshLabels()`.
- **Fixed scale mapping mismatch between UI and DSP**: The scale keys in OVTLanguages.h did not match the DSP Scale enum order. Removed obsolete keys (`kScaleNone`, `kScaleMinor`, `kScalePentatonic`, `kScaleWholeTone`, `kScaleDiminished`) and added missing keys (`kScaleMelodicMinor`, `kScaleHarmonicMinor`, `kScaleNaturalMinor`, `kScaleMajorPentatonic`, `kScaleMinorPentatonic`). Updated all 5 language translation maps (English, French, German, Spanish, Japanese) and the scale combo box keys array in PluginEditor.cpp to match the DSP enum order exactly.

## New Features

### Curve Editor
- **Added scale note lines to Curve Editor**: Horizontal reference lines for notes in the current scale are now drawn in the Curve Editor plot area (previously only in the Live visualizer). Lines are skipped for C notes (already drawn as octave grid lines).

### ARA2
- **Implemented ARA2 Waveform Overlay**: Captures input audio from processBlock when running in ARA mode, caches a mono downmix, and displays it as a semi-transparent background overlay in the Live visualizer. Toggle via "Show Waveform" menu item (visible only in ARA mode). Thread-safe double-buffering with CriticalSection lock.

### PresetMorpher Interpolation Engine
- **Added `PresetMorpher.h` header-only interpolation engine** in the `atdsp` namespace. Captures snapshots of all interpolable plugin parameters (continuous, discrete, boolean) and the PitchCurve into a `MorphState` struct.
- **Continuous parameter interpolation**: Linear interpolation for speed, amount, formant, harmony gain/blend/tone color, reverb mix, flex tune, and humanize.
- **Discrete/boolean parameter stepping**: Key, scale, harmony type/tone/shifted voices, latency mode, editor measures, and all boolean flags snap at the 50% morph threshold.
- **PitchCurve resampling and interpolation**: Resamples two PitchCurves to 128 samples on a normalized [0,1] time range, then lerps between them. Step mode is copied from whichever curve dominates.
- **XML state loading**: `loadStateFromXml()` reconstructs a `MorphState` from base64-encoded plugin state XML used by A/B slots, parsing all parameters and the PitchCurve.
- **Applied state output**: `applyInterpolatedState()` writes interpolated values back to an `AudioProcessorValueTreeState` via `setValueNotifyingHost`.

## Files Modified
- `Source/ui/OVTTheme.h` - Forced `grid()` and `scaleLine()` to always use dark values; improved CPU meter colors; updated dark mode grays (`bgDark` = #26282B, `bgPanel`/`headerBg` = #373A3E, `textDim` = #868686, `vizHeaderBg`/`vizLegendBg` = #191B1E)
- `Source/PluginEditor.cpp` - Fixed banner background, export code, Measures theme, tooltip refresh in `refreshLabels()`; removed background gradient; updated scale combo box keys array
- `Source/ui/OVTLanguages.h` - Added tooltip translation keys; fixed scale keys to match DSP enum (14 scales)
- `Source/ui/PitchCurveEditor.h` - Added `scaleIntervals` member
- `Source/ui/PitchCurveEditor.cpp` - Added scale line drawing, fixed Auto-Scroll visibility bug
- `Source/ui/LookAndFeel.cpp` - Combo dropdown uses darker background (`bgDark()`)
- `Source/ui/PitchVisualizer.cpp` - Uses `ovt::vizLegendBg()` for legend block
- `Source/PluginProcessor.h` - Added ARA waveform cache members and accessors
- `Source/PluginProcessor.cpp` - Added ARA waveform capture in processBlock and copyAraWaveform method
- `docs/ara2-waveform-overlay-plan.md` - ARA2 waveform overlay implementation plan (new)
- `Source/ui/OVTTheme.h` - Added `WaveformDisplayType` enum and `drawWaveformOverlay()` shared rendering function
- `Source/PluginProcessor.h` - Added `waveformDisplayType` member and public accessors (`getWaveformDisplayType`/`setWaveformDisplayType`)
- `Source/PluginProcessor.cpp` - Persist waveform display type in `getStateInformation`/`setStateInformation`
- `Source/ui/PitchVisualizer.h` - Added `currentDisplayType` member and `setDisplayType()` method
- `Source/ui/PitchVisualizer.cpp` - Replaced inline waveform rendering with shared `ovt::drawWaveformOverlay()` call
- `Source/ui/PitchCurveEditor.h` - Added `currentDisplayType` member and `setDisplayType()` method
- `Source/ui/PitchCurveEditor.cpp` - Replaced inline waveform rendering with shared `ovt::drawWaveformOverlay()` call
- `Source/PluginEditor.h` - Added `setWaveformDisplayType()` declaration and `lastWaveformDisplayType` member
- `Source/PluginEditor.cpp` - Added "Waveform Display" submenu in hamburger menu, `setWaveformDisplayType()` implementation, and sync logic in `timerCallback()`
- `Source/dsp/PresetMorpher.h` - New header-only interpolation engine for preset morphing (new)
