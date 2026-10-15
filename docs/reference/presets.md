# Presets

OpenVoxTuner has two distinct preset systems, both stored/serialized via the
JUCE `AudioProcessorValueTreeState` (APVTS):

1. **Curve presets** (factory + custom) — these drive the `ovtdsp::PitchCurve`
   in the Curve Editor.
2. **Plugin presets** (factory + custom) — these store only APVTS parameter
   state, never the pitch curve.

## Factory curve presets

The factory curve preset catalog is defined in `ovtdsp::FactoryPresets.h` /
`FactoryPresets.cpp` and returned by `getFactoryPresets()`. Each entry is a
`FactoryPresetInfo { id, displayName, category, description }`.

The list below shows the **real `id` values** as used by
`PitchCurve::loadPreset(id)`:

| id             | Display name      | Category         | Description                                   |
|----------------|-------------------|------------------|-----------------------------------------------|
| `default`      | Default           | Basic            | Flat A3 reference tone.                       |
| `robot_c3`     | Robot (C3)        | Robotic          | Monotone robotic voice at C3.                 |
| `robot_c4`     | Robot (C4)        | Robotic          | Monotone robotic voice at C4.                 |
| `spoken_male`  | Spoken (Male)     | Vocal Character  | Natural spoken-male contour.                  |
| `spoken_female`| Spoken (Female)   | Vocal Character  | Natural spoken-female contour.                |
| `bass`         | Bass              | Voice Type       | Low male bass melody.                         |
| `baritone`     | Baritone          | Voice Type       | Baritone melody.                              |
| `tenor`        | Tenor             | Voice Type       | Tenor melody.                                 |
| `alto`         | Alto              | Voice Type       | Alto melody.                                  |
| `mezzo`        | Mezzo             | Voice Type       | Mezzo-soprano melody.                         |
| `soprano`      | Soprano           | Voice Type       | Soprano melody.                               |

!!! note "Legacy curve presets"
    Earlier documentation described five `PitchCurve::loadPreset` names —
    `default`, `spoken`, `lyric`, `rap`, `robot`. The current factory catalog
    has evolved into the gallery above (robot → `robot_c3`/`robot_c4`,
    spoken → `spoken_male`/`spoken_female`, plus per-voice-type melodic
    presets). The `loadPreset()` API still accepts preset keys for the
    gallery entries.

## Factory plugin presets

These are built-in full-parameter presets (APVTS state only). The names below
are the exact strings in `getFactoryPluginPresetNames()` (`PluginEditor.cpp`),
in menu order; "Default" carries no overrides and applies the factory defaults
verbatim.

| Preset | Description | De-Esser |
|--------|-------------|----------|
| Default | True factory defaults (all parameters at default). | off |
| Pure Correction | Pitch correction only, no dynamics on the lead. | off |
| Transparent / Doubling | Light correction + stereo doubler, no dynamics. | off |
| Studio Stack | 4-voice unison/octave stack with formant preservation. | on, 55% |
| Live Scene | Fast correction, direct monitoring, no effects. | off |
| Spoken Voice / Podcast | Gentle correction, gate, strong upward compression. | on, 65% |
| Creative Robot | Hard tune with a strong formant shift. | off |
| Broadcast Voice | Firm gate + 0.8 upward lift for streaming/radio loudness. | on, 80% |
| Breathy Vocals | Airy lo-fi voice: no gate, gentle upward lift, light reverb. | off |
| Lead Vocal | Thickened lead: gate, −3/+3 harmony, reverb, upward compression. | on, 65% |

The presets that add top-end energy (upward compression, stacked harmony
voices, reverb) switch the De-Esser on; the presets that leave the lead
untouched keep it off so the sibilance stays exactly as recorded. Intensity is
the only De-Esser value a preset sets — the band centre comes from the user's
Voice Type, which no factory preset touches because it also constrains pitch
detection. The De-Esser sits **before** the reverb in the effects chain, so a
preset de-esses the corrected lead voice and never the room tail; the measured
placement table is in section 8v of `docs/implementation-roadmap.md`.

*(Plugin presets store only APVTS parameter state, never the pitch curve.)*

## Curve presets gallery

The Curve Editor exposes a **browsable gallery** (a modeless window of factory
+ custom presets with curve thumbnails / metadata), opened from the
`presetGalleryButton`. `applyFactoryPreset(name)` resets the morph, loads the
curve into the editor, commits it to the active A/B slot, and aligns the morph
slider. A contextual menu (right-click in empty space, or the hamburger
Options menu) also lists the presets.

## Serialization & storage

- **APVTS**: all parameters (key, scale, `custom0`…`custom11`, speed, amount,
  formant, harmony, reverb, noise gate, UI theme/language, etc.) are managed by
  the plugin's `AudioProcessorValueTreeState`. This synchronizes host ↔ GUI ↔
  DSP and is serialized through `getStateInformation()` /
  `setStateInformation()`.
- **Pitch curve**: serialized as an XML sub-element `<PITCH_CURVE>` in
  `getStateInformation()`, via `PitchCurve::toXml()` / `fromXml()`.
- **Custom presets**: user-saved plugin presets are written to files
  (`parameters.state` only), while custom curve presets are saved/loaded
  separately and re-applied to the editor.

## Custom scale state

When `Scale::Custom` (index 13) is active, the scale is defined by 12
`AudioParameterBool` (`custom0`…`custom11`, one per semitone 0=C … 11=B). The
default is **C major** (C, D, E, F, G, A, B on; C#, D#, F#, G#, A# off). These
12 booleans are individually automatable by the host and are edited via the
`ScaleKeyboardComponent`. The `ScaleQuantizer` consumes them through
`setCustomIntervals()` (without key offset), and the Curve Editor's interactive
snap uses the same custom notes.

## Related parameter reference

See `docs/default-parameters.md` for the full table of parameters, including
defaults, ranges, and the deprecated `flex_tune` / `attack_aware` /
`attack_release` controls (kept for preset compatibility, DSP disabled since
2026-07-24).
