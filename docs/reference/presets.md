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

| Preset | Description | De-Esser | Saturation | Reverb |
|--------|-------------|----------|------------|--------|
| Default | True factory defaults (all parameters at default). | off | off | off |
| Pure Correction | Pitch correction only, no dynamics on the lead. | off | off | off |
| Transparent / Doubling | Light correction + stereo doubler, no dynamics. | off | off | off |
| Studio Stack | 4-voice unison/octave stack with formant preservation. | on, 55% | on, drive 25%, warmth 60% | harmony: room 10% + hall 35% |
| Live Scene | Fast correction, direct monitoring, no effects. | off | off | off |
| Spoken Voice / Podcast | Gentle correction, gate, strong upward compression. | on, 65% | on, drive 25%, warmth 70% | off |
| Creative Robot | Hard tune (Modern, Speed 0) with a strong formant shift. | off | on, drive 45%, warmth 20% | off |
| Hard Tune (Cher / T-Pain) | Hard mode (bypasses the speed floor, the ratio-jump duck and formant preservation): Speed 0, 100 %, no vibrato, formant +2 st. | off | on, drive 40%, warmth 25% | off |
| Broadcast Voice | Firm gate + 0.8 upward lift for streaming/radio loudness. | on, 80% | on, drive 30%, warmth 60% | off |
| Breathy Vocals | Airy lo-fi voice: no gate, gentle upward lift, light reverb. | off | on, drive 15%, warmth 80% | lead: room 15% + hall 40%, duck 30% |
| Lead Vocal | Thickened lead: gate, −3/+3 harmony, reverb, upward compression. | on, 65% | on, drive 35%, warmth 50% | lead: room 15% + hall 45%, duck 35%; harmony: room 10% + hall 30% |

The Reverb column lists the two SHARED rooms the preset sends into (ROOM = short,
HALL = long) and the sidechain ducker depth. A preset that wants no room leaves both
voice switches off, which is the factory default; the two that promise space switch
the lead on and give the hall the larger share, because the hall is the legacy Main
room. Studio Stack keeps the lead dry and puts only the harmony stack in the room: a
4-voice stack is where a long room fattens without washing the lead.

The presets that add top-end energy (upward compression, stacked harmony
voices, reverb) switch the De-Esser on; the presets that leave the lead
untouched keep it off so the sibilance stays exactly as recorded. Intensity is
the only De-Esser value a preset sets — the band centre comes from the user's
Voice Type, which no factory preset touches because it also constrains pitch
detection. The De-Esser sits **before** the reverb in the effects chain, so a
preset de-esses the corrected lead voice and never the room tail; the measured
placement table is in section 8v of `docs/implementation-roadmap.md`.

The master Saturation stage follows the same rule: the presets that promise nothing added
(Pure Correction, Transparent / Doubling, Live Scene) name it **off** explicitly, so a live
monitor path never gains loudness from the plugin. The stack and on-air presets take a low
drive (25–30%) with warmth high enough to keep the added harmonics below the de-esser band.
The two hard-tune presets are the ones that want the edge — Creative Robot at 45% drive and
20% warmth, Hard Tune at 40% and 25% — and both leave the De-Esser off on purpose: the added
harmonics ARE the effect, so taming them would undo the preset. Drive and warmth are the only
saturation values a preset sets.

*(Plugin presets store only APVTS parameter state, never the pitch curve.)*

## Hard tune (Cher / T-Pain)

The **Hard Tune (Cher / T-Pain)** factory preset is the "Auto-Tune as an
instrument" sound: the voice is snapped to the scale grid instead of being
gently corrected. The **Hard** position of the correction-mode switch is the
engine half of it — it bypasses the 80 ms anti-jitter speed floor, the pitch
shifter's ratio-jump duck (the onset mask stays on, so note attacks do not
click) and the formant preserver. The other settings
remove everything that would soften or colour the correction:

| Setting | Value | Why |
|---------|-------|-----|
| Correction mode | **Hard** (switch) | The third position: Modern + the Hard Tune engine path. Transparent (the factory default) multiplies Amount by 0.8 and floors Speed at 30 ms on its own. |
| Speed | **0 ms** | The fastest retune the engine offers; with Hard Tune the floor no longer overrides it. |
| Amount | **100 %** | The full correction ratio. |
| Vibrato Preserve | **0 %** | No modulation survives: the note is corrected against the instantaneous pitch, not a smoothed centre. |
| Humanize | **0 cents** | No random fluctuation. |
| Formant | **+2 st** | The Cher "chipmunk" colour. Formant **off** (or +1 st) gives the T-Pain flavour: the same hard tune with a natural timbre. |
| Formant Strategy | **Precise** | The allpass path: shifts the tone without colouring it. |
| Saturation | on, drive 40 %, warmth 25 % | Optional edge; switch it off for the dry 1998 sound. |
| Scale | your choice | The preset preserves the Key & Scale selection. **Chromatic** is the rawest target set (every semitone is a target); the song's key and scale is the production setting. |
| Latency mode | your choice | Left alone so loading the preset never changes the session's monitored latency. Direct Monitoring is the live choice, Quality the studio one. |

The T-Pain flavour is the same recipe with Formant off (or +1 st) and a lower
Saturation drive (25–35 %, warmth 40–60 %). The preset ships the Cher flavour
because it is the more distinctive of the two; one switch gives the other.

**Production notes.** Feed the plugin a clean, dry, monophonic voice, first in
the chain; put EQ, compression and reverb after it. For the precise "played
instrument" effect, enable *Tuning follows MIDI IN* (Options menu) and hold the
target notes on a MIDI keyboard: the correction target becomes the held note
instead of the nearest scale note.

**Engine note.** Hard Tune removes the engine's own smoothing, so Speed 0 now
snaps instead of gliding. What still bounds the response is the pitch detector:
the YIN estimate averages about 93 ms of audio and refreshes every ~46 ms, so a
note change is locked within roughly one to two analysis hops. A sub-5 ms
Auto-Tune-style response would need a shorter analysis window — a separate engine
change, not a preset value.

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
