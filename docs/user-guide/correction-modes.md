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

### Correction Mode (Modern / Transparent)

| Setting | Character |
|---------|-----------|
| **Modern** | Tight, aggressive correction — notes are locked to the scale firmly. |
| **Transparent** | Gentle, subtle correction that preserves more of the natural performance. |

### Speed (0 – 200 ms)

Controls the retarget envelope — how quickly the pitch glides toward the target note.

| Speed | Result |
|-------|--------|
| **0 ms** | Instant correction — the classic "T-Pain" robotic effect. |
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

Trades latency against processing quality:

| Mode | Latency |
|------|---------|
| **Direct Monitoring** | 10 ms |
| **Low Latency** (default) | 12 ms |
| **Quality** | 20 ms |
| **Safe** | 30 ms |

---

## Quick workflow tips

- **Live**: use **Auto** mode, **Direct Monitoring** latency, and a speed around 30–60 ms.
- **Ballad / slow track**: raise **Speed** toward 150–200 ms and keep **Vibrato Preserve** high.
- **Pop / tight**: use **Modern** correction with a low **Speed**.
- **Background vocal layers**: lower **Amount** (60–80%) so they sit naturally behind the lead.

---

## Related

- [Quickstart](../user-guide/quickstart.md) — get started in minutes.
- [Curve Editor](../user-guide/curve-editor.md) — the Graphic mode editor in depth.
- [Harmony](../user-guide/harmony.md) — add harmony voices to your correction.
- [Default Parameters](../default-parameters.md) — all parameters, defaults, and ranges.
