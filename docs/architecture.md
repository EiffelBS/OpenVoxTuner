# Architecture of the Autotune Clone Plugin

## Overview

The plugin is an **audio effect** (not an instrument): it receives a mono or stereo audio signal and returns it transposed in pitch according to a chosen musical scale.

It is implemented in **C++** using the **JUCE 8** framework, which provides:
- the `AudioProcessor` interface (the audio pipeline);
- the `AudioProcessorEditor` interface (the GUI);
- VST3/Standalone/AU/AAX integration;
- DSP tools (FFT, windowing, smoothing).

## Source Tree

```
Source/
  PluginProcessor.h / .cpp        # Pipeline audio (point d'entree DSP)
  PluginEditor.h / .cpp          # GUI (point d'entree visuel)
  dsp/
    PitchDetector.h / .cpp        # Module 1 : detection de pitch (YIN)
    ScaleQuantizer.h / .cpp       # Module 2 : quantification tonique+mode
    PitchShifter.h / .cpp         # Module 3 : transposition (PSOLA)
    NoteUtils.h                   # Utilitaires Hz <-> MIDI <-> nom de note
  ui/
    PitchVisualizer.h / .cpp      # Composant GUI : courbes de pitch
                                  # + note chantee + cents + meter
    PitchCurveEditor.h / .cpp     # Editeur interactif de pitch curve
    PianoKeyboard.h / .cpp        # Clavier de piano vertical (gauche)
```

## DSP Pipeline

```
                   +-------------------+   f0_in
   Audio In  ----> |  PitchDetector    | ------------+
                   +-------------------+             |
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
  - Difference function
  - Normalized cumulative auto-correlation function
  - Clarity threshold (0.10-0.15)
  - Parabolic interpolation for sub-sample precision

- **Quantization**: projection onto the nearest scale note
  - Hz -> semitones conversion (relative to A4 = 440 Hz)
  - Search for the nearest semitone belonging to the scale
  - Inverse conversion back to Hz

- **Pitch shifting**: simplified PSOLA
  - Pitch mark detection (zero-crossings of the detected pitch)
  - Windowed grain segmentation (Hann, 2-4 fundamental periods)
  - Overlap-add with positions recalculated according to the ratio
  - Simplified phase vocoder for the stationary component (Phase 4)

### Phase 4 - Quality

#### PSOLA (Phase 4 implementation)

The PSOLA (Pitch-Synchronous Overlap-Add) algorithm is the standard technique for transposing quasi-periodic signals such as voice without altering the duration.

**Pipeline**:
1. **f0 detection**: call the `PitchDetector` (YIN) on the input buffer.
2. **Pitch mark detection**: for each fundamental period (sample `period = sr / f0`), search for the local amplitude maximum within a window of +/- 25% of the period. This sets the "phase" of each grain.
3. **PSOLA grain**: for each analysis pitch mark, extract a 2-period Hann-windowed grain centered on the mark.
4. **Re-positioning**: place this grain at the synthesis position (`synthMark = m * synthPeriod`, where `synthPeriod = period / ratio`).
5. **Overlap-Add (OLA)**: add the grains into the output buffer with a 2-period Hann window / 1-period hop, which satisfies the COLA condition (Constant OverLap-Add factor = 1 over stationary regions).

**Notes**:
- The grain is 2 periods wide to avoid abrupt cuts.
- The Hann window is normalized by /2 (theoretical overlap gain).
- The algorithm is O(N) where N is the number of pitch marks.

#### Formant Compensation (Phase 4)

When transposing a vocal signal via PSOLA, the formants (vocal tract resonances) are shifted along with the pitch. This produces an unnatural "chipmunk" effect (the voice becomes "thinner" when raised).

**Implemented solution**: simplified "LP-filter + resample" technique.
- **Problem analysis**: formants lie in the upper part of the spectrum. Raising the pitch shifts them upward in absolute value, but their relative position with respect to f0 changes.
- **Solution**: before PSOLA, apply a 2nd-order Butterworth low-pass filter whose cutoff frequency is inversely proportional to the transposition ratio (compensation via `sqrt(ratio)`).
- **Why sqrt**: a simple `1/ratio` would be too aggressive; a plain `1.0` corrects nothing. The `sqrt` compromise yields a natural result.
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
- Editable "graphical" pitch curve mode (Melodyne style): SEE BELOW

## "Graphic" Mode (Phase 4 - Melodyne style)

Graphic mode lets the user **draw the ideal pitch curve** that the audio should follow over time. This is what distinguishes an Auto-Tune Pro from a basic plugin.

### Auto Mode vs Graphic Mode

| Mode    | Target pitch source                                      | Usage                       |
|---------|----------------------------------------------------------|-----------------------------|
| Auto    | Automatic quantization to the scale (Key/Scale)          | Live singing, fast          |
| Graphic | Pitch curve drawn with the mouse (points + interpolation)| Offline mixing, perfection  |

The user switches between the two via the "Mode" ComboBox in the GUI. The mode is saved in the plugin state.

### PitchCurve: Data Structure

`atdsp::PitchCurve` is a sorted list of `PitchPoint { time (s), pitch (Hz) }`.
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

The mode is saved in the plugin state via the "mode" parameter (AudioParameterInt 0/1). The PitchCurve itself is serialized as an XML sub-element `<PITCH_CURVE>` in `getStateInformation()`.

### Current Limitations

- No Bezier curves (linear interpolation only).
- No snapping other than snap-to-scale.
- No direct capture of the current pitch by clicking (but possible via `capturePitch()` exposed in the API).
- No zoom (the range is fixed to 4 seconds, 50-1000 Hz).

## Exposed Parameters

| Name     | Type      | Range              | Default | Description                            |
|----------|-----------|--------------------|---------|----------------------------------------|
| Speed    | float ms  | 0 - 200            | 50      | Correction retargeting time            |
| Amount   | float 0-1 | 0.0 - 1.0          | 1.0     | Intensity (0 = passthrough)            |
| Key      | int       | 0 - 11             | 0 (C)   | Scale tonic                            |
| Scale    | int       | 0 - 5              | 0 (Maj) | Mode (Maj, Min, Pent Maj, Pent Min, Chr, Custom) |
| Bypass   | bool      | off / on           | off     | Processing bypass                      |
| custom0..custom11 | bool | off / on      | (Maj)   | Active notes for the Custom scale (C, C#, D, ..., B) |

## Custom Scale (Custom mode)

The `Scale::Custom` mode (index 5) lets the user choose which notes (in semitones 0..11) belong to the scale.
Implementation:
- 12 `AudioParameterBool` exposed to the host (individual automation possible)
- 12 `juce::ToggleButton` in the GUI, arranged in a horizontal row
- Visible only if Scale = "Custom" (handled by `updateCustomVisibility()`)
- The `ScaleQuantizer` receives the note list via `setCustomIntervals()` (without key offset, unlike other modes)
- The interactive snap of `PitchCurveEditor` uses `snapToScaleCustom()` for quantization on the chosen notes

## Real-Time Display (sung note + cents + meter)

The `PitchVisualizer` permanently displays:
- **The sung note name** (e.g., "F3") in its header.
- **The target note name** (e.g., "-> F3") if it differs.
- **The cents offset** (e.g., "-50 c") with color coding:
  - green (|c| < 5): within the note
  - yellow (|c| < 25): close
  - red (|c| >= 25): clearly off
- **A vertical tuning meter** (needle according to cents, graduations at +/-50 and +/-100, Antares / Studio One style).
- **The current scale note lines** in the background (semi-transparent yellow) over 4 octaves (C2 -> C6).

The information computation is done by `atdsp::describePitch()` in `NoteUtils.h` (Hz -> MIDI -> name conversion + cents offset calculation between input pitch and quantized pitch).

## Vertical Piano Keyboard (PianoKeyboard)

The `ui::PianoKeyboard` is a component placed to the left of `PitchCurveEditor` (40 px wide) that draws a vertical piano keyboard:
- **White keys** (C, D, E, F, G, A, B) full width
- **Black keys** (C#, D#, F#, G#, A#) overlaid, 60% shorter
- **Scale notes highlighted in yellow** (lets you immediately see which notes are "allowed" by the current scale)
- **Octave labels** (C2, C3, ...) on the left of the C keys

The Y axis is vertical: low notes at the BOTTOM, high notes at the TOP.
Default range: C2 (MIDI 36) -> C7 (MIDI 96), sufficient for voice.

## Latency

Latency depends on the PSOLA analysis window.
MVP target: **20-30 ms** (acceptable for monitoring).
The exact latency is declared to the host via `AudioProcessor::getLatencySamples()`.

## Multi-format

| Format  | Status         | Platform      | Notes                              |
|---------|----------------|---------------|------------------------------------|
| VST3    | Active         | Windows       | Compiled and testable              |
| Standalone | Active       | Windows       | .exe application, test without DAW |
| AU      | Configured     | macOS         | Requires a Mac to compile          |
| AAX     | Configured     | macOS         | Requires Mac + Avid dev            |
| LV2     | Not configured | Linux         | Add if needed                     |

## Architectural Decisions

1. **Isolated DSP modules**: PitchDetector, ScaleQuantizer and PitchShifter are separate classes with a single responsibility. This allows testing them independently and replacing them.

2. **AudioProcessorValueTreeState**: parameters are managed by JUCE's value tree, which automatically synchronizes host <-> GUI <-> DSP.

3. **No external dependency**: we use only JUCE (provided). No third-party library for DSP (no libsoxr, no rubberband) in order to keep the project simple and controlled.

4. **C++17**: we target this standard (defined by Projucer) to benefit from `<optional>`, `if constexpr`, etc. without requiring C++20.

## References

- de Cheveigne, A., & Kawahara, H. (2002). YIN, a fundamental frequency estimator for speech and music. JASA.
- Moulines, E., & Charpentier, F. (1990). Pitch-synchronous waveform processing techniques for text-to-speech synthesis using diphones. Speech Communication.
- McLeod, P., & Wyvill, G. (2005). A smarter way to find pitch.
- Zölzer, U. (2011). DAFX: Digital Audio Effects (2nd ed.). Wiley.
