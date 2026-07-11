# Changelog - June 10, 2026

## New UI features (improvements requested by Jérôme)

### Display of the sung note and the offset in cents

- **PitchVisualizer header**: displays in large the name of the currently
  sung note (e.g. "F3") + a smaller text indicating the target note
  (e.g. "-> F3") if different.
- **Cents offset**: a large text displays "+/- 50 c" to the right of the
  note name. The color changes according to severity:
  - green (|c| < 5): on the note
  - yellow (|c| < 25): close to the note
  - red (|c| >= 25): clearly off
- **Vertical tuning meter** (Antares / Studio One style): a horizontal
  needle moves according to the cents offset, with a green center bar
  (0 cents) and ticks at +/-50 and +/-100 cents.

### Display of the scale notes

- **Semi-transparent yellow lines** drawn in the PitchVisualizer for all
  the scale notes (over 4 octaves, C2 -> C6).
- These notes are updated in real time according to the chosen Key + Scale.

### Custom scale

- **New `Scale::Custom` mode** (index 5) added to `atdsp::Scale`.
- **12 AudioParameterBool booleans** (`custom0` to `custom11`): each note
  can be individually checked/unchecked (C, C#, D, ..., B).
- **12 ToggleButton booleans** in the GUI, arranged in 1 horizontal row
  below the knobs. Visible only if Scale = "Custom".
- **`ScaleQuantizer::setCustomIntervals()`** pushes the list of active
  notes to the quantizer.
- **Default**: C major (C, D, E, F, G, A, B).

### Vertical piano keyboard (PianoKeyboard)

- **New `ui::PianoKeyboard` component**: draws a vertical piano keyboard
  (low notes at the bottom, high notes at the top) with white and black
  keys correctly aligned.
- **Placed to the left of the PitchCurveEditor** (40 px wide): helps
  visually identify the notes of the pitch curve.
- **Scale notes highlighted in yellow**: see at a glance which notes are
  "allowed" by the current scale.
- **Octave labels (C2, C3, C4, ...)** on the left of the keyboard.

## Architecture and implementation

### New DSP utilities file

- **`Source/dsp/NoteUtils.h`**: inline conversion functions
  Hz <-> MIDI note, cent (hundredth of a semitone), and `NoteInfo` struct
  to group all information displayed in the UI.

### Modifications to existing modules

- **`ScaleQuantizer.h/.cpp`**:
  - New enum value `Scale::Custom` (= 5).
  - New `setCustomIntervals(const juce::Array<int>&)` method.
  - `rebuildIntervals()` distinguishes the Custom case (uses the custom
    list directly, without key shift) from the other modes.
- **`PitchCurve.h/.cpp`**:
  - New `snapToScaleCustom()` method for snapping with a custom scale.
  - The interactive snap of `PitchCurveEditor` automatically chooses
    between `snapToScale` (preset modes) and `snapToScaleCustom` (Custom).
- **`PluginProcessor.h/.cpp`**:
  - New `custom0..custom11` parameter (12 AudioParameterBool).
  - `scale` parameter range changed to 0..5.
  - New `getCurrentCentsOffset()` getter and atomic field
    `lastCentsOffset` updated every block.
  - `syncParameters()` pushes the custom notes to the quantizer
    if `scaleIdx == 5`.

### UI modifications

- **`PitchVisualizer.h/.cpp`**:
  - New top banner (60 px) with note + cents.
  - New tuning meter area on the right (60 px wide).
  - Scale lines drawn in the background.
  - New `setNoteInfo()` and `setScaleIntervals()` methods.
- **`PitchCurveEditor.h/.cpp`**:
  - PianoKeyboard integrated as a child, redrawn on the left.
  - `timeToX` / `xToTime` account for the piano width.
  - Custom mode propagated to `snapToScaleCustom`.
  - `setCustomIntervals()` to receive the custom notes.
- **`PianoKeyboard.h/.cpp`** (new):
  - White keys (C, D, E, F, G, A, B) drawn full width.
  - Black keys (C#, D#, F#, G#, A#) shorter, on top.
  - Different colors for in-scale notes (yellow).
  - Octave labels (C2, C3, ...) on the C keys.
- **`PluginEditor.h/.cpp`**:
  - 12 new `ToggleButton customButtons[0..11]` + their attachments.
  - `updateCustomVisibility()`: shows/hides the 12 buttons depending on
    whether Scale = Custom or not.
  - `refreshVisualizer()` enriched: sends NoteInfo + scaleIntervals to the
    PitchVisualizer and to the PitchCurveEditor (which propagates to piano).
  - Layout adjusted: bottom bar increased from 160 to 220 px to make room
    for the 12 custom boolean row.

## CMakeLists.txt

- Added `Source/dsp/NoteUtils.h`, `Source/ui/PianoKeyboard.h`,
  `Source/ui/PianoKeyboard.cpp` to the sources of the `AutotuneClone`
  target.

## Build

- Release x64 build succeeded:
  - `Autotune Clone.vst3` produced.
  - `Autotune Clone.exe` (Standalone) produced.
- 1 MSVC C4172 warning (PitchCurve.h:68-69): "returning address of local
  or temporary variable" on `getPoint(int)`; pre-existing (from the
  previous version), not fixed because harmless (references are used
  immediately in the same scope).

## Bug fix: JUCE assertion on first PianoKeyboard paint

### Symptom
On the first call to `PianoKeyboard::paint()`, Visual Studio (Debug mode)
triggered a breakpoint on the `jassertquiet` assertion in
`juce::`anonymous namespace'::coordsToRectangle<float>` (line 91 of
`juce_GraphicsContext.cpp`), with the call stack:

```
coordsToRectangle<float>             [juce_GraphicsContext.cpp:91]
juce::Graphics::fillRect (float)     [juce_GraphicsContext.cpp:560]
ui::PianoKeyboard::paint             [PianoKeyboard.cpp:109]
```

### Cause
In `PianoKeyboard::paint`, for each white key we compute:
```cpp
const float y    = midiToY (midi);
const float keyH = midiToY (midi + 1) - y;
```
But for `midi = highestMidi` (=96, C7):
- `midiToY(96) = H - (96-36)/60 * H = 0`
- `midiToY(97) = H - (97-36)/60 * H = -0.0167 * H` (NEGATIVE)
- So `keyH = -0.0167 * H < 0`, which violates the `(int) h >= 0` assertion.

The same problem affected the black keys of the last semitone.

### Fix
Clamp the height to a minimum of 1 pixel for both key types (white and
black), to avoid any `fillRect` with a null or negative height.

See `debug-pianokeyboard-negative-height.md` for the full session.

### Verification
- Release x64 build succeeded after the fix.
- Standalone launch: no assertion triggered.
- The `PianoKeyboard` component displays correctly to the left of the
  `PitchCurveEditor`.

## Bug fixes and clarifications following feedback

### Layout reorganization (unreadable UI)
- **Bypass button**: moved to the top right with "Bypass" label and an
  explanatory tooltip. The "v0.1.0 - Phase 1" version label in the banner
  is no longer overlapped by the button.
- **Custom scale buttons**: moved to their OWN row at the bottom (28 px
  high, after the knobs/ComboBox row), instead of being overlaid on the
  knobs.
- **3 distinct rows** in the bottom bar:
  1. Mode / Snap (28 px)
  2. Knobs (Speed, Amount) + ComboBox (Key, Scale) (90 px)
  3. 12 Custom scale booleans (28 px, visible only in Custom)
- **Spacing**: 10 px padding between rows, no more overlap.

### More readable piano keyboard
- **Width increased from 40 to 60 pixels** in `PitchCurveEditor::resized()`.
- **Black key width** increased from 60% to 65% of the total width.
- **Label size** increased from 9pt to 11pt bold for C2, C3, etc.

### Clarification of the Bypass role
- **Tooltip added** on the Bypass button: explains that enabling Bypass
  passes the audio through unprocessed (dry pass-through), and that the
  visualizer keeps working in both cases.
- Note in the documentation: the "Mute audio input" in the standalone's
  Audio/MIDI Settings is a distinct toggle that cuts the standalone's
  hardware monitoring (independent of the plugin's bypass).

### Audio bug fix: systematic PSOLA call

**Symptom**: with bypass OFF, the audio was "wrong" (silence or
distortion) when the user quickly moved from in-scale to out-of-scale.
With "Mute audio input" checked, nothing was audible.

**Identified cause**: in `PluginProcessor::processBlock`, the call to
`pitchShifter->process` was conditioned by `|ratio - 1.0f| > 1e-3f`.
When the user sang in scale (ratio close to 1), `pitchShifter->process`
was NOT called, so:
- The ring buffer was not fed with the new samples.
- `totalSamplesWritten` and `nextSynthMarkSample` stayed frozen.
- No pitch mark was detected.

When the user then moved out-of-scale (ratio != 1), `pitchShifter->process`
was called with a stale ring buffer containing samples from N blocks ago.
The synthesis could produce silence or artefacts.

**Fix**: we ALWAYS call `pitchShifter->process`, without any condition on
the ratio. It is `PitchShifter::process` itself that decides whether to do
passthrough (ratio close to 1 or f0 <= 0) or OLA synthesis. The ring buffer
is therefore continuously up to date.

**File**: `Source/PluginProcessor.cpp` (around line 210).

**Debug session**: `debug-psola-audio-incorrect.md` (marked [OPEN], to be
confirmed after user testing).

### Note on PitchCurveEditor point drag
The drag only works when the mode is "Graphic" (not "Auto").
The gray overlay "Mode Auto : switch to Graphic mode to edit" is shown in
the editor when Auto mode is active. This is desired behavior, not a
regression: you cannot edit the curve manually in Auto mode (the plugin
follows the scale automatically).

## Round 2 of fixes following feedback (June 10, 2026 PM)

### Fix A (CONFIRMED): visualizer / curve editor overlap
**Symptom**: the visualizer area was completely empty (the header with
note/cents, the tuning meter, the scale lines were not visible). Only the
curve editor (with its points) was displayed, in an area that should have
been shared.

**Cause**: in `PluginEditor::resized`, I had written:
```cpp
auto vizArea   = centerArea.reduced (pad).removeFromTop (...);
auto curveArea = centerArea.reduced (pad);  // BUG: full area, not remaining
```
The `removeFromTop` was called on a temporary Rectangle (the result of
`centerArea.reduced(pad)`), so the original Rectangle was not modified.
`curveArea` was the COMPLETE area and fully covered the visualizer.

**Fix**: use an intermediate variable so that `removeFromTop` actually
modifies the rectangle:
```cpp
auto reducedCenter = centerArea.reduced (pad);
auto vizArea       = reducedCenter.removeFromTop (...);
auto curveArea     = reducedCenter;
```

**File**: `Source/PluginEditor.cpp` (around line 240).

### Fix B (strong suspicion): drag impossible in Graphic mode
**Symptom**: even in Graphic mode, dragging the PitchCurveEditor points
did not work.

**Probable cause**: the `PianoKeyboard` (child of `PitchCurveEditor`, 60 px
on the left) had `setInterceptsMouseClicks(true, true)` by default, which
could interfere with mouse event dispatch to the parent (curve editor).

**Fix**: `pianoKeyboard.setInterceptsMouseClicks(false, false)` in the
`PitchCurveEditor` constructor. The piano is a pure display, it does not
need to intercept events.

**File**: `Source/ui/PitchCurveEditor.cpp` (constructor).

### Fix C (preventive): audio glitches "fast dropouts"
**Symptom**: the user reports very fast glitches when singing out-of-tune
(with bypass OFF).

**Probable cause**: the ratio smoothing coefficient in the PSOLA was much
too slow:
- `smoothingCoeff = 0.995` -> tau ~4.6 seconds (with 1024-sample blocks at
  44.1 kHz).
- `currentF0` smoothing 0.95 -> tau ~0.45 seconds.

The PSOLA took seconds to converge after a target ratio change, producing
audible artefacts (dropouts).

**Fix**:
- `smoothingCoeff` 0.995 -> 0.9 (tau ~0.23 s, fast response)
- `currentF0` smoothing 0.95 -> 0.85 (tau ~0.13 s)

**Files**: `Source/dsp/PitchShifter.h`, `Source/dsp/PitchShifter.cpp`.

### Verification
- Release x64 build succeeded.
- Standalone launches without problem.
- To be confirmed by user testing.

### Debug session
`debug-visualizer-overlap-and-drag.md` (status: FIXED).

## Round 3 - Audio performance fix: crash and glitches at small buffer

### Symptom
- Audible audio glitches even at 2048 samples.
- Many more glitches at 144 samples (3.3ms per block).
- Intermittent crash in `AudioProcessorPlayer::audioDeviceIOCallbackWithContext`
  while manipulating the controls (Scale, Speed, etc.).

### Identified cause
In `PitchShifter::process`, the output buffer was created LOCALLY at each
call via `juce::AudioBuffer<float> output; output.setSize(...);`. This is a
**heap allocation** in the audio thread.

- 144 samples @ 44.1 kHz -> 306 blocks/sec -> 306 allocations/sec
- 2048 samples @ 44.1 kHz -> 22 blocks/sec -> 22 allocations/sec
- The pressure on the heap (Windows allocator) causes real-time deadline
  misses -> glitches.
- When the audio thread misses too many deadlines, Windows may kill it ->
  crash in the callback.

### Fix
- `outputBuffer` is now a member of `PitchShifter` (header
  [PitchShifter.h](file:///c:/Users/User/Documents/trae_projects/VST3/Source/dsp/PitchShifter.h#L107-L113)).
- Pre-allocated ONCE in `prepare()` to `juce::jmax(bs*2, 2048)` samples,
  which covers all realistic cases.
- In `process()`, we only do `std::memset` (zero-copy memory, no allocation).
- Safety fallback: if a block larger than the allocated capacity arrives
  (unlikely with 2x preallocation), we let the input pass through unchanged
  (early return) instead of risking a crash.

### Verification
- Release x64 build succeeded.
- Standalone launches.
- To be confirmed by user testing at 144 and 2048 samples.

### Debug session
`debug-audio-callback-crash-and-glitches.md` (status: FIXED).
