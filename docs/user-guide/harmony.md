# Harmony Engine

The **Harmony Engine** generates additional backing voices from your lead vocal. It turns a single vocal into a layered, chordal performance in real time.

---

## Enabling harmony

1. Turn on **Harmony Enable** (off by default).
2. Choose a **Harmony Type** other than **None**.
3. Set the overall **Harmony Volume** and **Harmony Blend**.
4. Choose how voices are generated: **Use Voice** or **Synth**.

!!! tip
    Harmony is applied *on top of* your pitch correction, so the voices follow the corrected lead pitch. Enable the harmony and listen to how it sits with your lead before adjusting the mix.

---

## Harmony types

OpenVoxTuner provides **22 harmony types** (None plus 21 harmony options):

- **Intervals** — classic harmony intervals, including 3rd below/above, 4th, 5th, and octave.
- **Vocal Stack** — a layered "doubled" stack of voices.
- **Power Chord** — a fifth-based power chord for rockier textures.
- **Drone** — a sustained drone note.
- **Unison** — thickening via closely spaced voices.
- And more.

Each type defines which intervals and voice configuration are generated relative to the lead.

| Harmony Type | Default |
|--------------|---------|
| 22 options (None + 21 harmony types) | **3rd Below + Above** |

---

## Use Voice mode

**Use Voice** (default: on) pitch-shifts your live vocal into the harmony voices:

- Generates **1–4 shifted voices** (default 4) from your vocal.
- Voices are **formant-preserved** so they sound like real singers rather than "chipmunk" artifacts.
- Voices are spread across the stereo field with **stereo panning**.

### Harmony Formant Shift

Controls the vocal-tract character of the harmony voices from **-5 to +5 semitones** (`harmony_formant`). Shifting this up can make the harmony sound brighter/smaller; shifting it down makes it darker/larger — useful for emulating different singers.

### Voice Type awareness

The harmony engine is **voice-type aware**: it respects the selected **Voice Type** (Universal, Bass, Baritone, Tenor, Alto, Soprano) so generated voices stay in a plausible register for the chosen voice.

---

## Synth mode

When **Use Voice** is off, harmony voices are **synthesized** rather than derived from your vocal:

- **Harmony Tone** — choose a tone color such as **Choir** or **Organ**.
- **Harmony Tone Color** — blend/adjust the tone color (0–1, default 0.5) to taste.

This mode is great when you want a smooth, pad-like backing that doesn't need to match the lead's timbre exactly.

!!! note
    In Synth mode the voices do not follow the lead's formant/timbre — they use the selected synthesized tone instead.

---

## Harmony controls reference

| Control | Range | Default | Description |
|---------|-------|---------|-------------|
| **Harmony Enable** | off / on | off | Master enable for the engine. |
| **Harmony Type** | None + 21 types | 3rd Below + Above | Which harmony to generate. |
| **Harmony Gain** | 0 – 1 | 0.75 | Overall harmony volume. |
| **Harmony Blend** | 0 – 1 | 0.5 | Blend between lead and harmony. |
| **Harmony Attack** | 1 – 300 ms | 35 ms | Per-voice attack time. |
| **Use Voice** | off / on | on | Use (shifted) voice vs. synth for harmony. |
| **Shifted Voices** | 1 – 4 | 4 | Number of harmony voices. |
| **Harmony Tone** | Choir / Organ | Choir | Synthesized tone color (Synth mode). |
| **Harmony Tone Color** | 0 – 1 | 0.5 | Tone color adjustment (Synth mode). |
| **Harmony Formant Shift** | -5 – +5 st | 0 | Vocal-tract character of harmony. |
| **Harmony Follow Lead** | off / on | on | Harmony follows the lead pitch. |
| **Harmony Gain Match** | off / on | on | Auto RMS level matching of harmony voices. |

!!! tip
    **Gain Match** automatically balances the harmony level against the lead, so you don't have to chase volume levels manually. **Follow Lead** keeps the harmony glued to the lead's pitch/timing.

---

## Visualizing harmony

When harmony is enabled, the generated voices are shown as **traces** in the [Curve Editor](../user-guide/curve-editor.md), letting you see exactly where each harmony voice lands relative to your corrected lead curve.

---

## Quick recipes

- **Thick pop chorus** — **Vocal Stack**, Use Voice on, 4 voices, blend ~0.4.
- **Rock power** — **Power Chord**, tight gain, low attack.
- **Ethereal pad** — **Synth** mode with **Choir** tone and a high tone color.
- **Classic 3rds** — **3rd Below + Above**, blend ~0.5, natural attack.

---

## Related

- [Correction Modes](../user-guide/correction-modes.md) — the correction that harmony builds on.
- [Curve Editor](../user-guide/curve-editor.md) — where you can see harmony traces.
- [Quickstart](../user-guide/quickstart.md) — get started in minutes.
- [Default Parameters](../default-parameters.md) — harmony parameter defaults.
