# OpenVoxTuner — Preset Morphing / Crossfade: Technical Strategy

> Date: 2026-07-08
> Status: Technical design document

---

## 1. Executive Summary

The Preset Morphing feature allows users to smoothly transition between two
plugin states (a "source" and a "target") using a dedicated morph slider.
The transition interpolates all audio-relevant parameters in real-time while
maintaining glitch-free audio output.

---

## 2. Technical Architecture

### 2.1 Parameter Classification

All 30+ plugin parameters are classified into three interpolation categories:

| Category | Parameters | Interpolation |
|----------|-----------|---------------|
| **Continuous** | `speed`, `amount`, `formant`, `harmony_gain`, `harmony_blend`, `harmony_tone_color`, `reverb_mix`, `flex_tune`, `humanize` | Linear interpolation (lerp) |
| **Discrete (ordered)** | `key` (0-11), `scale` (0-13), `harmony_type` (0-21), `harmony_tone` (0-5), `harmony_shifted_voices` (1-4), `latency_mode` (0-2), `editor_measures` | Step transition at morph threshold (50%) |
| **Boolean** | `formant_enable`, `bypass`, `harmony_enable`, `harmony_use_voice`, `midi_out_enable`, `reverb_enable`, `correction_mode`, `auto_scroll` | Step transition at morph threshold (50%) |
| **UI-only** | `ui_theme`, `ui_language`, `mode` (Live/Curve) | Not interpolated (kept from source) |
| **Custom scale** | `custom0`-`custom11` | Transition at morph threshold (50%) |

### 2.2 PitchCurve Interpolation

The PitchCurve is the most complex element to morph. Two approaches are
considered:

**Approach A — Time-aligned interpolation (chosen)**:
- Normalize both curves to a common time range [0, 1]
- Sample both curves at N fixed time points (N = 128)
- Linearly interpolate each sampled pitch value
- Reconstruct the target PitchCurve from the interpolated samples

**Approach B — Point-by-point matching**:
- Match source/target points by time proximity
- Interpolate matched pairs, interpolate unpaired points toward the opposite
  curve's nearest segment
- More accurate for sparse curves but complex edge-case handling

**Decision**: Approach A is simpler, deterministic, and produces smooth
results. The 128-sample resolution is sufficient for visual and audio quality.

### 2.3 Audio-Safe Crossfade

The morph operation must never produce audio glitches. The implementation uses
a **parameter smoothing ramp** approach:

1. The morph slider value (0.0 = source, 1.0 = target) drives all parameter
   interpolation in the editor
2. Parameter values are written to the `AudioProcessorValueTreeState` via
   `setValueNotifyingHost()` (no notification to host, prevents parameter
   automation conflicts)
3. The DSP pipeline reads the interpolated parameter values naturally through
   the existing `getRawParameterValue()` calls — no DSP changes needed
4. For discrete parameters, the transition happens at the 50% threshold to
   minimize audible jumps

**Glitch prevention**: The `RetargetEnvelope` in the pitch correction
pipeline already smooths speed parameter changes. Harmony voice gains are
already smoothed via `LinearSmoothedValue<float>` (10ms). No additional
smoothing is required.

### 2.4 State Capture and Restore

The morph system reuses the existing A/B state mechanism:

```
┌─────────────────────────────────────────────────┐
│  MorphSlider (0.0 ──────────── 1.0)            │
│       │                    │                    │
│       ▼                    ▼                    │
│  ┌─────────┐         ┌─────────┐               │
│  │ Source   │         │ Target  │               │
│  │ State    │         │ State   │               │
│  │ (XML)    │         │ (XML)   │               │
│  └────┬─────┘         └────┬─────┘              │
│       │                    │                    │
│       └──────┬─────────────┘                    │
│              ▼                                  │
│  ┌──────────────────────────┐                   │
│  │  Interpolation Engine    │                   │
│  │  (lerp per parameter)    │                   │
│  └────────────┬─────────────┘                   │
│               ▼                                 │
│  ┌──────────────────────────┐                   │
│  │  Active Plugin State     │                   │
│  │  (parameters + curve)    │                   │
│  └──────────────────────────┘                   │
└─────────────────────────────────────────────────┘
```

**Source state**: captured when the user starts moving the morph slider (or
presses a "Capture" button).

**Target state**: loaded from a preset (factory or custom) or from the
other A/B slot.

---

## 3. User Interface Design

### 3.1 Morph Slider Component

A new horizontal slider added to the plugin header, between the A/B button
and the presets button:

```
[A/B]  ──[═════════●════════]──  [Presets]
       Source ▲          ▲ Target
             0%        100%
```

**UI Element**: `juce::Slider` (linear horizontal)
- Range: 0.0 to 1.0, step 0.01
- Slider label: "Morph" (small, above the slider)
- Source label: preset name or "Current" (left of slider)
- Target label: preset name or "Target" (right of slider)
- Color: accent color (#1A9AF0) for the filled portion

**Width**: ~180px (fits in the header strip alongside existing controls)

### 3.2 Interaction Model

| Action | Behavior |
|--------|----------|
| **Right-click morph slider** | Popup menu: "Set Source (Current)", "Set Target from Preset...", "Load Target from A/B Slot" |
| **Drag morph slider** | Real-time interpolation between source and target |
| **Release morph slider** | Parameters remain at the interpolated position |
| **Double-click morph slider** | Snap to center (0.5) |
| **Ctrl+Z** | Undo morph (restore state before morph started) |

### 3.3 Preset Menu Integration

When the user selects a preset from the Presets menu while the morph slider
is visible, the selected preset becomes the **target** state. The morph
slider resets to 0.0 (source) and the user can drag to morph toward the
target.

### 3.4 Standalone vs Plugin

| Aspect | Plugin (VST3/AU) | Standalone |
|--------|-------------------|------------|
| Parameter automation | Host can automate the morph slider | No automation |
| State persistence | Morph position saved in project state | Saved in user preferences |
| Target preset loading | From plugin preset menu | From file browser |
| A/B integration | Morph between A and B slots | Same |

### 3.5 Ergonomic Constraints

1. **No modal dialogs** — morphing is always non-destructive and reversible
2. **Visual feedback** — the morph slider position is reflected in real-time
   in the curve editor (the pitch curve visibly morphs)
3. **Parameter names** — source/target preset names are displayed as labels
   to orient the user
4. **Performance** — morphing must not cause audio dropouts; all parameter
   writes happen on the message thread, DSP reads happen on the audio thread
   via lock-free atomics (already the case for all parameters)

---

## 4. Development Phases

### Phase 1: Core Interpolation Engine (no UI)

**Duration**: 2-3 days

1. Create `Source/dsp/PresetMorpher.h`:
   - `struct MorphState` — snapshot of all interpolable parameters + PitchCurve
   - `MorphState captureState(OpenVoxTunerAudioProcessor&)` — captures current state
   - `MorphState loadStateFromXml(const juce::XmlElement&)` — loads from preset XML
   - `void applyInterpolatedState(OpenVoxTunerAudioProcessor&, const MorphState& source,
                                   const MorphState& target, float morphAmount)` — applies lerped state
   - `PitchCurve interpolateCurves(const PitchCurve& a, const PitchCurve& b, float t)` — curve morphing

2. Implement parameter classification logic (continuous/discrete/boolean)

3. Implement PitchCurve interpolation (128-sample resampling approach)

**Validation**: Unit tests for interpolation correctness, boundary conditions,
and PitchCurve morphing.

### Phase 2: Morph Slider UI

**Duration**: 2-3 days

1. Add `morphSlider` (juce::Slider) to `PluginEditor.h`:
   - Position in header strip
   - Labels for source/target names

2. Add morph slider logic to `PluginEditor.cpp`:
   - `sliderValueChanged()` callback triggers interpolation
   - Right-click menu for source/target management
   - Integration with existing preset menu

3. Add state members:
   - `MorphState morphSource, morphTarget`
   - `bool morphActive = false`
   - `float lastMorphValue = 0.0f`

4. Add to `getStateInformation()` / `setStateInformation()`:
   - Save morph slider position and source/target state names

**Validation**: Manual testing with various presets, verify no audio glitches.

### Phase 3: Curve Editor Visual Feedback

**Duration**: 1-2 days

1. In `PitchCurveEditor`, display the morphed curve in real-time as the
   slider moves (a ghost curve showing the target state)

2. Visual distinction: source curve in normal color, target curve as a
   semi-transparent overlay, current morphed curve as the active curve

**Validation**: Visual inspection, verify curve updates smoothly during morph.

### Phase 4: A/B Integration

**Duration**: 1 day

1. Add "Morph from A to B" option in the morph slider context menu
2. When selected, captures slot A as source, slot B as target
3. Morphing between A and B in real-time

**Validation**: Test A→B morphing preserves all parameters including curve.

### Phase 5: Polish and Edge Cases

**Duration**: 1-2 days

1. Handle edge cases:
   - Morphing while audio is playing
   - Changing preset while morph is active
   - Undo/redo during morph
   - State save/restore with active morph

2. Performance optimization:
   - Only recalculate interpolation when morph value changes (dirty flag)
   - Cache interpolated PitchCurve to avoid recalculation on every
     `processBlock` call

3. Final UI polish:
   - Smooth animation when morph slider snaps
   - Tooltip showing current morph percentage

**Validation**: Stress testing with rapid parameter changes, long morph
operations, and state save/restore cycles.

---

## 5. Success Criteria

### 5.1 Functional Criteria

| Criterion | Metric |
|-----------|--------|
| **Glitch-free audio** | Zero audio glitches during morph at any speed |
| **Parameter coverage** | All continuous parameters interpolated correctly |
| **Curve interpolation** | Morphed curve is smooth and musically coherent |
| **Latency** | Morph responds within 1 audio block (< 5ms at 44.1kHz/512 samples) |
| **State preservation** | Source and target states survive plugin reload |
| **Undo/redo** | Morph operation is fully reversible |

### 5.2 Performance Criteria

| Criterion | Metric |
|-----------|--------|
| **CPU overhead** | < 0.5% additional CPU during morph (parameter lerp is trivial) |
| **Memory** | < 50KB additional for storing two MorphStates |
| **Audio thread** | No allocations or locks on the audio thread during morph |

### 5.3 UX Criteria

| Criterion | Metric |
|-----------|--------|
| **Discoverability** | User can find and use morph within 30 seconds |
| **Visual feedback** | Curve editor updates in real-time during morph |
| **Labeling** | Source and target preset names always visible |
| **Reversibility** | One-click reset to source state |

---

## 6. Risk Assessment

### 6.1 High Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **Audio glitches during parameter changes** | High — unacceptable artifacts | Use existing smoothing infrastructure (`RetargetEnvelope`, `LinearSmoothedValue`). Test with all parameter combinations. Add ramp time for discrete parameter flips. |
| **Parameter automation conflicts** | Medium — host automation may fight with morph | Use `setValueNotifyingHost()` which does NOT trigger host automation recording. Document that morph is a manual performance tool, not automatable. |
| **PitchCurve interpolation artifacts** | Medium — musically incoherent curves | Limit morph speed. Add visual preview before committing. Use 128-sample resolution which is sufficient for smooth curves. |

### 6.2 Medium Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **State size bloat** | Low — two full states in memory | States are XML-based, typically < 10KB each. Total < 50KB is negligible. |
| **Discrete parameter "popping"** | Medium — audible click when boolean/discrete params flip | Apply short crossfade (5ms) around the 50% threshold. Use the existing `LinearSmoothedValue` for gain smoothing during the flip. |
| **Custom scale interpolation** | Low — custom scale booleans don't lerp | Treat as binary: flip at 50% morph. Visual feedback shows which scale is active. |

### 6.3 Low Risks

| Risk | Impact | Mitigation |
|------|--------|------------|
| **UI layout overflow** | Low — morph slider may not fit on small screens | Make morph slider collapsible. Use minimum width constraint. |
| **Preset format changes** | Low — future preset format updates may break morph state | Version the MorphState XML format. Add fallback for missing fields. |

---

## 7. File Impact Summary

| File | Changes |
|------|---------|
| `Source/dsp/PresetMorpher.h` | **NEW** — MorphState struct, interpolation engine |
| `Source/PluginEditor.h` | Add morph slider, morph state members, right-click menu |
| `Source/PluginEditor.cpp` | Morph slider setup, callback logic, preset menu integration |
| `Source/PluginProcessor.h` | Add morph-related accessors (getMorphAmount, setMorphSource) |
| `Source/PluginProcessor.cpp` | Save/restore morph state in getStateInformation/setStateInformation |
| `Source/ui/PitchCurveEditor.h` | Add ghost curve overlay for target curve visualization |
| `Source/ui/PitchCurveEditor.cpp` | Draw target curve as semi-transparent overlay during morph |
| `Source/ui/OVTTheme.h` | Add morph slider color constants |
| `README.md` | Document the morph feature |

---

## 8. Dependencies

- **No new external dependencies** — all implementations use existing JUCE
  and plugin infrastructure
- **Reuses existing**:
  - `AudioProcessorValueTreeState` parameter system
  - `PitchCurve::toXml()` / `fromXml()` for state serialization
  - A/B slot mechanism for state capture
  - `LinearSmoothedValue` for audio-safe parameter transitions
  - `LookAndFeel` system for morph slider styling

---

## 9. Future Extensions

1. **Morph automation** — record morph as DAW automation lane (requires host
   cooperation)
2. **Multi-preset morph** — morph between 3+ presets using a radial UI
3. **Morph presets** — save/restore morph configurations (source + target pairs)
4. **Morph recording** — record morph movements as a performance, replay later
5. **Morph curves** — non-linear morph interpolation (ease-in, ease-out, S-curve)
