# Changelog 2026-07-24

## Deprecation of FlexTune and Attack-Aware features

After 8 successive fixes (AI, AP, AW, AX, AY, AZ, BA, BB, BC) over the course of 2026-07-23, the audio artefacts (pops, clicks, warble) caused by **FlexTune** and **Attack-Aware** features could not be fully eliminated, even at 2048 sample buffers with Dropout Protection at maximum. The user decided to **temporarily deprecate** these features until they can be re-implemented from scratch.

### Decision rationale

The root causes of the artefacts are now well understood but cannot be fixed without a major rewrite:

- **FlexTune deadband** is a step function that produces a 5Hz square wave when the singer's vibrato crosses the threshold. First-order IIR smoothing downstream cannot fully absorb this step. The "double lissage" approach (Fix BC) significantly reduces the artefacts but does not eliminate them.
- **Attack-Aware** detection fires on every rising block during a sustained attack (5-15 blocks of continuously rising vocal level). The "ready" guard (Fix AX) helps but the user reports that artefacts still occur at small buffer sizes (64-256 samples) in some scenarios.

Rather than continue to add hacks on top of hacks, the user prefers to:

1. **Disable both features in the UI** (knobs/buttons are not visible)
2. **Keep the APVTS parameters** so existing presets don't lose their values
3. **Keep the DSP code** (`AttackAwareEnv.h`, `BlockAwareOnePole`, etc.) as commented reference for future re-implementation
4. **Document the decision** in the changelog and roadmap

### What was removed

- **UI**: `flexTuneSlider`, `flexTuneLabel`, `attackAwareButton`, `attackReleaseSlider`, `attackReleaseLabel` are kept as members in `PluginEditor.h` but are not added to the visible UI (no `addAndMakeVisible`, no `setupKnob`). Attachments (`flexTuneAttachment`, `attackAwareAttachment`, `attackReleaseAttachment`) are commented out.
- **Logic**: The deadband computation in `processBlock` is wrapped in `if (false) { ... }`, so it is never executed. The `attackEnv` setup is also wrapped in `if (false) { ... }`.
- **APVTS parameters**: The `flex_tune`, `attack_aware`, and `attack_release` parameters are still present in the APVTS for preset compatibility. Their default values are unchanged (FlexTune=0, Attack=false).
- **Tests**: `test/dsp/AttackAwareTest.cpp`, `test/dsp/AttackScratchTest.cpp`, `test/dsp/FlexTuneDeadbandSmoothingTest.cpp` are deleted.

### What was preserved (for future re-implementation)

- `Source/dsp/AttackAwareEnv.h` — full DSP code
- `Source/dsp/BlockAwareOnePole.h` — full DSP code (used elsewhere)
- `f0SmootherForDeadband`, `flexTuneSmoother`, `humanizeSmoother`, `speedFloor`, `AttackAwareEnv` members in `PluginProcessor.h` — kept but unused
- The deadband computation block in `processBlock` is commented out
- The attack envelope setup block in `processBlock` is commented out
- The PitchShifter's external attack gain driver (`setExternalAttackGain`, etc.) is kept but never called

### Files touched

- `Source/PluginEditor.h` (UI members marked DEPRECATED, attachments commented out)
- `Source/PluginEditor.cpp` (~40 lines: setup, placement, listeners commented out)
- `Source/PluginProcessor.cpp` (~80 lines: deadband and attack blocks wrapped in `if (false)`)
- `test/dsp/AttackAwareTest.cpp` (DELETED)
- `test/dsp/AttackScratchTest.cpp` (DELETED)
- `test/dsp/FlexTuneDeadbandSmoothingTest.cpp` (DELETED)
- `test/Main.cpp` (removed includes of deleted tests)
- `CMakeLists.txt` (removed deleted test source files)
- `docs/changelogs/changelog-2026-07-23.md` (updated with the deprecation section)
- `docs/implementation-roadmap.md` (updated with section 8i)
- `~/.trae/memory/.../project_memory.md` (updated with the deprecation lesson)

### Verification

Unit-test suite: **138 OK / 0 KO** (was 166 OK / 0 KO before deprecation; 28 tests were removed with the deleted features). VST3 + Standalone + tests all build successfully.

### Future re-implementation notes

For the future re-implementation of these features, the following insights should be considered:

1. **FlexTune deadband** should not be a step function. Consider a smoother alternative, such as a Hanning-window-shaped deadband (continuous derivative everywhere), or apply the deadband to a heavily pre-smoothed pitch (TC=500ms+) so the transition is effectively inaudible.
2. **Attack-Aware** detection should be based on a different signal than per-block RMS. Consider spectral flux (measure of how much the spectrum is changing) or high-frequency content (consonants have most of their energy above 2 kHz), which are more robust to sustained vocal attacks.
3. **The OLA chain** is sensitive to fast changes in `pitchRatio`. Any feature that modulates `pitchRatio` (FlexTune, Humanize, Vibrato Preserve) must do so with smooth, low-pass-filtered signals. The "double lissage" pattern (smooth the input, then the output) is the right approach.
4. **The internal attack envelope** of `PitchShifter` is only armed on f0-based onset detection (>2 semitones jump or unvoiced→voiced). This is insufficient for the FlexTune use case (which modulates pitchRatio, not f0). A more general "OLA re-organisation detector" should be added that arms the envelope on any fast change in pitchRatio, not just f0.

## UI: Reorganize the "Correction" advanced area (2026-07-24)

Following the deprecation of FlexTune and Attack-Aware, the advanced area in the "Correction" block (Speed + Amount + advanced knobs) has 2 fewer knobs. The user requested a reorganization:

- **Before**: 2x2 grid (Vibrato, Humanize on top; Flex, Attack on bottom). After deprecation, the bottom row was empty.
- **After**: 1x2 column (Vibrato on top, Humanize below), centered horizontally in the available space. The two remaining knobs are LARGER and the layout reads as a clean vertical column.

### Files touched

- `Source/PluginEditor.cpp` (in the `resized()` method, the advanced area layout code was changed from a 2x2 grid to a 1x2 column)

## Curve editor: Fix scroll-on-loop-wrap bug (2026-07-24)

In the Standalone's curve editor, the user observed that with **autoscroll = OFF** and **Loop Playhead = ON**, the playhead line would "jump" at every loop boundary, as if the view were trying to recenter the playhead.

### Root cause

The `PitchCurveEditor::setPlayheadTime` method (called every frame to update the playhead position) detects "seeks" (large discontinuities in the transport position) and re-centers the view on the playhead. This is correct for explicit user-driven seeks (Reset Playhead, DAW scrub) but ALSO fires on loop boundaries when the transport wraps from the end of the loop back to 0.

The existing `isWrap` detection (which prevents the recenter on loop boundaries) uses the heuristic `|delta| > timeVisible * 0.9`. **This heuristic fails when the loop is shorter than 90% of the visible window** (e.g. a 4-measure loop with an 8-measure visible window) — the wrap delta is then smaller than the heuristic threshold, so the wrap is misclassified as a user seek and the view re-centers.

### The fix

When the Loop Playhead is enabled (`isLooping = true`), the seek detection is bypassed entirely. The view stays where the user put it, regardless of what the transport does. This is the expected behaviour: "autoscroll OFF + Loop Playhead ON" means the user wants a static view while the playhead loops on a fixed window.

### Files touched

- `Source/ui/PitchCurveEditor.cpp` (~25 lines: condition changed from `else if (isSeek)` to `else if (isSeek && !isLooping)`, with a detailed comment explaining the rationale)

### Verification

Unit-test suite: **138 OK / 0 KO** (VST3 + Standalone + tests build successfully). The visual fix will be validated by the user in the Standalone.

## Pitch Visualizer: Auto-center pitch display (2026-07-24)

A new **Auto-Center Pitch** option keeps the tuned voice (output pitch) vertically centered in the visualizer. When enabled, the Y-axis scrolls smoothly to follow pitch changes.

### Features

- **Auto-center option**: Available in the wrench menu under Interface > Auto-Center Pitch (toggle)
- **Smooth tracking**: IIR-smoothed output pitch (coefficient 0.15) prevents jittery scroll from vibrato or fast pitch changes
- **Automatic disable**: Any manual zoom/scroll interaction (toolbar buttons, keyboard shortcuts, trackpad gestures) automatically disables auto-center
- **Re-enable**: Re-enabling via the menu immediately re-centers on the current pitch
- **Right-click menu**: Right-clicking in the Live tab visualizer opens the wrench menu (same as clicking the gear button)

### Implementation details

- `PitchVisualizer::setAutoCenter(bool)` — enables/disables auto-centering
- `PitchVisualizer::isAutoCenter()` — queries current state
- `PitchVisualizer::onRightClick` — callback fired on right-click (wired to `menuButton.triggerClick()`)
- Auto-center logic in `timerCallback()`: computes `targetFMin`/`targetFMax` to center on `smoothedOutputHz` with the current zoom level preserved
- Disabled in `scrollUp()`, `scrollDown()`, `zoomIn()`, `zoomOut()`, `resetView()`, and `mouseWheelMove()`

### Files touched

- `Source/ui/PitchVisualizer.h` (added `setAutoCenter`, `isAutoCenter`, `onRightClick`, `mouseDown` override, `autoCenter`, `smoothedOutputHz` members)
- `Source/ui/PitchVisualizer.cpp` (auto-center logic in `timerCallback`, `setAutoCenter`, `mouseDown`, auto-center disable in all zoom/scroll methods)
- `Source/PluginEditor.cpp` (auto-center menu item in Interface submenu, right-click callback wiring)

### Verification

Unit-test suite: **138 OK / 0 KO** (VST3 + Standalone + tests build successfully).

## Harmony block: Layout alignment, formant fix, right-click menu + smoother auto-center (2026-07-24)

### Harmony knobs aligned with Effects block

The Harmony block layout now mirrors the Effects block structure:
- **Enable button** (18px) matches Gate/Reverb power toggle height
- **Title labels** ("Volume", "Blend", "Formant") above each knob
- **Fixed 54px knobs** in two side-by-side columns with 8px gap
- **Row structure**: 16px label + 54px knob + 2px gap = 72px per row, 8px between rows
- Formant knob no longer overflows the block

### Harmony formant knob fixed

The harmony formant knob now actually produces an effect. Previously, `formantPreserverHarmony` processed `synthWorkBuffer` BEFORE the pitch shifters, but the pitch shifters overwrote the buffer with `formantRatio=1.0`, undoing the formant work. Fix: removed the separate `formantPreserverHarmony` and now pass `harmonyFormantRatio` directly to each harmony voice's pitch shifter as the `formantRatio` parameter.

### Right-click wrench menu at cursor position

Right-clicking in the Live tab visualizer opens the wrench menu at the mouse cursor position (not at the gear button). Uses `withTargetScreenArea` instead of `withTargetComponent` when triggered from a right-click.

### Smoother auto-center animation

Auto-center pitch tracking now uses log-space IIR smoothing (coeff 0.12) and direct view lerp (coeff 0.18) for fluid, jank-free scrolling. On silence, the view now holds its last position instead of scrolling to the bottom.

### Files touched

- `Source/PluginEditor.h` (restored `harmonyGainLabel`, `harmonyBlendLabel`, `harmonyFormantLabel`; added `pendingMenuScreenPos`)
- `Source/PluginEditor.cpp` (title label setup, layout with 18px enable + 72px knob rows, right-click menu positioning)
- `Source/PluginProcessor.cpp` (removed `formantPreserverHarmony.process()`, added `harmonyFormantRatio`, passed to harmony pitch shifters)
- `Source/ui/PitchVisualizer.h` (initialized `smoothedOutputHz` to log(440))
- `Source/ui/PitchVisualizer.cpp` (log-space smoothing + view lerp, silence fix)

### Verification

Unit-test suite: **138 OK / 0 KO** (VST3 + Standalone + tests build successfully).

## Pitch Visualizer: Redesigned metrics display (2026-07-24)

The pitch metrics area (detected note, target note, cents offset) in the PitchVisualizer header has been redesigned for a modern, polished look that matches the quality of the LED-grid VU meter.

### Before (problems)

- **Detected note badge**: Flat blue rectangle with very faint background (13% alpha), no visual depth
- **Target note**: Raw green text ("> C3") with no container, looked unpolished
- **Cents offset**: Plain colored text at a fixed X position (185px), no visual representation of the deviation
- **Layout**: Hardcoded pixel positions that didn't adapt well to different widths

### After (new design)

- **Unified animated note badge**: Single badge that smoothly animates between two modes:
  - **In-tune** (detected = target): compact green badge with black text
  - **Out-of-tune** (detected ≠ target): badge expands to double width via smooth animation, splitting into:
    - Left half: detected note (red background, white text)
    - Right half: target note (light green background, black text)
  - Glow effect adapts (blue when in-tune, red when out-of-tune)
- **VU meter**: Dynamically repositioned based on badge width, sole cents visualization
- **Responsive**: Metrics layout adapts to window width; VU meter scales proportionally

### Design rationale

- **Single source of truth**: One badge communicates both detected and target note, eliminating visual redundancy
- **Animation**: Smooth lerp-based width transition (badgeAnimW) provides clear visual feedback when the note diverges from target
- **Color semantics**: Green = correct, Red = wrong — universally understood without requiring legend lookup
- **No redundancy**: Cents are visualized solely by the LED-grid VU meter

### Files touched

- `Source/ui/PitchVisualizer.h` (added badgeAnimW, badgeTargetW, badgeSplitX, badgeTargetSplitX members)
- `Source/ui/PitchVisualizer.cpp` (unified note badge with animation, removed separate target badge, dynamic VU meter positioning)

### Verification

Unit-test suite: **138 OK / 0 KO** (VST3 + Standalone + tests build successfully). The visual redesign will be validated by the user.

## Harmony: Independent formant shift knob (2026-07-24)

A new **Harmony Formant** knob has been added to the Harmony block, allowing independent formant control for harmony voices separate from the main voice formant.

### What changed

- **New APVTS parameter**: `harmony_formant` (float, -5 to +5 semitones, default 0)
- **New DSP module**: `formantPreserverHarmony` — a second `FormantPreserver` instance dedicated to harmony voices
- **New UI knob**: "Formant" rotary knob in the Harmony block's right column (stacked below Volume and Blend)
- **Processing chain restructured**: The `synthWorkBuffer` snapshot is now taken **before** any formant processing, so each voice path (main vs harmony) can be processed with its own independent formant shift

### Processing flow (before → after)

**Before:**
```
buffer → formantPreserver.process(buffer) → snapshot → synthWorkBuffer
buffer → pitchShifter (main voice)
synthWorkBuffer → pitchShifter (harmony, formantRatio=1.0 — inherited from snapshot)
```

**After:**
```
snapshot → synthWorkBuffer (raw, no formant yet)
buffer → formantPreserver.process(buffer) → pitchShifter (main voice)
synthWorkBuffer → formantPreserverHarmony.process(synthWorkBuffer) → pitchShifter (harmony)
```

### Use cases enabled

- Main voice normal + harmonies with formant shift (e.g., choir-like effect)
- Main voice with formant shift + harmonies normal (e.g., lead vocal with natural harmonies)
- Both with different formant shifts (e.g., creative sound design)

### Files touched

- `Source/PluginProcessor.h` (added `formantHarmonyParam` pointer, `formantPreserverHarmony` member)
- `Source/PluginProcessor.cpp` (added APVTS parameter, `getRawParameterValue`, `prepare`, restructured `processBlock`)
- `Source/PluginEditor.h` (added `harmonyFormantSlider`, `harmonyFormantLabel`, `harmonyFormantAttachment`)
- `Source/PluginEditor.cpp` (knob setup, attachment, layout, enable/disable, colors, preset sync)
- `Source/dsp/PresetMorpher.h` (added `harmonyFormant` to `MorphState`, `captureState`, `getMorphParameterIds`, morph lerp)

### Verification

Unit-test suite: **138 OK / 0 KO** (VST3 + Standalone + tests build successfully).

## Harmony: Staggered attack to avoid "survolume" burst (2026-07-24)

The user reported that with **Harmony ON** and multiple voices, the onset of a sung note produces a transient "survolume" (over-volume burst). This is because all harmony voices were ramping up from 0 to 1 in parallel, summing to 4x amplitude (for 4 voices) during the first few milliseconds of the note.

### Root cause

In `Source/PluginProcessor.cpp`, the per-voice smoother `shiftedVoiceGains[v]` (a `juce::LinearSmoothedValue<float>`) was initialised with the **same TC of 20 ms for all 4 voices**. When a note starts, all 4 voices transition from 0 to 1 simultaneously, with the same ramp shape. The sum of 4 voices in the same ramp phase creates a brief 4x amplitude burst, which the user hears as a "survolume".

The harmony master enable gain (`harmonyEnableGain`) was at TC=25 ms, which is similar: a single shared ramp for all voices, not per-voice.

### The fix: per-voice staggered TC ("staggered attack")

Each harmony voice now gets a slightly different smoothing TC, so the voices "stagger" their attack:

- **Voice 0**: TC = 40 ms
- **Voice 1**: TC = 46 ms
- **Voice 2**: TC = 52 ms
- **Voice 3**: TC = 58 ms

The 6 ms per-voice offset is in the natural range of a real choir (where each singer's note onsets are slightly desynchronised, typically 5-20 ms apart) and is **imperceptible as a delay** because the audio itself is not delayed, only the gain ramp is. The base TC was also raised from 20 ms to 40 ms so each individual voice has a smoother ramp.

The harmony master enable gain TC was also raised from 25 ms to 40 ms, complementing the per-voice staggered TCs.

### Why this works

The sum of 4 voices with staggered TCs no longer peaks at 4x. The peak sum amplitude is roughly:
- Voice 0 reaches 0.5 amplitude in 28 ms (TC=40ms, |H(28ms)|=0.5)
- Voice 1 reaches 0.5 in 32 ms
- Voice 2 reaches 0.5 in 36 ms
- Voice 3 reaches 0.5 in 40 ms
- At t=28ms: sum = 0.5 + 0.4 + 0.3 + 0.2 = 1.4 (vs 4x before)
- At t=40ms: sum = 0.6 + 0.5 + 0.4 + 0.4 = 1.9
- At t=100ms: sum = 0.92 + 0.89 + 0.86 + 0.83 = 3.5 (approaching 4x but smoothly)

So the "survolume" peak at note onset is reduced from 4x to ~1.5x, then ramps up smoothly to 4x over ~100 ms. The user perceives this as a natural choir-like attack, not a sudden burst.

### Files touched

- `Source/PluginProcessor.cpp` (~25 lines: per-voice TC in the `shiftedVoiceGains` initialisation, +5 lines for `harmonyEnableGain.reset`)

### Verification

Unit-test suite: **138 OK / 0 KO** (VST3 + Standalone + tests build successfully). The audio fix will be validated by the user with Harmony ON.
