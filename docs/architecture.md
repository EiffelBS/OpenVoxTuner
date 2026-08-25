# Architecture of the OpenVoxTuner Audio Plugin

## Overview

The plugin is an **audio effect** (not an instrument): it receives a mono or stereo audio signal and returns it transposed in pitch according to a chosen musical scale.

It is implemented in **C++** using the **JUCE 8** framework, which provides:
- the `AudioProcessor` interface (the audio pipeline);
- the `AudioProcessorEditor` interface (the GUI);
- VST3/Standalone/AU integration (AAX is **not** built);
- DSP tools (FFT, windowing, smoothing);
- ARA2 (Audio Random Access) support for DAW timeline sync.

The real-time signal chain (DSP stages, algorithms, latency presets) is
detailed in the dedicated [DSP Pipeline](architecture/dsp-pipeline.md) page.

## Source Tree

```
Source/
  PluginProcessor.h / .cpp        # Audio pipeline (DSP entry point)
  PluginEditor.h / .cpp           # GUI (visual entry point)
  BuildInfo.h.in                  # CMake-generated build metadata
  dsp/
    YinPitchDetector.h / .cpp      # Pitch detection (active YIN implementation)
    IPitchDetector.h              # Pitch detector interface
    ScaleQuantizer.h / .cpp       # Quantization to the nearest scale note
    PitchShifter.h / .cpp         # Pitch shifting (PSOLA, implements IPitchShifter)
    IPitchShifter.h               # Pitch shifter interface
    NoteUtils.h                   # Hz <-> MIDI <-> note-name utilities
    PitchCurve.h / .cpp           # Graphical pitch curve data structure
    FormantPreserver.h / .cpp     # Formant compensation (2nd-order Butterworth LP)
    RetargetEnvelope.h / .cpp     # Speed / retarget smoothing (1st-order IIR)
    NoiseGate.h                   # Input noise gate (before pitch detection)
    ReverbEffect.h / .cpp         # Post-processing reverb (implements IEffect)
    IEffect.h                     # Post-effect interface
    HarmonyEngine.h               # Harmony voice generation
    PresetMorpher.h               # A/B preset morphing engine
  ui/
    PitchVisualizer.h / .cpp      # GUI: pitch curves + note + cents + meter
    PitchCurveEditor.h / .cpp     # Interactive pitch curve editor
    PianoKeyboard.h / .cpp        # Vertical piano keyboard (left side)
    ScaleKeyboardComponent.h / .cpp # 12-button custom scale editor
    LookAndFeel.h / .cpp          # JUCE LookAndFeel customization
    OVTTheme.h                    # Centralized dark/light theme accessors
    OVTLanguages.h                # i18n string tables
    OVTFonts.h                    # Centralized font system
  external/
    presonus/ipsleditcontroller.h # Presonus micro-view VST3 extension
```

## DSP Pipeline

The real-time signal chain is:

```
NoiseGate -> YinPitchDetector -> ScaleQuantizer -> RetargetEnvelope
          -> PitchShifter (PSOLA) -> HarmonyEngine (optional) -> ReverbEffect (optional)
```

The [DSP Pipeline](architecture/dsp-pipeline.md) page is the single source of
truth for the stage-by-stage description: algorithm details, latency presets
(8-40 ms), formant strategies (Current / P0 / P1 / P2) and how ARA2 augments
the input stage without re-routing the chain. This page focuses on the module
map, modes and UI architecture.

## "Graphic" Mode (Phase 4 - graphical curve editor)

Graphic mode lets the user **draw the ideal pitch curve** that the audio should follow over time. This is what distinguishes an Auto-Tune Pro from a basic plugin.

### Auto Mode vs Graphic Mode

| Mode    | Target pitch source                                      | Usage                       |
|---------|----------------------------------------------------------|-----------------------------|
| Auto    | Automatic quantization to the scale (Key/Scale)          | Live singing, fast          |
| Graphic | Pitch curve drawn with the mouse (points + interpolation)| Offline mixing, perfection  |

The user switches between the two via the "Mode" ComboBox in the GUI. The mode is saved in the plugin state.

### PitchCurve: Data Structure

`ovtdsp::PitchCurve` is a sorted list of `PitchPoint { time (s), pitch (Hz) }`.
- Minimum 0 points: default auto mode.
- 1 point: constant value (the pitch curve holds this value).
- N points: linear interpolation between 2 consecutive points.
- Before the first point / after the last: hold the endpoint value.
- Evaluation is done by binary search (O(log N)).

### Interactive Editing (`ui::PitchCurveEditor`)

The `PitchCurveEditor` component allows to:
- **Drag** a point: move the point vertically (changes the pitch).
- **Double-click**: add a point at the cursor position.
- **Right-click on a point** (or Alt+click): delete the point.
- **Right-click in empty space**: preset menu (default, spoken, lyric, rap, robot).
- **Snap to scale**: if enabled, points are rounded to the nearest note of the current scale (Key/Scale sliders).

The component is connected to the processor via a Listener pattern: on each modification, `pitchCurveChanged()` is called, and the editor copies the curve to `processorRef.getPitchCurve()`.

### Factory Presets

| Preset     | Description                                               |
|------------|-----------------------------------------------------------|
| default    | Flat curve at 440 Hz (minimal correction)                 |
| spoken     | Spoken voice: slight oscillation around 200 Hz            |
| lyric      | Lyrical singing: large expressive variations A3..A4       |
| rap        | Rising and falling ascents (~200-250 Hz)                  |
| robot      | Same as default (placeholder for extreme "T-Pain" effect -> would require Speed=0) |

### Wiring in processBlock

The processor's `processBlock` queries `getPlayHead()->getPosition()` to get the transport time in seconds. In Graphic mode, this time is passed to `pitchCurve->getPitchAt(t, f0_in)` which returns the target Hz. The rest of the pipeline (amount blend, retarget, formants, PSOLA) is unchanged.

The mode is saved in the plugin state via the "mode" parameter (AudioParameterChoice 0/1). The PitchCurve itself is serialized as an XML sub-element `<PITCH_CURVE>` in `getStateInformation()`.

### Current Limitations

- No Bezier curves (linear interpolation only).
- No snapping other than snap-to-scale.
- No direct capture of the current pitch by clicking (but possible via `capturePitch()` exposed in the API).
- No zoom (the range is fixed to 4 seconds, 50-1000 Hz).

## Exposed Parameters

The complete parameter reference (IDs, types, defaults, ranges) is maintained
in [Default Parameters](default-parameters.md), which is kept in sync with
`Source/PluginProcessor.cpp`. In summary:

- **Correction**: speed, latency_mode, amount, bypass, mode, correction_mode,
  pitch_detector
- **Key / Scale**: key, scale (+ custom0..custom11 for Custom mode),
  key_detect, key_source, companion_group
- **Formants**: formant, formant_enable, formant_strategy
- **Harmony**: harmony_type, harmony_enable, harmony_gain, harmony_blend,
  harmony_use_voice, harmony_shifted_voices, harmony_tone, harmony_tone_color,
  harmony_gain_match
- **Effects**: reverb_enable, reverb_mix, noise_gate_enable,
  noise_gate_threshold
- **Misc / UI**: midi_out_enable, humanize, flex_tune (deprecated), dbg_test_grain,
  ui_theme, ui_language, editor_measures, auto_scroll

## Custom Scale (Custom mode)

The `Scale::Custom` mode (index 13) lets the user choose which notes (in semitones 0..11) belong to the scale.
Implementation:
- 12 `AudioParameterBool` (`custom0`..`custom11`) exposed to the host (individual automation possible)
- 12 `juce::ToggleButton` in the GUI, arranged in a horizontal row
- Visible only if Scale = "Custom" (handled by `ScaleKeyboardComponent`)
- The `ScaleQuantizer` receives the note list via `setCustomIntervals()` (without key offset, unlike other modes)
- The interactive snap of `PitchCurveEditor` uses `snapToScaleCustom()` for quantization on the chosen notes

## Real-Time Display (sung note + cents + meter)

The `PitchVisualizer` permanently displays:
- **The sung note name** (e.g., "F3") in its header.
- **The target note name** (e.g., "-> F3") if it differs.
- **The cents offset** (e.g., "-50 c") with color coding:
  - green  (|c| < 5):  in tune
  - yellow (|c| < 15): close
  - orange (|c| < 35): off
  - red    (|c| >= 35): clearly off
- **A vertical tuning meter** (needle according to cents, graduations at +/-50 and +/-100, Antares / Studio One style).
- **The current scale note lines** in the background (semi-transparent yellow) over 4 octaves (C2 -> C6).

The information computation is done by `ovtdsp::describePitch()` in `NoteUtils.h` (Hz -> MIDI -> name conversion + cents offset calculation between input pitch and quantized pitch).

## Vertical Piano Keyboard (PianoKeyboard)

The `ui::PianoKeyboard` is a component placed to the left of `PitchCurveEditor` (40 px wide) that draws a vertical piano keyboard:
- **White keys** (C, D, E, F, G, A, B) full width
- **Black keys** (C#, D#, F#, G#, A#) overlaid, 60% shorter
- **Scale notes highlighted in yellow** (lets you immediately see which notes are "allowed" by the current scale)
- **Octave labels** (C2, C3, ...) on the left of the C keys

The Y axis is vertical: low notes at the BOTTOM, high notes at the TOP.
Default range: C2 (MIDI 36) -> C7 (MIDI 96), sufficient for voice.

## Multi-format

| Format  | Status            | Platform      | Notes                              |
|---------|-------------------|---------------|------------------------------------|
| VST3    | Active            | Windows, macOS | Compiled and testable              |
| Standalone | Active          | Windows, macOS | .exe application, test without DAW |
| AU      | Active (optional) | macOS         | ON by default (`OVT_ENABLE_AU`); requires a Mac to compile |
| AAX     | Not built         | -             | Not part of the build formats      |
| LV2     | Not configured    | Linux         | Add if needed                     |

## Architectural Decisions

1. **Isolated DSP modules**: YinPitchDetector, ScaleQuantizer and PitchShifter are separate classes with a single responsibility. This allows testing them independently and replacing them.

2. **AudioProcessorValueTreeState**: parameters are managed by JUCE's value tree, which automatically synchronizes host <-> GUI <-> DSP.

3. **No external dependency**: we use only JUCE (provided). No third-party library for DSP (no libsoxr, no rubberband) in order to keep the project simple and controlled.

4. **C++17**: we target this standard, defined via **CMake** (`set(CMAKE_CXX_STANDARD 17)`), not Projucer, to benefit from `<optional>`, `if constexpr`, etc. without requiring C++20.

## Future Work

- **LPC formant preservation refinements**: the P0/P1/P2 cross-synthesis
  strategies are implemented (`LpcFormantPreserver`); remaining work is
  frame-based analysis with hop / overlap-add (formant analysis report item
  LP.7) to remove residual block-boundary artifacts at large transposition
  ratios, plus perceptual validation (MUSHRA harness).
- **Pitch detection via MPM** (McLeod Pitch Method) in addition to YIN.
- **Transient preservation** (onset detection -> PSOLA bypass).

## References

- de Cheveigne, A., & Kawahara, H. (2002). YIN, a fundamental frequency estimator for speech and music. JASA.
- Moulines, E., & Charpentier, F. (1990). Pitch-synchronous waveform processing techniques for text-to-speech synthesis using diphones. Speech Communication.
- McLeod, P., & Wyvill, G. (2005). A smarter way to find pitch.
- Zölzer, U. (2011). DAFX: Digital Audio Effects (2nd ed.). Wiley.
