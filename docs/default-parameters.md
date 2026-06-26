# Default Parameters Reference — OpenVoxTuner v0.1.1

## Audio Processing

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Speed | `speed` | Float | **50.0** | 0 – 200 ms |
| Latency Mode | `latency_mode` | Choice | **1 (Quality)** | 0=Low Latency, 1=Quality, 2=Safe |
| Amount | `amount` | Float | **1.0** (100%) | 0 – 1 (0–100%) |
| Bypass | `bypass` | Bool | **false** | false / true |
| Mode | `mode` | Choice | **0 (Live)** | 0=Live, 1=Curve Editor |

## Formant

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Formant Shift | `formant` | Float | **0.0** | -5.0 – +5.0 semi-tons |
| Formant Enable | `formant_enable` | Bool | **false** | false / true |

## Pitch Correction (Scale)

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Key | `key` | Int | **0 (C)** | 0 – 11 (C → B) |
| Scale | `scale` | Choice | **0 (Chromatic)** | 0=Chromatic … 13=Custom |
| Custom C | `custom0` | Bool | **true** | |
| Custom C# | `custom1` | Bool | **false** | |
| Custom D | `custom2` | Bool | **true** | |
| Custom D# | `custom3` | Bool | **false** | |
| Custom E | `custom4` | Bool | **true** | |
| Custom F | `custom5` | Bool | **true** | |
| Custom F# | `custom6` | Bool | **false** | |
| Custom G | `custom7` | Bool | **true** | |
| Custom G# | `custom8` | Bool | **false** | |
| Custom A | `custom9` | Bool | **true** | |
| Custom A# | `custom10` | Bool | **false** | |
| Custom B | `custom11` | Bool | **true** | |

## Harmony

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Harmony Type | `harmony_type` | Choice | **0 (None)** | 0=None … 11=Drone |
| Harmony Enable | `harmony_enable` | Bool | **false** | false / true |
| Harmony Volume | `harmony_gain` | Float | **1.0** | 0 – 1 |
| Harmony Blend | `harmony_blend` | Float | **0.5** | 0 – 1 |
| Use Voice for Harmony | `harmony_use_voice` | Bool | **true** | false / true |
| Shifted Voices | `harmony_shifted_voices` | Int | **4** | 1 – 4 |
| Harmony Tone | `harmony_tone` | Choice | **0 (Choir)** | 0=Choir, 1=Bright, 2=Synth Lead, 3=Strings, 4=Guitar, 5=Vocoder-like |
| Harmony Tone Color | `harmony_tone_color` | Float | **0.5** | 0 – 1 |

## MIDI

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| MIDI Out Enable | `midi_out_enable` | Bool | **true** | false / true |

## Curve Editor

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Editor Measures | `editor_measures` | Int | **4** | 1 – 32 |
| Auto Scroll | `auto_scroll` | Bool | **true** | false / true |

## Curve Editor — Interactive State (not in processor parameters)

| State | Default |
|-------|---------|
| Snap to scale | **ON** |
| Snap to grid | **ON** |
| Step mode | **ON** |

## Debug

| Parameter | ID | Type | Default |
|-----------|-----|------|---------|
| Debug Test Grain | `dbg_test_grain` | Bool | **false** |

## Factory Presets

| Preset | Notes | Type |
|--------|-------|------|
| default | Flat line at A3 (220 Hz) | Constant |
| robot_c3 | Flat line at C3 (130.81 Hz) | Constant |
| robot_c4 | Flat line at C4 (261.63 Hz) | Constant |
| spoken_male | Natural undulation around B2 (120 Hz) | Curved |
| spoken_female | Natural undulation around A3 (220 Hz) | Curved |
| bass | E2→G2→A2→C3 melody (4 notes) | Melodic |
| baritone | A2→C3→D3→F3 melody | Melodic |
| tenor | C3→D3→E3→G3 melody | Melodic |
| alto | F3→G3→A3→C4 melody | Melodic |
| mezzo | A3→B3→C4→E4 melody | Melodic |
| soprano | C4→D4→E4→G4 melody | Melodic |