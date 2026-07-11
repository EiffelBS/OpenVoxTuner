# Changelog - 2026-07-11

## Bug Fix: A/B Morph automation overwrites concurrent parameter automation

### Problem
When the A/B morph (`morph_amount`) was automated by the DAW at the same time as
other parameters (e.g. `speed`, `amount`), the morph worked until its automation
lane started. From that point on, only the morph had any effect: the DAW
automation of `speed`/`amount` was ignored.

### Root cause
The morph is applied every audio frame from the editor's `timerCallback()` via
`onMorphSliderChanged()` → `atdsp::applyInterpolatedState()`. That function
overwrites **every** morphable parameter (speed, amount, formant, harmony_*,
reverb, etc.) with the interpolated source→target values. As soon as the morph
automation lane began moving, this per-frame write clobbered the values the DAW
was sending for `speed`/`amount`, so those lanes had no audible effect.

### Fix
`applyInterpolatedState()` now accepts an optional exclusion list. Before
applying the morph, the editor detects parameters that are currently being
driven externally (DAW automation or UI): a parameter is considered externally
driven when its live value differs from the value the morph last applied to it.
Such parameters are skipped so the morph crossfade and the concurrent
automation lanes coexist.

### Files changed
- `Source/dsp/PresetMorpher.h`
  - Added `atdsp::getMorphParameterIds()`.
  - Added optional `exclude` parameter to `applyInterpolatedState()`.
- `Source/PluginEditor.h`
  - Added `lastMorphIntendedValues` map (tracks the morph's last applied value per parameter).
- `Source/PluginEditor.cpp`
  - `onMorphSliderChanged()`: compute the external-automation exclusion set and
    pass it to `applyInterpolatedState()`; record applied values for the next frame.
  - Clear `lastMorphIntendedValues` on fresh morph capture and in `resetMorph()`.

### Verification
- VST3 target builds cleanly with `cmake --build build --config Release --target OpenVoxTuner_VST3`.
- With DAW automation on `speed` + `amount` + `morph`, all three lanes now
  remain effective simultaneously (the morph no longer freezes `speed`/`amount`).

## UI: Effects block reorganized into two rows of knobs

### Change
The Effects block (Block 4) previously showed Gate / Reverb / Formant as three
side-by-side columns that slightly overlapped. It now uses two rows:
- Row 1: Noise Gate + Reverb
- Row 2: Formant

### Details
- Removed the value textbox from the three effect knobs (Threshold, Reverb Mix,
  Formant Shift). Their value is now shown via the tooltip while dragging,
  matching the existing FlexTune / Humanize behaviour.
- Reduced the effect knobs and their power (enable) buttons by ~5% so the two
  rows fit without overlapping (power button 18px -> 17px).
- Tooltips for Reverb Mix and Formant sliders show the live value on drag
  (percentage and semitones respectively); Threshold shows dB.

### Files changed
- `Source/PluginEditor.cpp`
  - Effect knobs (Noise Gate, Reverb, Formant): `NoTextBox` + `onValueChange`
    tooltip showing the live value.
  - Block 4 layout: two-row arrangement (Gate+Reverb on top, Formant below),
    with a 5% smaller power button height.

### Verification
- VST3 target builds cleanly.
- Effects block renders as two non-overlapping rows; knob values visible via
  tooltip on drag.

## UI: Tooltip behavior for value-less knobs + missing tooltips

### Change
- Knobs without a value textbox (FlexTune, Humanize, Formant, Reverb Mix,
  Noise Gate Threshold) now show their live value in the tooltip **only while
  the user is dragging** the knob. On mouse release (`onDragEnd`), the normal
  descriptive tooltip is restored.
- Added missing tooltips to Speed, Amount, Harmony Volume and Harmony Blend
  knobs (which keep their value textbox and a persistent descriptive tooltip).

### Details
- New tooltip keys added to `OVTLanguages.h` (EN/FR/DE/ES/JA):
  `kTooltipSpeed`, `kTooltipAmount`, `kTooltipVolume`, `kTooltipBlend`.
- Effect knobs use `onDragStart`/`onValueChange` (value) + `onDragEnd`
  (descriptive tooltip restored via `ovt::tr`).

### Files changed
- `Source/ui/OVTLanguages.h` — new tooltip keys + 5-language translations.
- `Source/PluginEditor.cpp` — drag-only value tooltips for value-less knobs;
  persistent tooltips for Speed/Amount/Volume/Blend (setup + language refresh).

### Verification
- VST3 target builds cleanly.
- Dragging a value-less knob shows its value; releasing restores the normal
  tooltip. Speed/Amount/Volume/Blend now show a tooltip on hover.

## UI: Narrowed Root combo and Scale block

### Change
The Root (Key) combo only ever displays up to 2 characters (e.g. "C", "C#").
Its width was reduced from 80px to 56px, and the Scale/Keyboard block width
was narrowed from 260px to 236px by the same 24px, freeing horizontal space
for the Harmony block on the right.

### Files changed
- `Source/PluginEditor.cpp` — `scaleBlockWidth` 260 -> 236; Root `keyBox`
  width 80 -> 56 in Block 1 layout.

### Verification
- VST3 target builds cleanly.
- Root combo still shows note names without clipping; Scale block is 24px narrower.

## Bug Fix: Curve Editor "Snap to scale" snaps to the wrong scale

### Problem
In the Curve Editor, moving a point did not snap to the currently selected scale.
For example, in **C Natural Minor** (scale {C, D, Eb, F, G, Ab, Bb}), dragging a
point upward from C4 jumped to **E4** (which is not even in the scale) instead of
**D4** (which is in the scale).

### Root cause
The editor snapped against a **hard-coded interval table + a `currentScale`/`keyIdx`
pair** (`PitchCurve::snapToScale`), while the on-screen scale display was driven by a
**different** source: `processorRef.getScaleIntervals()` (the processor's
`ScaleQuantizer`). These two sources of truth could diverge — e.g. if
`currentScale`/`keyIdx` were not updated in lock-step with the selected scale/key,
the interactive snap used a different (often default `Major`) scale than the one
shown on screen. The math in `snapToScale` itself was correct (verified with a
standalone reproduction of C Natural Minor: C4 -> D4 -> Eb4), so the bug was purely
the dual-source-of-truth divergence between snapping and display.

### Fix
Introduced a single authoritative snap function `PitchCurve::snapToIntervals(hz,
intervals)` that snaps a frequency to the nearest note of an explicit interval set.
The Curve Editor now snaps every interactive gesture (single-point drag, selection
drag, double-click add, capture, and the re-snap on scale change) against its
`scaleIntervals` member — the **exact same interval set already used for the
on-screen scale lines** (sourced from `getScaleIntervals()`, already shifted by the
key). This guarantees the snap always matches the visible scale, eliminating the
divergence.
- Removed the now-unused `PitchCurve::snapToScale` and `PitchCurve::snapToScaleCustom`
  (and their declarations) to avoid keeping a second, divergent code path.
- The "snap off" magnetism branch now snaps to a full chromatic interval set via a
  small `getChromaticIntervals()` helper.

### Files changed
- `Source/dsp/PitchCurve.cpp`
  - Added `PitchCurve::snapToIntervals(hz, intervals)`.
  - Removed `PitchCurve::snapToScale` and `PitchCurve::snapToScaleCustom`.
- `Source/dsp/PitchCurve.h`
  - Declared `snapToIntervals`; removed `snapToScale` / `snapToScaleCustom` declarations.
- `Source/ui/PitchCurveEditor.cpp`
  - All snap sites (single-point drag, selection drag, double-click, `capturePitch`,
    `setKeyAndScale` re-snap) now use `atdsp::PitchCurve::snapToIntervals(hz, scaleIntervals)`.
  - `getChromaticIntervals()` helper for the "snap off" magnetism branch.

### Verification
- VST3 target builds cleanly (`cmake --build build --config Release --target OpenVoxTuner_VST3`, exit 0).
- In C Natural Minor, dragging a point upward from C4 now snaps C4 -> D4 -> Eb4
  (matching the displayed scale), never to an off-scale note like E4.

## Bug Fix: selecting a scale/key in the ComboBox did not update the parameter (root cause of "snap to wrong scale")

### Problem
After the previous snap fix, the Curve Editor snap was consistent with `scaleIntervals`,
but the user reported that in **C Natural Minor** only the notes C4, D#4, F4, G#4, C5
snapped — the notes D4, G4, A#4 did not. Those snapping notes are exactly the set
`{C, Eb, F, Ab}` = interval set `{0, 3, 5, 8}`. A standalone replication of
`ScaleQuantizer` for C Natural Minor confirmed the quantizer returns the 7 correct notes
`{0, 2, 3, 5, 7, 8, 10}`, while `Custom{0, 3, 5, 8}` returns exactly `{0, 3, 5, 8}`.
So at runtime the plugin was actually in **Custom mode with only 4 notes (C, Eb, F, Ab)
selected**, not Natural Minor — the dropdown *displayed* "Natural Minor" but the
underlying `scale` parameter was a different (stale/loaded) value.

### Root cause
In `PluginEditor.cpp`, the `Key`/`Scale` ComboBoxes are bound to the `key`/`scale`
parameters via `AudioProcessorValueTreeState::ComboBoxAttachment`. The attachment installs
its own callback on `comboBox.onChange`. However, the code immediately reassigned
`scaleBox.onChange = [...]` (and `keyBox.onChange = [...]`) afterwards, **overwriting the
attachment's callback**. As a result, changing the selection in the ComboBox updated only
the visible text but **never wrote the new value to the parameter**. The parameter kept
whatever value it was initialised/loaded with, so the engine and the snap used a scale/key
different from what the dropdown showed.

> **Correction (see follow-up entry below):** the statement above that "the attachment installs
> its callback on `comboBox.onChange` and was overwritten" is inaccurate. JUCE's
> `ComboBoxParameterAttachment` uses the `ComboBox::Listener` mechanism (`comboBoxChanged`), **not**
> the `onChange` callback, so reassigning `onChange` never broke the attachment. The genuine defect
> was that the combo -> parameter direction relied solely on the `onChange` handlers writing the
> parameter, and those handlers were not driving the engine correctly. The follow-up hardens the
> binding against morph/automation regressions.

### Fix

A second, related defect: the `key` parameter is an `AudioParameterInt` whose value is
**normalised [0, 1]** (confirmed by `PresetMorpher`, which does `getValue() * 11.0f`). The
key getters used `static_cast<int>(keyParam->load())`, i.e. they read the normalised value
as if it were the integer key — so only **root C (0)** resolved correctly; every other
root was silently treated as C. (Root cause of the "snap only reaches some notes" symptom
once the scale binding was fixed.)

### Fix
- In `PluginEditor.cpp`, the `scaleBox.onChange` and `keyBox.onChange` handlers now also push
  the selected index to the parameter:
  - scale: `getParameter("scale")->setValueNotifyingHost(idx / 13.0f)` (inverse of the
    existing read path `rawScale->load() * 13.0f`).
  - key:   `getParameter("key")->setValueNotifyingHost(idx / 11.0f)` (normalised, matching
    `PresetMorpher`'s convention).
  This restores the ComboBox -> parameter direction that the overwritten attachment used to
  provide. The parameter -> ComboBox direction still works via the attachment's parameter
  listener, so there is no feedback loop.
- Corrected all `key` getters to read the normalised value: `static_cast<int>(round(load * 11.0f))`
  instead of `static_cast<int>(load)`, in `PluginProcessor.cpp` (lines ~732, ~1150, ~1317,
  ~1682) and `PluginEditor.cpp` (~1973). Now every root (not just C) resolves correctly, so
  the snap and the audio engine use the selected key.

### Files changed
- `Source/PluginEditor.cpp`
  - `scaleBox.onChange`: push selected scale to the `scale` parameter; key getter uses
    normalised `rawKey->load() * 11.0f`.
  - `keyBox.onChange`: push selected key to the `key` parameter, then re-run the scale
    handler to refresh the piano keys.
  - `refreshVisualizer` key getter: `round(rawKey->load() * 11.0f)`.
- `Source/PluginProcessor.cpp`
  - Key getters at the 4 read sites corrected to `round(keyParam->load() * 11.0f)`.

### Verification
- VST3 target builds cleanly (exit 0).
- Selecting "Natural Minor" in the ComboBox now actually sets the `scale` parameter, so the
  Curve Editor snap offers all 7 notes (C, D, Eb, F, G, Ab, Bb) and the on-screen scale lines
  match the snap. Selecting a non-C root (e.g. G) now shifts the scale correctly in both the
  snap and the audio engine.

## Follow-up: accurate ComboBox/parameter binding mechanism + morph-regression hardening

### Context
Investigated the Root/Scale ComboBox display during an A/B morph (parameter -> combo direction)
after the previous binding fix. Verified the whole chain against the actual JUCE 8 source
(`C:/JUCE/modules/juce_audio_processors/utilities/juce_ParameterAttachments.{h,cpp}`).

### Findings (JUCE mechanism, verified in source)
- `ComboBoxParameterAttachment : private ComboBox::Listener`. The combo -> parameter direction on a
  **genuine user selection** is handled by `comboBoxChanged()` (the Listener), not by the `onChange`
  callback. So reassigning `comboBox.onChange` never "overwrote" the attachment (the earlier entry's
  root cause was imprecise — a correction note was added there).
- The parameter -> combo direction (`setValue`) computes `index = round(normValue * (numItems - 1))`
  and applies it with **`sendNotificationSync`**. JUCE wraps this in `ignoreCallbacks`, which suppresses
  only the **Listener** (`comboBoxChanged`); the `onChange` callback is **not** guarded. Therefore any
  `onChange` handler ALSO fires whenever the attachment drives the combo (morph / host automation).
- Mapping is correct: `kScaleNames` order == the `scale` `AudioParameterChoice` choices order (14
  entries), and `numItems - 1 == 13`; root `key` uses 12 items / `numItems - 1 == 11`. `key`/`scale`
  morph via `lerpOrStep` (step at 50%), so only clean integer indices are ever written — never
  fractional — so there is no snap-to-integer corruption concern.

### Problem this follow-up fixes
The previous binding fix made `keyBox.onChange` / `scaleBox.onChange` write the parameter back. Because
`onChange` fires during a morph (sendNotificationSync, unguarded), and `keyBox.onChange` calls
`scaleBox.onChange()` which wrote `comboDisplayedIndex / 13` to the `scale` parameter, a morph that
changes **both** key and scale at the crossfade could momentarily overwrite the morph's new scale with
the combo's still-stale displayed scale (the key async-update can run before the scale async-update).
Transient (corrected next frame) but a real coupling bug.

### Fix
- `keyBox.onChange` and `scaleBox.onChange` no longer write `key` / `scale` parameters. The combo ->
  parameter direction on genuine user selection is solely the attachment's `comboBoxChanged` Listener
  (verified idempotent and correct in JUCE source). This removes any chance of the handler fighting the
  morph / automation.
- The onChange handlers now only mirror the per-note custom flags / piano keys for whatever `scale` /
  `key` the combo currently shows. This is safe in every direction (user selection, morph, automation)
  because it never writes `key` / `scale` back, and `ScaleQuantizer::getScaleIntervals()` uses hardcoded
  intervals for preset scales (so the snap is unaffected by any transient custom-flag mirror state).

### Files changed
- `Source/PluginEditor.cpp`
  - `keyBox.onChange`: removed the `key` parameter write; only re-syncs custom flags/piano when the key
    changes (calls `scaleBox.onChange()`).
  - `scaleBox.onChange`: removed the `scale` parameter write; only re-syncs the per-note custom flags /
    piano for the selected preset scale (`Custom` index 13 is left untouched so the user's custom notes
    persist).
  - Rewrote the binding comments to describe the real JUCE mechanism (Listener vs onChange, guarded
    Listener vs unguarded onChange during morph/automation).

### Verification
- VST3 target builds cleanly (`cmake --build build --config Release --target OpenVoxTuner_VST3`, exit 0).
- User selection of a scale/key still updates the parameter (attachment Listener) and the snap/engine.
- During an A/B morph that changes both root and scale, the ComboBox display follows the morph target and
  the handlers no longer write `scale`/`key` back, so there is no transient scale overwrite.

## Bug Fix: morph slider stops having any effect after toggling A <-> B several times

### Problem
After switching between slot A and slot B a few times, moving the morph slider no longer changed the
sound. Switching slots again usually (but not always) restored it.

### Root cause
`onMorphSliderChanged()` keeps an exclusion map `lastMorphIntendedValues` (per-parameter) so a morph can
coexist with external automation: any parameter whose live value differs from the value the morph last
applied is treated as externally driven and is skipped by the morph. A skipped parameter's baseline entry
is never refreshed, so once excluded it stays excluded.

This map was only cleared in `resetMorph()` and on the first auto-capture. The slot-switch click handler
reset `morphSource` / `morphTarget` / `morphUndoState` but **not** `lastMorphIntendedValues`. So:
1. After a slider move, `lastMorphIntendedValues` holds the interpolated (mid-morph) values.
2. Toggling to the other slot calls `loadSlot()` which overwrites every parameter with the slot's saved
   values, while the stale `lastMorphIntendedValues` persists.
3. The next slider move sees live (slot) values != stale intended values and excludes those parameters
   from the morph. Each toggle accumulates more exclusions; after a few A<->B switches almost every
   parameter is excluded, so the morph applies nothing — the slider is dead. Switching slots re-seeds the
   baseline, which is why it "usually" recovered.

### Fix
- Clear `lastMorphIntendedValues` in the slot-switch click handler, alongside the other morph-state resets,
  so the new crossfade starts with a clean exclusion baseline.
- Clear `lastMorphIntendedValues` in the "A -> B" morph context-menu action for the same reason (it also
  establishes a fresh A->B crossfade). The incremental "set source" / "set target A/B" menu actions are
  deliberately left untouched, as clearing there would wipe a legitimate, in-progress exclusion baseline.

### Files changed
- `Source/PluginEditor.cpp`
  - Slot-switch click handler: added `lastMorphIntendedValues.clear()` before `loadSlot()`.
  - `showMorphContextMenu()` "A -> B" action: added `lastMorphIntendedValues.clear()`.

### Verification
- VST3 target builds cleanly (exit 0).
- Repro: fill A and B with different states, drag the slider, then toggle A<->B several times and drag the
  slider each time — it now keeps full effect after every toggle (no accumulated exclusions).
- The DAW-automation coexistence feature is unaffected: clearing only happens on slot switch / fresh A->B,
  not during a continuous morph drag.


