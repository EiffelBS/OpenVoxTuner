# Architecture of the OpenVoxTuner Audio Plugin

## Overview

The plugin is an **audio effect** (not an instrument): it receives a mono or stereo audio signal and returns it transposed in pitch according to a chosen musical scale.

It is implemented in **C++** using the **JUCE 8** framework, which provides:
- the `AudioProcessor` interface (the audio pipeline);
- the `AudioProcessorEditor` interface (the GUI);
- VST3/Standalone/AU integration (AAX is **not** built);
- DSP tools (FFT, windowing, smoothing);
- ARA2 (Audio Random Access) support for DAW timeline sync.

## Source Tree

```
Source/
  PluginProcessor.h / .cpp        # Audio pipeline (DSP entry point)
  PluginEditor.h / .cpp           # GUI (visual entry point)
  BuildInfo.h.in                  # CMake-generated build metadata
  dsp/
    YinPitchDetector.h / .cpp      # Pitch detection (active YIN implementation)
    PitchDetector.h / .cpp        # Legacy YIN detector (superseded by YinPitchDetector)
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

```
                   +----------------------+   f0_in
   Audio In  ----> |  YinPitchDetector    | ------------+
                   +----------------------+             |
                                                    v
                                          +----------------------+
                                          |   ScaleQuantizer     |
                                          |   (key, scale)       |
                                          +----------------------+
                                                    |
                                                    v  f0_target
                                          +----------------------+
   Audio Out <--- |    PitchShifter     | <----------------------+
                   |    (PSOLA)         |   ratio = f0_target/f0_in
                   +--------------------+
```

## Algorithms Used

### Phase 1 - Functional MVP

- **Pitch detection**: YIN algorithm (by Cheveigne & Kawahara, 2002)
  - Implemented by `YinPitchDetector` (the active implementation; the older
    `PitchDetector` class is legacy and no longer used by the pipeline)
  - Difference function
  - Cumulative mean normalized difference function d'(tau)
  - Clarity threshold (default 0.05, adjustable ~0.05-0.15)
  - Parabolic interpolation for sub-sample precision

- **Quantization**: projection onto the nearest scale note
  - Hz -> semitones conversion (relative to A4 = 440 Hz)
  - Search for the nearest semitone belonging to the scale
  - Inverse conversion back to Hz

- **Pitch shifting**: simplified PSOLA (Phase-Synchronous Overlap-Add)
  - Pitch-mark detection per fundamental period (period = sr / f0)
  - Windowed grain segmentation (Hann)
  - Overlap-add with positions recalculated according to the ratio
  - (No phase vocoder is used; PSOLA is the only pitch-shifting algorithm.
    SWIPE, PYIN, RubberBand and SoundTouch were evaluated and removed.)

### Phase 4 - Quality

#### PSOLA (Phase 4 implementation)

The PSOLA (Pitch-Synchronous Overlap-Add) algorithm is the standard technique for transposing quasi-periodic signals such as voice without altering the duration. It is the only pitch shifter in the codebase and is the sole implementation of `IPitchShifter`.

**Pipeline**:
1. **f0 detection**: call the `YinPitchDetector` (YIN) on the input buffer.
2. **Pitch-mark detection**: for each fundamental period (sample `period = sr / f0`), advance an output phase and create a grain when the phase wraps.
3. **PSOLA grain**: for each analysis pitch mark, extract a Hann-windowed grain centered on the mark, whose length is scaled by the formant ratio.
4. **Re-positioning**: place this grain at the synthesis position, using correlation (`findBestOffset`) to align it with the previous grain for smooth overlap-add.
5. **Overlap-Add (OLA)**: add the grains into the output buffer with a Hann window / 1-period hop, which satisfies the COLA condition over stationary regions.

**Notes**:
- The grain length is scaled by `2 * max(Tin / F, Tout)` so the formant ratio `F` preserves vocal-tract resonances.
- The algorithm is O(N) where N is the number of pitch marks.

#### Formant Compensation (Phase 4)

When transposing a vocal signal via PSOLA, the formants (vocal tract resonances) are shifted along with the pitch. This produces an unnatural "chipmunk" effect (the voice becomes "thinner" when raised).

**Implemented solution**: simplified "LP-filter + resample" technique, provided by the `FormantPreserver` module (2nd-order Butterworth low-pass whose cutoff follows the transposition ratio).
- **Problem analysis**: formants lie in the upper part of the spectrum. Raising the pitch shifts them upward in absolute value, but their relative position with respect to f0 changes.
- **Solution**: before PSOLA, apply a 2nd-order Butterworth low-pass filter whose cutoff frequency is inversely proportional to the transposition ratio. In the live pipeline the same formant shift is applied to the PSOLA grains via a formant ratio (`formantRatio = 2^(semitones/12)`).
- **Why sqrt / inverse ratio**: the cutoff is moved in the opposite direction of the pitch so PSOLA restores the formants to their original position.
- **Limitations**: we do not exactly preserve the formants; we deform them in a plausible way. Exact preservation would require a linear prediction (LPC) model and non-uniform resampling.

#### Retarget Envelope (Antares "Speed" style)

The `Speed` parameter controls how quickly the pitch follows the target note:
- `Speed = 0 ms`: instant correction (T-Pain style "robotic" effect)
- `Speed = 50 ms`: fast but smooth correction (Antares default)
- `Speed = 200 ms`: slow and very natural correction (almost no effect)

**Implementation**: a 1st-order IIR filter (exponential smoothing):
```
y[n] = y[n-1] + alpha * (target - y[n-1])
alpha = 1 - exp(-dt / tau)   where  tau = speedMs / 1000
```

This gives an exponential response with time constant `tau`:
- After `tau`: 63% of the target reached
- After `3*tau`: 95%
- After `5*tau`: 99%

### Future Phase

- Pitch detection via MPM (McLeod Pitch Method) in addition to YIN
- Exact formant preservation via LPC + non-uniform resampling
- Transient preservation (onset detection -> PSOLA bypass)
- Additional YIN refinements (the legacy `PitchDetector` and other detectors were removed)

## "Graphic" Mode (Phase 4 - Melodyne style)

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

| Name     | Type      | Range / Choices                                    | Default             | Description                                   |
|----------|-----------|----------------------------------------------------|---------------------|-----------------------------------------------|
| speed    | float ms  | 0 - 200                                            | 20                  | Correction retargeting time                   |
| latency_mode | choice | Direct Monitoring / Low Latency / Quality / Safe  | Low Latency (1)     | Latency quality mode                          |
| amount   | float 0-1 | 0.0 - 1.0                                          | 1.0                 | Intensity (0 = passthrough)                   |
| formant  | float     | -5.0 - 5.0 (semitones)                             | 0.0                 | Formant shift                                 |
| formant_enable | bool | off / on                                          | off                 | Formant shift enable                          |
| key      | int       | 0 - 11                                             | 0 (C)               | Scale tonic                                   |
| scale    | choice    | Chromatic, Major, Melodic Minor, Harmonic Minor, Natural Minor, Major Pentatonic, Minor Pentatonic, Blues, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Custom | Chromatic (0) | Scale mode (14 types) |
| custom0..custom11 | bool | off / on                                         | C major (C,D,E,F,G,A,B) | Active notes for the Custom scale         |
| bypass   | bool      | off / on                                           | off                 | Processing bypass                             |
| mode     | choice    | Live / Curve Editor                                | Live (0)            | Editor mode                                   |
| harmony_type | choice | None + 21 harmony types | 3rd Below + Above (3) | Harmony type                          |
| harmony_enable | bool | off / on                                         | off                 | Harmony master enable                         |
| harmony_gain | float | 0.0 - 1.0                                        | 0.75                | Harmony volume                                |
| harmony_blend | float | 0.0 - 1.0                                        | 0.5                 | Harmony blend (lead / harmony)                |
| harmony_use_voice | bool | off / on                                        | on                  | Use (shifted) voice for harmony               |
| harmony_shifted_voices | int | 1 - 4                                        | 4                   | Number of shifted voices                      |
| harmony_tone | choice | Choir, Bright, Synth Lead, Strings, Guitar, Vocoder-like | Choir (0)    | Harmony tone                                  |
| harmony_tone_color | float | 0.0 - 1.0                                      | 0.5                 | Harmony tone color                            |
| midi_out_enable | bool | off / on                                        | on (plugin) / off (standalone) | MIDI out enable                  |
| pitch_detector | choice | YIN / Reserved                                  | YIN (0)             | Pitch detector (YIN only)                     |
| reverb_enable | bool | off / on                                         | off                 | Reverb enable                                 |
| reverb_mix | float   | 0.0 - 1.0                                          | 0.30                | Reverb mix                                    |
| noise_gate_enable | bool | off / on                                       | off                 | Noise gate enable                             |
| noise_gate_threshold | float | -80.0 - 0.0 (dB)                              | -40.0               | Gate threshold                                |
| flex_tune | float    | 0.0 - 100.0 (cents)                               | 10.0                | FlexTune deadband around target note          |
| humanize | float    | 0.0 - 50.0 (cents)                                | 40.0                | Humanize fluctuation                          |
| correction_mode | bool | Modern (false) / Transparent (true)             | Modern (false)      | Correction mode                               |
| ui_theme | int      | 0 - 1                                              | 0 (Dark)            | UI theme                                      |
| ui_language | int    | 0 - 5 (EN, FR, DE, ES, JA, ZH)                     | 0 (English)         | UI language                                   |
| dbg_test_grain | bool | off / on                                         | off                 | Debug test grain                              |
| editor_measures | int | 1 - 32                                            | 4                   | Editor measures                               |
| auto_scroll | bool   | off / on                                           | on                  | Auto scroll                                   |

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

## Latency

Latency is reported to the host via `setLatencySamples()` and is driven by `PitchShifter::setLatencyMs()`, which clamps the requested latency to **8-40 ms**.
The `latency_mode` parameter selects one of four presets:

| latency_mode | Latency (ms) |
|--------------|--------------|
| Direct Monitoring | 10 |
| Low Latency (default) | 12 |
| Quality | 20 |
| Safe | 30 |

The `PitchShifter` default (before `latency_mode` is applied) is 20 ms.

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

## References

- de Cheveigne, A., & Kawahara, H. (2002). YIN, a fundamental frequency estimator for speech and music. JASA.
- Moulines, E., & Charpentier, F. (1990). Pitch-synchronous waveform processing techniques for text-to-speech synthesis using diphones. Speech Communication.
- McLeod, P., & Wyvill, G. (2005). A smarter way to find pitch.
- Zölzer, U. (2011). DAFX: Digital Audio Effects (2nd ed.). Wiley.
