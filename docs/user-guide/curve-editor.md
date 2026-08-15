# Curve Editor

The **Curve Editor** (Graphic mode) is a graphical editor that lets you **draw the ideal pitch curve** the audio should follow over time. Instead of snapping automatically to the scale, you control the target pitch note-by-note.

!!! tip
    Switch to Graphic mode using the **Mode** selector. The editor is time-synced to your DAW via ARA2 when available, and uses an internal transport otherwise.

---

## Editing the pitch curve

The curve is made of **points** positioned in time (horizontal axis) and pitch (vertical axis). Between two consecutive points the curve is interpolated linearly; before the first and after the last point it holds the endpoint value.

| Action | Result |
|--------|--------|
| **Drag a point** | Move it (vertically to change pitch, horizontally to change timing). |
| **Double-click** empty space | Add a new point at the cursor position. |
| **Right-click a point** (or **Alt+click**) | Delete that point. |
| **Right-click empty space** | Open the preset menu. |

The **pitch axis** is vertical — low notes at the **bottom**, high notes at the **top** — with a piano keyboard on the left.

### Snap to scale

If **Snap to Scale** is enabled, points are rounded to the nearest note of the current **Key/Scale**. This is the easiest way to keep your drawn curve musically in tune. The allowed notes are highlighted in **yellow** on the piano keyboard.

### Snap to grid

Snap points to the time grid so curves line up with bars/beats, which is especially useful for rhythmic vocals.

### Copy / paste & undo / redo

- **Copy / paste** — duplicate portions of a curve quickly.
- **Undo / redo** — revert or re-apply editing changes (global undo/redo also covers the automatable parameters).

---

## Built-in curve presets

Right-click in empty space to load a factory curve preset as a starting point:

| Preset | Description |
|--------|-------------|
| **default** | Flat curve (minimal correction). |
| **spoken** | Slight oscillation around 200 Hz — spoken-voice character. |
| **lyric** | Large expressive variations (A3..A4) — lyrical singing. |
| **rap** | Rising and falling ascents (~200–250 Hz). |
| **robot** | Flat placeholder for an extreme "T-Pain" effect (pair it with a very low **Speed**). |

Additional curve presets tuned by voice register (bass, baritone, tenor, alto, mezzo, soprano) are also available.

---

## Importing a pitch curve from MIDI

You can build a curve from an existing MIDI file instead of drawing it by hand:

1. Make sure you are on the **Curve Editor** tab.
2. Drag a `.mid` / `.midi` file from the OS file explorer onto the plugin window,
   or choose **"Import MIDI..."** in the Curve Editor hamburger menu (opens a file
   chooser filtered on `*.mid`).
3. If the file contains several active channels, a dialog asks you to pick the
   target **channel** and an **extraction strategy**:
   - **Highest note** — the lead melody (top note of each chord).
   - **Lowest note** — the bass line.
   - **Loudest note** — by note velocity (emphasis).
   - **Specific MIDI channel** — only one channel.
   The percussion channel (10) is always excluded.
4. The notes are converted into a curve with **step mode automatically enabled**
   (discrete notes, so there is no unwanted glissando between them), and the view
   **auto-fits** the imported duration. The import is **undoable** (Ctrl+Z) and
   overwrites the current curve.

Once imported, the curve behaves like any hand-drawn one — you can snap it to the
scale, move points, apply presets, etc. See also [Quickstart > Section 7: MIDI](../user-guide/quickstart.md#7-midi).

---

## Piano keyboard & note highlighting

A vertical **piano keyboard** runs along the left edge of the editor:

- White keys (C, D, E, F, G, A, B) and black keys (C#, D#, F#, G#, A#).
- **Scale notes are highlighted in yellow**, so you immediately see which notes the current scale allows.
- Octave labels (C2, C3, ...) are shown on the left of the C keys.
- The default range (C2–C7) is sufficient for voice.

---

## Waveform overlay

You can overlay the audio waveform behind the curve to guide your editing. Three display types are available:

| Type | Description |
|------|-------------|
| **Line** | Standard line waveform. |
| **Mirror** | Symmetric/mirrored display of the waveform. |
| **Spectral** | Spectral representation of the audio. |

The overlay is toggled with **Show Waveform**, and the display type is selected with **Waveform Display Type**.

---

## Measures ruler & ARA2

The editor includes a **measures ruler** with time-signature awareness:

- **ARA2 time-signature support** — time signatures are read directly from your DAW project, including mid-project changes.
- **Auto-scroll** follows the playhead as your track plays.
- The number of visible measures is adjustable (**Editor Measures**, 1–32).

!!! note
    ARA2 integration also lets the plugin read key, scale, and tempo directly from your arrangement, so you generally don't need to set them by hand.

---

## Harmony traces

If the **Harmony Engine** is enabled, the generated harmony voices are shown as **overlay traces** in the editor, so you can see how the harmony relates to your corrected lead curve. This makes it easy to check that the harmony notes sit where you expect before rendering. See [Harmony](../user-guide/harmony.md).

---

## Export as PNG

You can export a high-resolution snapshot of the editor:

- **Export as PNG (2x)** — renders the curve editor at 2× resolution for a clean, print-ready image (great for sharing or documentation).

---

## Editor layout summary

| Element | Location | Purpose |
|---------|----------|---------|
| Pitch curve | Main canvas | Drawn correction target |
| Piano keyboard | Left edge | Note reference + scale highlighting |
| Waveform overlay | Behind curve | Visual timing reference |
| Measures ruler | Top | Time-signature-aware timeline |
| Harmony traces | Main canvas | Preview of generated harmonies |

---

## Related

- [Correction Modes](../user-guide/correction-modes.md) — Auto vs. Graphic explained.
- [Quickstart](../user-guide/quickstart.md) — get started in minutes.
- [Harmony](../user-guide/harmony.md) — add harmony voices over your curve.
- [Default Parameters](../default-parameters.md) — editor-related parameters and defaults.
