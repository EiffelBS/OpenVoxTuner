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

## UI Bug: transport icons and "Measures" control overlapped the tabs

### Problem
The standalone transport (Play/Stop) buttons and the "Measures" label+combo were placed on the
left of the Curve Editor toolbar row and overlapped the "Live" / "Curve Editor" tab labels.

### Root cause
The toolbar overlay row is the top 30px of the `TabbedComponent` — i.e. the tab strip itself.
The right-aligned view/snap icons sat over the empty right side of the strip, so they never
clashed with the tabs. Moving the transport + Measures controls to the **left** of that same
row put them directly on top of the tab labels.

### Fix
Before laying out the left-aligned controls, the code now skips past the tab labels by measuring
the right edge of the last tab button (`TabbedButtonBar::getTabButton(numTabs-1)->getRight()`) and
removing that width (+ 6px gap) from the left of the toolbar area. The controls now start just
right of the "Curve Editor" tab and no longer overlap it.

### Files changed
- `Source/PluginEditor.cpp` — `resized()`: offset the toolbar's left group past the tab strip
  using `tabbedComponent.getTabbedButtonBar()`.

### Verification
- VST3 target builds cleanly (exit 0).
- In the Curve Editor tab, the Play/Stop buttons and the Measures control sit to the right of
  the "Curve Editor" tab label, with no overlap.

## Bug Fix: SnapToScale did not snap scale notes to their exact pitch (D4 / G4 / A#4)

### Problem
With SnapToScale ON in the Curve Editor, dragging or double-clicking a point near a
**scale** note (e.g. D4, G4, A#4 in C Natural Minor) did not "snap": the point stayed at
the exact clicked frequency instead of being pulled onto the note. Off-scale notes, by
contrast, visibly moved to the nearest scale note, so the snap appeared to work only for
non-scale notes.

### Root cause
`atdsp::PitchCurve::snapToIntervals()` (in `Source/dsp/PitchCurve.cpp`) returned the raw
clicked frequency whenever the note's pitch class was already a member of the interval set:

```cpp
if (intervals.contains (noteInOctave)) return hz;   // returned the raw clicked hz
```

So an in-scale note was left where it was clicked (a few cents off the true note), while
an out-of-scale note was corrected by the circular-distance branch. That asymmetry is
exactly what the user saw: scale notes "don't snap", off-scale notes do.

### Fix
When the note is already in the scale, the function now quantizes to the **exact** note
frequency (the same pitch the off-scale branch already returns) instead of echoing the raw
input:

```cpp
if (intervals.contains (noteInOctave))
    return 440.0f * std::pow (2.0f, (currentMidi - midiRef) / 12.0f);
```

`currentMidi` is the nearest semitone to the clicked pitch, so the point now lands exactly
on the scale note regardless of where inside the note's zone it was clicked. All 7 notes of
C Natural Minor (and every other scale) snap to their exact pitch.

### Files changed
- `Source/dsp/PitchCurve.cpp` — `snapToIntervals()` returns the exact note pitch for in-scale
  notes.
- `test/dsp/PitchCurveTest.cpp` — new regression test asserting that clicking slightly off
  D4 / G4 / A#4 (C Natural Minor) snaps to the exact note frequency, plus the existing
  out-of-scale and empty-set behaviours. Linked by the `OpenVoxTunerTests` target.

### Verification
- `cmake --build build --config Release --target OpenVoxTunerTests` then run `OpenVoxTunerTests.exe`:
  the `PitchCurve` suite (8 assertions) passes, including the new "Note de la gamme cliquee
  legerement a cote : SNAP vers la note exacte" case. (The 4 pre-existing YIN headless-audio
  failures are unrelated to this change.)
- Manual: in C Natural Minor with SnapToScale ON, points placed anywhere near D4 / G4 / A#4
  now lock to the exact note.

## UI: Standalone transport — single Play/Pause toggle + "Return to start" (rewind) button

### Change
Per user request, the standalone transport no longer uses two buttons (Play + Stop). It is
now a single **Play/Pause** toggle plus a **"Return to start"** (rewind) button:

- **Play/Pause toggle**: shows the Play glyph (and "Play" tooltip) when stopped and the Stop
  glyph (and "Stop" tooltip) when playing — i.e. the icon switches to its opposite action,
  like a typical media control. Clicking toggles `processorRef.setTransportPlaying()`.
- **"Return to start" (rewind)**: a vertical bar + left-pointing triangle glyph (classic DAW
  skip-to-beginning icon). It calls `processorRef.resetTransportTime()` and
  `curveEditor->clearInputTrace()` — the same action as the Options-menu "Reset Playhead".

### Files changed
- `Source/PluginEditor.h` — replaced the `stopButton` member with `rewindButton`.
- `Source/PluginEditor.cpp`
  - Added the `svgRewind` icon (Lucide-style 24x24 SVG).
  - `playButton` is now set up manually with Play as the normal image set and Stop as the
    "on" image set; `onClick` toggles transport state.
  - New `rewindButton` (non-toggle) wired to `resetTransportTime()` + `clearInputTrace()`.
  - `resized()` / visibility / `toFront` / `refreshLabels` updated: `rewindButton` replaces
    `stopButton`; `playButton` tooltip is driven by state in `syncTransportButtons()`.
  - `syncTransportButtons()` now swaps the Play/Stop glyph + tooltip from the processor's
    playing state (no `stopButton` toggle remains).
- `Source/ui/OVTLanguages.h` — added `kTooltipRewind` ("Return to start / reset playhead")
  with EN/FR/DE/ES/JA translations.

### Verification
- VST3 and Standalone targets build cleanly (exit 0).
- In Standalone Curve Editor: the toolbar shows [Play/Pause][Rewind] (left), then Measures.
  Clicking Play switches the icon to Stop; clicking again switches back to Play. The rewind
  button resets the playhead and clears the input trace.


