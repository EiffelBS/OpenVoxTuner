# Changelog - 2026-07-09

## UI Improvements

### A/B Comparison Redesign
- **Replaced single toggle button with two separate A/B buttons**: The old `abButton` that toggled between A and B states has been replaced with two independent buttons (`buttonA` and `buttonB`). Each button directly loads its respective slot on click.
- **Morph slider positioned between A and B buttons**: Layout changed from `[abButton] [morphSlider] [menuButton]` to `[buttonA] [morphSlider] [buttonB] [menuButton]` in the top-right header area.
- **Morph slider visibility is automatic**: The morph slider is only visible when both slot A and slot B have valid data. It auto-hides when either slot is empty.
- **Direct click behavior replaces toggle**: Clicking button A loads slot A, clicking button B loads slot B. No more automatic save-before-switch behavior.
- **Right-click context menus per button**: Right-clicking button A shows "Save current to Slot A", right-clicking button B shows "Save current to Slot B". Each button manages its own save action.
- **Green border indicates valid data**: Button A always shows a green border (it holds the default state), button B shows green only when slot B has been saved.
- **Active slot has bright fill background**: The currently loaded slot's button gets a semi-transparent blue fill; the inactive slot is transparent.
- **Removed `toggleAB()` method**: Replaced by direct click handlers on each button.
- **Removed `morphSourceLabel` and `morphTargetLabel`**: These source/target text labels next to the morph slider have been removed, replaced by the A and B buttons themselves serving as visual indicators.
- **Added `updateABButtonStates()` helper**: Centralizes button visual state updates and morph slider visibility logic. Called after load, save, and morph reset operations.

## Files Modified
- `Source/PluginEditor.h` - Replaced `abButton` with `buttonA`/`buttonB`, removed `toggleAB()` declaration, added `updateABButtonStates()` declaration, fixed `ABTextButton::paint()` compilation errors (`outlineColourId` -> `TextButton::buttonColourId`, `fillRoundedRectangle` Colour parameter fix)
- `Source/PluginEditor.cpp` - New AB button setup in constructor, updated `resized()` layout, removed `toggleAB()` method, removed all `morphSourceLabel`/`morphTargetLabel` references, updated `applyThemeToAllComponents()` and `refreshLabels()` for new buttons, added `updateABButtonStates()` implementation

## Internationalization (i18n) - Full UI Localization

### Overview
All remaining hardcoded English strings in the UI have been replaced with `ovt::tr()` calls using the existing i18n system. This ensures the entire plugin UI is translatable across all 5 supported languages (English, French, German, Spanish, Japanese).

### New Translation Keys Added (~100+ keys)
- **Tooltips** (15 new): `kTooltipCheckUpdates`, `kTooltipMorphDrag`, `kTooltipMorphLabel`, `kTooltipResetTransportDetail`, `kTooltipBypassIcon`, `kTooltipMidiOutIcon`, `kTooltipMenuOptions`, `kTooltipDebugWindow`, `kTooltipUpdateAvailable`, `kTooltipUpdateReleases`, `kTooltipAbSlotA/B`, `kTooltipAutoScroll`, `kTooltipUndo/Redo`
- **Button text** (11 new): `kLabelHarmonyBtn`, `kLabelFormantBtn`, `kLabelReverbBtn`, `kLabelTone`, `kLabelModernBtn`, `kLabelTransparentBtn`, `kLabelBypassBtn`, `kLabelMidiOutBtn`, `kLabelDebug`, `kLabelAutoScroll`, `kLabelMeasures`
- **Menu items** (23 new): Latency modes, waveform display, MIDI Learn, keyboard shortcuts, reset confirmation, morph context menu, preset management
- **Status messages** (5 new): Update checker states
- **Help overlay** (13 new): Keyboard shortcuts title, all shortcut descriptions, close hint
- **Dialogs/Alerts** (26 new): Export, save/delete preset, MIDI Learn dialogs
- **MIDI Learn params** (9 new): Parameter names for MIDI CC assignment
- **PitchCurveEditor hints** (4 new): Scroll/zoom hints, live mode hint
- **CPU display** (1 new): `kLabelCpu`

### Files Modified
- `Source/ui/OVTLanguages.h` - Added all new translation keys and translations in all 5 languages
- `Source/PluginEditor.cpp` - Replaced ~80 hardcoded English strings with `ovt::tr()` calls. Updated `refreshLabels()` to refresh all translatable button text, tooltips, tab names, and curve editor labels on language change.
- `Source/ui/PitchVisualizer.cpp` - Replaced 5 hardcoded legend strings with `ovt::tr()` calls (`kLegendInput`, `kLegendOutput`, `kLegendHarmony`, `kLegendScrollHint`, `kLegendZoomHint`)
- `Source/ui/PitchCurveEditor.cpp` - Added `#include "OVTLanguages.h"`, replaced hardcoded labels/tooltips with `ovt::tr()` calls, added `refreshTranslations()` method for language change support
- `Source/ui/PitchCurveEditor.h` - Added `refreshTranslations()` public method declaration

## Morphing Tooltip Localization Fix

### Issues Fixed
- **"Morph" label text was hardcoded**: The morph slider label displayed "Morph" as a hardcoded English string. Now uses `ovt::tr(ovt::Keys::kLabelMorph)` for proper localization.
- **Morph tooltips not refreshed on language change**: The `morphSlider` and `morphSliderLabel` tooltips were set in the constructor but never updated when the user changed language. Added refresh calls in `refreshLabels()` so both tooltips and the label text update dynamically.
- **Missing translation key**: Added `kLabelMorph` key with translations in all 5 languages (EN: "Morph", FR/DE/ES: "Morph", JA: "MORFU").

## New Latency Mode: Direct Monitoring (10 ms)

### Technical Feasibility
- The `PitchShifter` already accepts latencies down to **8 ms** (clamped in `PitchShifter::setLatencyMs()`). The previous minimum was 12 ms ("Low Latency").
- At 10 ms, the PSOLA grain center search has sufficient room for stable cross-correlation. Audio quality at 10 ms is comparable to 12 ms with minimal perceptible degradation.
- The new mode is compatible with all supported sample rates (44.1 kHz, 48 kHz, 96 kHz, 192 kHz) and both VST3/Standalone hosts.

### Latency Mode Changes
| Mode | Index | Latency (ms) | Use Case |
|------|-------|-------------|----------|
| Direct Monitoring | 0 | **10.0** | Real-time monitoring, minimal delay |
| Low Latency | 1 | **12.0** | Fast response, slight quality trade-off |
| Quality (default) | 2 | **20.0** | Best balance (previous default) |
| Safe | 3 | **30.0** | Maximum quality, highest latency |

### Files Modified
- `Source/ui/OVTLanguages.h` - Added `kMenuDirectMonitoring` key and `kLabelMorph` key with 5-language translations
- `Source/PluginEditor.cpp` - Updated latencyModeBox to 4 items, updated latency submenu with "Direct Monitoring" entry, fixed morph label localization, added morph tooltip refresh in `refreshLabels()`
- `Source/PluginProcessor.cpp` - Updated parameter choice to 4 options, adjusted latency values (10/12/20/30 ms), updated mode name logging
- `Source/dsp/PresetMorpher.h` - Updated normalization factors from `/2.0` to `/3.0` for 4-mode support

## A/B Comparison & Morphing System — Bug Fixes

### Bug 1: Parameters not restored when switching A/B slots

**Root cause:** `loadSlot()` called `resetMorph()` which applied the `morphSource` parameters to the processor via `applyInterpolatedState()` *before* `setStateInformation()` restored the target slot's data. This caused an intermediate state where parameters were set to the wrong values. Although `setStateInformation()` subsequently overwrote them, the `resetMorph()` call also cleared `morphSource` and `morphTarget`, destroying the morph state needed for future slider dragging.

**Fix:** Removed the `resetMorph()` call from `loadSlot()`. The caller (button onClick handler) now manages morph state directly:
- Clears old morph state before loading
- Loads the slot (which only does `setStateInformation` + slider resync)
- Sets up new morphSource/morphTarget after loading for future morphing

### Bug 2: Morph slider not functional

**Root cause:** The old code set `morphSource` to the "other" slot and `morphTarget` to the "clicked" slot before loading. After `loadSlot()` cleared everything via `resetMorph()`, the auto-capture in `onMorphSliderChanged` would capture stale states from the wrong slots.

**Fix:** After loading, `morphSource` is set to the clicked slot (current position) and `morphTarget` to the other slot (drag destination). This ensures dragging the slider correctly morphs between the two slots.

### Bug 3: Morph slider position resets to left on B click

**Root cause:** `morphSlider.setValue(1.0)` triggered `onMorphSliderChanged(1.0)` synchronously, which called `applyInterpolatedState()` and modified parameters before the slot data was fully loaded. The `switchingSlot` flag now prevents this.

**Fix:** Added a `switchingSlot` flag that suppresses `onMorphSliderChanged` during slot loading. The slider is positioned with `juce::dontSendNotification` after the slot data is restored.

### Files Modified
- `Source/PluginEditor.h` - Added `switchingSlot` flag member
- `Source/PluginEditor.cpp` - Rewrote `setupABButton` onClick handler, removed `resetMorph()` from `loadSlot()`, added `switchingSlot` guard in `onMorphSliderChanged()`

## A/B Buttons & Morph Slider UX Improvements

### Right-click save on A/B buttons
- **Issue:** The `ABTextButton` class had an `onRightClick` callback mechanism, but it was never assigned. The tooltip promised "Right-click: save current state" but right-click did nothing.
- **Fix:** Assigned `onRightClick` handlers to both buttons that call `saveSlot()` and refresh the A/B button states. Right-clicking A or B now saves the current plugin state into that slot.

### Morph slider track visibility
- **Issue:** The morph slider's unfilled portion (right of the thumb) was invisible because `backgroundColourId` was set to `bgDark()` (nearly identical to the plugin background), and the filled track used `accent().withAlpha(0.3f)` (too transparent).
- **Fix:** Added a custom `drawLinearSlider()` override in `AutotuneLookAndFeel` that draws the full track background with a visible but subtle color (`bgPanel().brighter(0.15f)`), the filled portion with accent at 70% opacity, and a circular thumb. Also changed `backgroundColourId` to `bgPanel()` for better contrast.

### Files Modified
- `Source/PluginEditor.cpp` - Added `onRightClick` assignments for A/B buttons, updated morph slider colors
- `Source/ui/LookAndFeel.h` - Added `drawLinearSlider()` override declaration
- `Source/ui/LookAndFeel.cpp` - Added `drawLinearSlider()` implementation with proper track rendering

## A/B Slot Loading — Robust Attachment Reset

### Issue
Previous fix (removing `resetMorph()` from `loadSlot()`) was insufficient. The root cause was that JUCE `SliderAttachment` / `ButtonAttachment` objects bound to old parameter objects would fire asynchronous `parameterChanged` callbacks after `parameters.replaceState()`, reverting sliders to stale values.

### Fix
`loadSlot()` now follows a strict sequence:
1. **Destroy all 28 attachments** (reset unique_ptrs) before calling `setStateInformation()`
2. **Restore state** via `setStateInformation()` which calls `parameters.replaceState()`
3. **Recreate all attachments** bound to the new parameter objects
4. **Force-resync** all sliders as a safety net

## New Feature: Noise Gate

### Overview
Added a noise gate effect on the input audio, applied before pitch detection. This reduces background noise between vocal phrases and improves YIN pitch detection accuracy.

### DSP Implementation
- **`Source/dsp/NoiseGate.h`** — Header-only noise gate class in the `atdsp` namespace
  - RMS-based level detection across all channels (mono detection, stereo gain)
  - Smooth per-sample gain ramping: 5ms attack, 50ms release (click-free operation)
  - `prepare(sampleRate)`, `setEnabled(bool)`, `setThresholdDb(float)`, `process(buffer)`

### Parameters
| Parameter | ID | Type | Range | Default |
|-----------|----|----|-------|---------|
| Noise Gate enable | `noise_gate_enable` | Bool | ON/OFF | OFF |
| Gate Threshold | `noise_gate_threshold` | Float | -80 to 0 dB | -40 dB |

### UI Controls
- Toggle button "Gate" (same style as Formant/Reverb)
- Rotary knob for threshold (-80 to 0 dB)
- Label "Threshold" below the knob

### Processing Chain Position
Waveform capture → **Noise Gate** → Pitch detection (YIN) → Pitch correction / Harmony

### Integration
- A/B comparison: noise gate state saved/restored in MorphState
- Preset morphing: threshold interpolated, enable toggled at 50%
- i18n: keys `kLabelNoiseGate`, `kTooltipNoiseGate`, `kLabelThreshold`, `kTooltipThreshold` in all 5 languages

### Files Modified
- `Source/dsp/NoiseGate.h` — New file
- `Source/dsp/PresetMorpher.h` — Added noise gate to MorphState, captureState, applyInterpolatedState, loadStateFromXml
- `Source/PluginProcessor.h` — Added noiseGate member and param pointers
- `Source/PluginProcessor.cpp` — Added parameters, prepare, processBlock integration
- `Source/PluginEditor.h` — Added UI members and attachments
- `Source/PluginEditor.cpp` — Added UI setup, layout, refreshLabels, applyTheme
- `Source/ui/OVTLanguages.h` — Added 4 translation keys × 5 languages
5. Cleaned up unused `modeAttachment` (dead code referencing non-existent `modeBox`)
