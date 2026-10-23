# Default Parameters Reference — OpenVoxTuner v0.1.1+

> Kept in sync with `Source/PluginProcessor.cpp`. Last updated: 2026-07-29.

## Audio Processing

| Parameter | ID | Type | Default | Range |
|-----------|-----|------|---------|-------|
| Speed | `speed` | Float | **20.0** | 0 – 200 ms |
| Latency Mode | `latency_mode` | Choice | **0 (Fast)** | 0=Fast (10 ms), 1=legacy 12 ms (not exposed; migrated to Fast on load), 2=Standard (20 ms), 3=High quality (30 ms) |
| Amount | `amount` | Float | **1.0** (100%) | 0 – 1 (0–100%) |
| Bypass | `bypass` | Bool | **false** | false / true |
| Mode | `mode` | Choice | **0 (Live)** | 0=Live, 1=Curve Editor |
| Correction Mode | `correction_mode` | Bool | **true (Transparent)** | false=Modern, true=Transparent |
| Hard Tune | `hard_tune` | Bool | **false** | false / true — bypasses the 80 ms anti-jitter speed floor, the pitch shifter's ratio-jump duck and the formant preserver, and switches pitch analysis to the short window (2048/512, 3-frame median); note attacks keep their click mask (the Hard position of the mode switch) |
| Pitch Detector | `pitch_detector` | Choice | **0 (YIN)** | 0=YIN, 1=Reserved |

The latency_mode values 0-3 select internal processing modes of 10 / 12 / 20 / 30 ms. The latency the plugin **declares** to the host is the measured effective delay of the correction engine for the selected Voice Type (see [the low-latency analysis](low-latency-shifter-analysis.md#13-what-to-declare-instead-pdc-replacement-values)); it is larger than the mode number and grows as the voice goes down.

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
| Lead Reverb | `lead_reverb_enable` | Bool | **false** | false / true |
| Lead Room | `lead_room_send` | Float | **0.15** | 0 – 1 |
| Lead Hall | `lead_hall_send` | Float | **0.45** | 0 – 1 |
| Upward Comp Enable | `upward_comp_enable` | Bool | **false** | false / true |
| Upward Comp Amount | `upward_comp_amount` | Float | **0.25** | 0 – 1 |
| Doubler Enable | `double_enable` | Bool | **false** | false / true |
| Double Amount | `double_amount` | Float | **0.0** | 0 – 100 % |
| De-Esser Enable | `deess_enable` | Bool | **false** | false / true |
| De-Esser Intensity | `deess_amount` | Float | **0.65** (65%) | 0 – 1 (0–100%) |
| Harmony Reverb | `harmony_reverb_enable` | Bool | **false** | false / true |
| Harmony Room | `harmony_room_send` | Float | **0.10** | 0 – 1 |
| Harmony Hall | `harmony_hall_send` | Float | **0.30** | 0 – 1 |
| Reverb Ducking | `reverb_duck_depth` | Float | **0.35** | 0 – 1 |
| Output Trim | `output_trim` | Float | **0.0 dB** | -12 – +12 dB |
| Limiter Enable | `limiter_enable` | Bool | **true** | false / true |
| Saturation Enable | `saturation_enable` | Bool | **false** | false / true |
| Saturation Drive | `saturation_drive` | Float | **0.35** (35%) | 0 – 1 (0–100%) |
| Saturation Warmth | `saturation_tone` | Float | **0.40** (40%) | 0 – 1 (0–100%) |

The effects are applied at three tap points, each with its own parameters: the lead
voice (`deess_*`, `lead_reverb_*`), the harmony bus (`harmony_reverb_*`) and the master
output (`saturation_*`, `output_trim`, `limiter_enable`).

The reverb is a **bus pair**, the DAW model: two shared rooms (a short **ROOM** at
RT60 ≈ 1.18 s and a long **HALL** at RT60 ≈ 2.43 s, the legacy Main room) and four
amounts that send each voice into them. A send adds to the room and never attenuates
the dry voice, so raising an amount can never make the singer quieter. `lead_room_send`
/ `lead_hall_send` place the corrected lead, `harmony_room_send` / `harmony_hall_send`
place the harmony stack; `lead_reverb_enable` / `harmony_reverb_enable` switch a voice's
two amounts together without losing their values, so switching a voice off and back on
needs no re-dial. Both switches start **off** (a fresh session is dry), while the
amounts already carry the "ready to sing" pair above, so the first switch-on applies a
balanced short room + hall behind the voice. `reverb_duck_depth` is one sidechain
ducker on both returns, keyed by the corrected lead (**not** the noise gate): the room
steps back while the voice sings and blooms back between phrases, up to 12 dB at 100 %.

On screen the two voice switches read **Lead** and **Harmony**, the four amounts read
**Room** / **Hall** under their voice, and the ducker reads **Ducking**: the short names
fit one cell column in every locale, measured at 14 pt. The host parameter names keep
the voice names, because a mixer automating a plugin needs to see which voice a control
acts on, never a timbre word such as *pad*.

The limiter is **on by default** and caps the **true peak** at **-1 dBFS**: it detects the
inter-sample crest through a 65-tap, 4x oversampling reconstruction filter, so it looks ahead by its group
delay (8 samples, about 0.17 ms) and that latency IS declared to the host on top of the pitch shifter's.
The gain closes slightly below the ceiling (-1.22 dBFS target) so the soft clip's own ringing stays under
-1 dBFS; measured true peaks are -1.15 dBFS on a full-scale square and -1.03 dBFS on a noise burst.
`output_trim` scales the final result only, after
every effect, and works with the limiter off (the Output Trim knob is not greyed out by
the limiter switch).

The De-Esser has one knob. Intensity maps the trigger level (`-38 x amount` dB) and the
depth cap (`19 x amount` dB) together, so 0% is inert and 100% is the deepest. The band
centre is not a parameter: it follows `voice_type` (Universal 7000 Hz, Bass 5200 Hz,
Baritone 6000 Hz, Tenor 6800 Hz, Alto 7600 Hz, Soprano 8400 Hz), with the band edges at
70% and 130% of that centre.

Saturation has two knobs and sits on the master output, **before** the shared reverb return, so
the room you add is never saturated. Drive sets `k = 1 + 9 x drive` and the shaper
`y = tanh(k x x) / tanh(k)`; that form keeps unity gain for quiet signals at every setting, and a
measured makeup curve (baked in the header, taken through the whole stage on a -18 dBFS sine)
keeps the loudness within 1 dB of the bypassed level. Drive **0** is not a shaper value: it is a
transparent unity bypass that touches nothing. Warmth maps the low-pass corner from 8000 Hz (0%)
to 5000 Hz (100%), rolling off the top harmonics the clipper adds. The stage runs at 2x
oversampling so the added harmonics stay under Nyquist, and it declares **no** plugin latency; a
subtractive high-pass at ~20 Hz after the shaper keeps the output DC-free, and inputs above full
scale (an ARA buffer can exceed ±1) are compressed into the curve before shaping.

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

The factory plugin presets are defined in `Source/PluginEditor.cpp`
(`getFactoryPluginPresetNames()` / `getFactoryPluginPresetOverrides()`) and
catalogued, with their exact overrides, in
[docs/reference/presets.md](reference/presets.md#factory-plugin-presets). That
page is the single source of truth: the list changes with features, so it is not
duplicated here.

*(Plugin presets store only APVTS parameter state, never the pitch curve.)*