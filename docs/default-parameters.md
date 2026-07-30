# Default Parameters Reference — OpenVoxTuner v0.1.1+

> Kept in sync with `Source/PluginProcessor.cpp`. Last updated: 2026-07-29.

## Audio Processing

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Speed | `speed` | Float | **20.0** | 0 – 200 ms |
| Latency Mode | `latency_mode` | Choice | **1 (Low Latency)** | 0=Direct Monitoring, 1=Low Latency, 2=Quality, 3=Safe |
| Amount | `amount` | Float | **1.0** (100%) | 0 – 1 (0–100%) |
| Bypass | `bypass` | Bool | **false** | false / true |
| Mode | `mode` | Choice | **0 (Live)** | 0=Live, 1=Curve Editor |
| Correction Mode | `correction_mode` | Bool | **true (Transparent)** | false=Modern, true=Transparent |
| Pitch Detector | `pitch_detector` | Choice | **0 (YIN)** | 0=YIN, 1=Reserved |

## Key / Scale

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Key | `key` | Int | **0 (C)** | 0 – 11 (C–B) |
| Scale | `scale` | Choice | **0 (Chromatic)** | 14 scales + Custom |
| Key/Scale Detection | `key_detect` | Bool | **false** | false / true |
| Key Source | `key_source` | Choice | **0 (Auto)** | 0=Auto, 1=OpenVoxKey, 2=Sidechain |
| Companion Group | `companion_group` | Choice | **0 (A)** | A, B, C, D |

### Custom Scale (active when Scale = Custom)

| Parameter | ID | Type | Default | Notes |
|-----------|-----|------|---------|-------|
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

## Formant

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Formant Shift | `formant` | Float | **0.0** | -5.0 – +5.0 semitones |
| Formant Enable | `formant_enable` | Bool | **false** | false / true |
| Formant Mode | `formant_mode` | Choice | **1 (MultiFormant)** | 0=Legacy, 1=MultiFormant |
| Formant Strategy | `formant_strategy` | Choice | **4 (Precise)** | 0=Subtle, 1=Balanced, 2=Marked, 3=Reactive, 4=Precise |
| Harmony Formant Shift | `harmony_formant` | Float | **0.0** | -5.0 – +5.0 semitones |

## Voice Type

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Voice Type | `voice_type` | Choice | **0 (Universal)** | Universal, Bass, Baritone, Tenor, Alto, Soprano |

**Frequency ranges (Hz):**
- Universal: 30 – 1000
- Bass: 82.41 – 329.63
- Baritone: 110 – 440
- Tenor: 130.81 – 523.25
- Alto: 174.61 – 698.46
- Soprano: 261.63 – 1046.50

## Harmony

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Harmony Type | `harmony_type` | Choice | **3 (3rd Below + Above)** | 22 options (None + 21 harmony types) |
| Harmony Enable | `harmony_enable` | Bool | **false** | false / true |
| Harmony Volume | `harmony_gain` | Float | **0.75** | 0 – 1 |
| Harmony Blend | `harmony_blend` | Float | **0.5** | 0 – 1 |
| Harmony Attack | `harmony_attack` | Float | **35.0 ms** | 1 – 300 ms |
| Use Voice | `harmony_use_voice` | Bool | **true** | false / true |
| Shifted Voices | `harmony_shifted_voices` | Int | **4** | 1 – 4 |
| Harmony Tone | `harmony_tone` | Choice | **0 (Choir)** | 0=Choir, 1=Organ |
| Harmony Tone Color | `harmony_tone_color` | Float | **0.5** | 0 – 1 |
| Harmony Follow Lead | `harmony_follow_lead` | Bool | **true** | false / true |
| Harmony Gain Match | `harmony_gain_match` | Bool | **true** | false / true |

## Effects

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Noise Gate Enable | `noise_gate_enable` | Bool | **false** | false / true |
| Gate Threshold | `noise_gate_threshold` | Float | **-50.0 dB** | -80 – 0 dB |
| Reverb Enable | `reverb_enable` | Bool | **false** | false / true |
| Reverb Mix | `reverb_mix` | Float | **0.30** | 0 – 1 |
| Upward Comp Enable | `upward_comp_enable` | Bool | **false** | false / true |
| Upward Comp Amount | `upward_comp_amount` | Float | **0.25** | 0 – 1 |

## Vibrato

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Vibrato Preserve | `vibrato_preserve` | Float | **0.20** (20%) | 0 – 1 (0–100%) |

## Deprecated (kept for preset compatibility)

| Parameter | ID | Type | Default | Notes |
|-----------|-----|------|---------|-------|
| FlexTune | `flex_tune` | Float | **0.0** (off) | Deadband around target note (0–100 cents). **DEPRECATED 2026-07-24** – DSP disabled. |
| Humanize | `humanize` | Float | **10.0** | Random pitch fluctuations (0–50 cents). Still active. |
| Attack-Aware | `attack_aware` | Bool | **false** (off) | **DEPRECATED 2026-07-24** – DSP disabled. |
| Attack Release | `attack_release` | Float | **60.0 ms** | **DEPRECATED 2026-07-24** – DSP disabled. |

## MIDI

| Parameter | ID | Type | Default | Notes |
|-----------|-----|------|---------|-------|
| MIDI Out Enable | `midi_out_enable` | Bool | **true** (plugin), **false** (standalone) | |
| MIDI Target Enable | `midi_target_enable` | Bool | **false** | External MIDI drives pitch target |

## UI / Editor

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| UI Theme | `ui_theme` | Int | **0 (Dark)** | 0=Dark, 1=Light |
| UI Language | `ui_language` | Int | **0 (English)** | 0=EN, 1=FR, 2=DE, 3=ES, 4=JA, 5=ZH |
| Show Waveform | `ui_show_waveform` | Int | **1 (On)** | 0=Off, 1=On |
| Waveform Display Type | `ui_waveform_type` | Int | **2 (Spectral)** | 0=Line, 1=Mirror, 2=Spectral |
| Auto-Center Pitch | `ui_auto_center` | Int | **0 (Off)** | 0=Off, 1=On |
| Visualizer Min Hz | `viz_fmin` | Float | **50.0** | 16 – 2000 Hz |
| Visualizer Max Hz | `viz_fmax` | Float | **1500.0** | 100 – 8372 Hz |
| Editor Measures | `editor_measures` | Int | **4** | 1 – 32 |
| Editor Playhead Loop | `editor_playhead_loop` | Bool | **false** | |
| Auto Scroll | `auto_scroll` | Bool | **true** | |

## Morph

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Morph Amount | `morph_amount` | Float | **0.0** | 0 – 1 |

## Debug

| Parameter | ID | Type | Default | Notes |
|-----------|-----|------|---------|-------|
| Debug Test Grain | `dbg_test_grain` | Bool | **false** | Internal test hook |

---

## Factory Presets (Curve Editor)

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

---

## Plugin Presets (Factory)

| Preset | Description |
|--------|-------------|
| Default | True factory defaults (all parameters at default) |
| Natural Light | Subtle correction, transparent mode |
| Modern Pop | Fast correction, bright formant |
| Ballad Slow | Slow retarget, vibrato preserved |
| Electronic | Heavy correction, formant shift |
| Live Vocal | Balanced for live use |
| Podcast Speech | Optimized for spoken voice |

*(Plugin presets store only APVTS parameter state, never the pitch curve.)*