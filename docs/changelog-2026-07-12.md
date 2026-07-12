# Changelog - 2026-07-12

## UI: Curve Editor toolbar rework (auto-scroll, Measures, Curve Presets submenu)

### Change
Continuation of the Curve Editor toolbar parity work. The toolbar row now carries
the curve-specific controls without the embedded widget block that used to overlap
the ruler.

- **Auto-Scroll** moved out of the embedded `ToggleButton`+label and into the
  **Options** menu as a ticked item. Toggling it keeps the editor's `autoScrollEnabled`
  state and persists it to the `auto_scroll` parameter (so it restores across sessions).
- **Measures** combo + label moved from the embedded editor controls onto the toolbar
  row (same line as the Options menu and the view icons). It no longer covers the ruler.
- **Curve Presets** is now a direct submenu of the Options menu (the `buildPresetsMenu()`
  refactor bakes the factory/custom/delete callbacks into a `juce::PopupMenu` instead of
  the old click-to-open `presetsButton`). Right-click on the editor still opens the same menu.
- The old embedded control members (`measuresBox`, `measuresLabel`, `autoScrollToggle`,
  `autoScrollVisible`, plus `getMeasuresBox()` / `getMeasuresLabel()` / `getAutoScrollToggle()`
  / `setAutoScrollVisible()` / `setMeasuresWithoutCombo()`) were removed from `PitchCurveEditor`.

### Files changed
- `Source/ui/PitchCurveEditor.h` / `Source/ui/PitchCurveEditor.cpp`
  - Removed the embedded control members/accessors and their construction, `resized()`
    positioning, and `refreshTranslations` lines.
  - `setPlayheadTime()`: removed the `autoScrollVisible` gate; auto-scroll now applies
    whenever `autoScrollEnabled` is true (always available from the Options menu).
  - Kept `getAutoScroll()` / `setAutoScroll()`.
- `Source/PluginEditor.h`
  - Added toolbar members `measuresLabel`, `measuresComboBox`, `playButton`, `stopButton`
    and declarations `buildPresetsMenu()`, `syncTransportButtons()`.
- `Source/PluginEditor.cpp`
  - `buildPresetsMenu()` (replaces `showPresetsMenu` index dispatch) returns a
    `juce::PopupMenu` with all preset actions wired to lambdas.
  - `showCurveOptionsMenu()` rewritten: ticked Auto-Scroll item, Clean Curves, Reset
    Playhead (disabled when bound to ARA), Curve Presets submenu, and — in standalone —
    a Tempo submenu.
  - `resized()` toolbar layout extended with the Measures label+combo (left) and, in
    standalone, the Play/Stop buttons (left).
  - Visibility + `toFront` order + `refreshTranslations` updated for the new controls.

### Verification
- VST3 and Standalone targets build cleanly
  (`cmake --build build --config Release --target OpenVoxTuner_VST3` and
  `... --target OpenVoxTuner_Standalone`, exit 0).
- Curve Editor toolbar shows, left-to-right: [Play][Stop] (standalone) | Measures
  [combo] | snap/grid/step | zoom/scroll/reset | Options.
- Options menu shows Auto-Scroll (ticked), Clean Curves, Reset Playhead, Curve Presets.

## Feature: Standalone transport (Play / Stop) to freeze the timeline while editing

### Change
In Standalone mode, the Curve Editor toolbar now exposes **Play** / **Stop** buttons
(standalone only). Previously the standalone fallback transport always advanced, so the
playhead was constantly moving and the curve was hard to edit.

- `processorRef.setTransportPlaying(bool)` / `isTransportPlaying()` added to the processor.
- The standalone fallback in `processBlock` is now gated on `transportPlaying`: when
  stopped, `currentTime` is frozen so the user can edit the curve; when playing, it
  advances at the standalone tempo.
- `syncTransportButtons()` reflects the playing state on the two toolbar buttons.

### Files changed
- `Source/PluginProcessor.h` — added `transportPlaying` (std::atomic<bool>, default true)
  and the `setTransportPlaying` / `isTransportPlaying` accessors.
- `Source/PluginProcessor.cpp` — `processBlock` fallback advances `currentTime` only when
  `transportPlaying` is true.

## Feature: Standalone tempo (BPM) selection

### Change
In Standalone mode the Options menu gains a **Tempo** submenu (standalone only) listing
common BPM values (60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 180). Selecting one
calls `processorRef.setBpm(b)`; the current BPM is ticked. The standalone fallback now
advances at `bpm / 60` beats per second instead of being locked at 120 BPM.

### Files changed
- `Source/PluginProcessor.h` — added `bpm` (std::atomic<float>, clamped 20..400, default
  120) and the `setBpm` / `getBpm` accessors.
- `Source/PluginProcessor.cpp` — `processBlock` fallback advances at `bpm / 60` BPS.
- `Source/PluginEditor.cpp` — Tempo submenu in `showCurveOptionsMenu()` (standalone only).
- `Source/ui/OVTLanguages.h` — added keys `kMenuAutoScroll`, `kMenuTempo`, `kTooltipPlay`,
  `kTooltipStop` with EN/FR/DE/ES/JA translations.

### Verification
- Both targets build cleanly (exit 0).
- In Standalone, opening the Curve Editor Options menu shows the Tempo submenu; choosing a
  BPM changes the rate at which the standalone playhead advances. Play/Stop freeze and run
  the timeline so the curve can be edited while stopped.

## Bug Fix: Auto-Scroll ON/OFF behaved identically

### Problem
The Curve Editor "Auto-Scroll" option (Options menu) made no visible difference. With it
OFF, the view still scrolled during playback — it just snapped the playhead to the left edge
instead of keeping it centered. And while editing (timeline stopped), neither mode scrolled.

### Root cause
In `PitchCurveEditor::setPlayheadTime()`, the auto-scroll OFF branch continuously re-scrolled
the view to follow the playhead (`scrollOffset = time - timeVisible * 0.5`) whenever the time
changed by more than 0.01 — which is true on every playback frame. So OFF still tracked the
playhead; only the anchor point differed (left edge vs center), which is barely noticeable.

### Fix
The OFF branch no longer follows the playhead during continuous playback. A frame is classified
as a **manual seek** (Reset Playhead / DAW scrub) vs normal playback by the size of the transport
discontinuity (> 0.5 beat in one frame is never normal playback). Auto-scroll OFF now:
- keeps the view **fixed** during playback, so the playhead can run past the visible window
  (genuinely different from ON, which smoothly follows and keeps the playhead centered);
- only reveals the playhead on an explicit seek, so the user is never left looking at empty space.

The now-unused `stoppedPlayheadTime` member was removed from `PitchCurveEditor`.

### Files changed
- `Source/ui/PitchCurveEditor.cpp` — `setPlayheadTime()` rewritten; seek detection via the
  per-frame delta; OFF no longer scrolls during playback.
- `Source/ui/PitchCurveEditor.h` — removed `stoppedPlayheadTime`.

### Verification
- VST3 target builds cleanly (`cmake --build build --config Release --target OpenVoxTuner_VST3`, exit 0).
- Auto-scroll ON: the playhead stays centered and the view follows it during playback.
- Auto-scroll OFF: the view stays put while the song plays; the playhead moves and exits the
  visible window. A Reset Playhead jumps the view back to reveal the start.

