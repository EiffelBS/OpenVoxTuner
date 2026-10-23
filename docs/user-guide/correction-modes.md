# Correction Modes

OpenVoxTuner offers two distinct ways to correct pitch, selected with the **Mode** control:

| Mode | Target pitch source | Best for |
|------|---------------------|----------|
| **Auto (Live)** | Automatic quantization to the current Key/Scale | Live singing, fast results |
| **Graphic (Curve Editor)** | A pitch curve you draw with the mouse | Offline mixing, note-perfect results |

The rest of the processing pipeline (amount blend, retarget speed, formants, pitch shifting) is shared by both modes — only the *target pitch* comes from a different source.

---

## Auto mode (scale-aware quantization)

In **Auto** mode, the plugin detects the pitch of the incoming signal and snaps it to the nearest note of the selected **Key** and **Scale**. This is the familiar real-time "autotune" behavior.

1. Choose a **Key** (tonic 0–11) and a **Scale** from 14 types.
2. Optionally enable **Key/Scale Detection** so the plugin determines the key itself (Auto / OpenVoxKey / Sidechain).
3. Set **Speed** and **Amount** to shape how aggressively the pitch is pulled to the target.

Auto mode is fast and ideal for live performance, quick demos, or when you want the correction to follow a scale automatically.

---

## Graphic mode (curve editor)

In **Graphic** mode, you draw the exact pitch curve the audio should follow over time. The plugin follows your drawn curve rather than snapping to the scale automatically (though snap-to-scale is available while editing).

- Points are connected by linear interpolation.
- The curve is time-synced to your DAW transport (via ARA2 when available).
- Snap to scale, grid, copy/paste, undo/redo, presets, and PNG export are all supported.

See the [Curve Editor](../user-guide/curve-editor.md) page for the full editing workflow.

---

## The core correction controls

These controls shape *how* the correction is applied in both modes.

### Correction Mode (Modern / Transparent / Hard)

| Setting | Character |
|---------|-----------|
| **Modern** | Tight, aggressive correction — notes are locked to the scale firmly. |
| **Transparent** | Gentle, subtle correction that preserves more of the natural performance. Transparent also multiplies Amount by 0.8 and ignores Speed below 30 ms, so it can never deliver a hard tune. |
| **Hard** | The hard-tune engine path: bypasses the 80 ms anti-jitter speed floor, the pitch shifter's ratio-jump duck and the formant preservation, and analyses pitch with a shorter window (a note change is reported in ~56 ms instead of ~218 ms), so the correction snaps instead of gliding. Note attacks keep their click mask. Speed 0 gives the rawest result; use it for the Cher / T-Pain effect. |

### Speed (0 – 200 ms)

Controls the retarget envelope — how quickly the pitch glides toward the target note.

| Speed | Result |
|-------|--------|
| **0 ms** | Fastest correction — the classic "T-Pain" hard-tune character. Pair it with the **Hard** mode and zero vibrato (the **Hard Tune (Cher / T-Pain)** preset does all three). |
| **~50 ms** | Fast but smooth — the Antares default feel. |
| **200 ms** | Slow, very natural correction with almost no audible effect. |

!!! tip
    Speed is an exponential smoothing curve: after one time constant it reaches ~63% of the target, after three ~95%, and after five ~99%. In practice, don't think in exact numbers — trust your ears.

### Amount (0 – 100%)

The dry/wet blend. **100%** delivers fully corrected audio; **0%** is the unprocessed signal. Lower amounts are useful for natural "always-on" tuning.

### Humanize (0 – 50 cents)

Adds subtle random pitch fluctuation to avoid an overly mechanical, "perfect" sound. Higher values sound more human; lower values sound tighter.

### Vibrato Preserve (0 – 100%)

Retains the singer's natural vibrato through correction. At higher values the natural vibrato survives tuning; at lower values it is flattened out.

### Voice Type

Constrains the pitch-detection range to a specific voice so the detector locks onto the intended register:

| Voice Type | Frequency range |
|------------|-----------------|
| Universal | 30 – 1000 Hz |
| Bass | 82.41 – 329.63 Hz |
| Baritone | 110 – 440 Hz |
| Tenor | 130.81 – 523.25 Hz |
| Alto | 174.61 – 698.46 Hz |
| Soprano | 261.63 – 1046.50 Hz |

### Latency Mode

The four modes trade latency against processing quality. The value the plugin
declares to the host (what the DAW compensates) is not the internal mode number:
it is the **measured effective delay** at the lowest note of the selected Voice
Type (the grain has to look a fraction of a period ahead, so the delay grows as
the voice goes down). See the
[low-latency analysis](../low-latency-shifter-analysis.md#13-what-to-declare-instead-pdc-replacement-values)
for the measurements behind the table.

| Voice Type | Direct Monitoring | Low Latency (default) | Quality | Safe |
|---|---|---|---|---|
| Universal | ~31 ms | ~35 ms | ~38 ms | ~38 ms |
| Bass | 31 ms | 35 ms | 38 ms | 38 ms |
| Baritone | 26 ms | 27 ms | 28 ms | 39 ms |
| Tenor | 23 ms | 24 ms | 27 ms | 36 ms |
| Alto | 20 ms | 20 ms | 27 ms | 35 ms |
| Soprano | 14 ms | 14 ms | 21 ms | 30 ms |

The declared value is the worst case for the register, so the corrected voice is
never late; notes higher in the register land a few ms early. That is the price
of one PDC value for a delay that depends on the pitch. The creative **Formant**
shift grows the grain and can add a few ms on top; it is not part of the table.

The two shortest modes declare within 1–2 ms of each other: pick them for their
processing character (grain size), not for a large latency difference.

---

## Quick workflow tips

- **Live**: use **Auto** mode, **Direct Monitoring** latency, and a speed around 30–60 ms.
- **Ballad / slow track**: raise **Speed** toward 150–200 ms and keep **Vibrato Preserve** high.
- **Pop / tight**: use **Modern** correction with a low **Speed**.
- **Hard tune (Cher / T-Pain)**: load the **Hard Tune (Cher / T-Pain)** factory preset — the switch reads **Hard**, Speed 0, Amount 100 %, Vibrato 0, Humanize 0, Formant +2 st. Keep the voice dry and put reverb after the plugin. Chromatic scale gives the rawest effect; the song's key and scale is the production setting. For the T-Pain flavour, switch Formant off.
- **Background vocal layers**: lower **Amount** (60–80%) so they sit naturally behind the lead.
- **Sibilant takes**: when you push **Upward Comp** (the quiet "s" gets lifted to the same level as the vowels), switch the **De-Esser** on with it — Intensity around 65%, and Voice Type on the register you sing (a low voice is tamed around 5 kHz, a soprano around 8.4 kHz). It works on the corrected lead, before the reverb, so the room you add stays bright.
- **Master glue**: switch **Saturation** on with **Drive** around 20–35% and **Warmth** around 60%. It sits on the master output, ahead of the shared reverb return, so it thickens the whole voice and never the room tail. Drive 0 adds nothing at all — the stage is bypassed — and loudness stays matched at every setting, so judge it by tone, not by level: turn **Warmth** up if the added harmonics read as harsh on your monitors.

---

## Related

- [Quickstart](../user-guide/quickstart.md) — get started in minutes.
- [Curve Editor](../user-guide/curve-editor.md) — the Graphic mode editor in depth.
- [Harmony](../user-guide/harmony.md) — add harmony voices to your correction.
- [Default Parameters](../default-parameters.md) — all parameters, defaults, and ranges.
