# Quickstart

Welcome to **OpenVoxTuner**, a real-time pitch correction and harmony generation plugin for vocals. This page gets you up and running in a few minutes. OpenVoxTuner is an audio *effect*: you insert it on a vocal track, choose a key and scale, and it shifts the performance onto the nearest notes of that scale.

!!! tip
    New to the plugin? Start here, then read [Correction Modes](../user-guide/correction-modes.md) to understand Auto vs. Graphic mode.

---

## 1. Insert the plugin

OpenVoxTuner ships as **VST3**, **AU** (macOS), and a **Standalone** application.

1. In your DAW, add a plugin instance to your vocal channel (as an insert/effect, not a MIDI instrument).
2. If you are not in a DAW, launch the **Standalone** app — it includes an internal transport (120 BPM) so you can test without a host.
3. Play some audio into it. The **Pitch Visualizer** shows the sung note, the target note, and a cents tuning meter in real time.

!!! note
    OpenVoxTuner is a pitch-correction tool for **voice** — for best results use a clean, monophonic vocal signal with minimal background noise.

---

## 2. Set a key and scale

The plugin corrects pitch toward the notes of a musical scale.

1. **Key** — choose the tonic (0 = C, up to 11 = B).
2. **Scale** — pick from 14 scale types: Chromatic, Major, Melodic Minor, Harmonic Minor, Natural Minor, Major Pentatonic, Minor Pentatonic, Blues, Dorian, Phrygian, Lydian, Mixolydian, Locrian, and **Custom** (a 12-button editor where you enable individual notes).
3. Optional: turn on **Key/Scale Detection** and choose a **Key Source** so the plugin derives the key automatically (see below).

The notes that are "allowed" by the current scale are highlighted in **yellow** on the vertical piano keyboard on the left of the editor.

### Key detection sources

| Source | Description |
|--------|-------------|
| **Auto** | Real-time key detection from the audio input using Krumhansl–Schmuckler profiles. |
| **OpenVoxKey** | A companion bridge that shares key/scale data via shared-memory IPC. |
| **Sidechain** | Analyzes the accompaniment arriving on a dedicated sidechain bus. |

---

## 3. Tune the core parameters

The most important controls are:

| Control | Range | What it does |
|---------|-------|--------------|
| **Correction Mode** | Modern / Transparent | Character of the correction: tight & aggressive vs. gentle. |
| **Speed** | 0 – 200 ms | How fast the pitch glides to the target note. |
| **Amount** | 0 – 100% | Dry/wet blend — 100% = fully corrected, 0% = unprocessed. |
| **Humanize** | 0 – 50 cents | Adds subtle random variation for a more natural sound. |
| **Vibrato Preserve** | 0 – 100% | Retains the singer's natural vibrato through correction. |
| **Voice Type** | Universal, Bass, Baritone, Tenor, Alto, Soprano | Constrains the pitch-detection range to the selected voice. |
| **Latency Mode** | Direct Monitoring, Low Latency, Quality, Safe | Latency/quality trade-off (see table below). |

### Speed cheat-sheet

| Speed | Result |
|-------|--------|
| **0 ms** | Instant correction — the classic "T-Pain" robotic effect. |
| **~50 ms** | Fast but smooth — the Antares default feel. |
| **200 ms** | Slow, very natural correction that barely moves the pitch. |

### Latency modes

| Mode | Latency |
|------|---------|
| **Direct Monitoring** | 10 ms |
| **Low Latency** (default) | 12 ms |
| **Quality** | 20 ms |
| **Safe** | 30 ms |

!!! tip
    For live singing, use **Direct Monitoring** or **Low Latency**. For finished mixes, **Quality** or **Safe** give the processor more room and can sound better.

---

## 4. Choose Auto or Graphic mode

OpenVoxTuner has two ways of working:

- **Auto (Live)** — scale-aware quantization in real time. Great for live performance and fast results.
- **Graphic (Curve Editor)** — draw the exact pitch curve the audio should follow. Great for offline, note-by-note perfection (Melodyne-style).

You switch between them with the **Mode** selector. See [Correction Modes](../user-guide/correction-modes.md) and [Curve Editor](../user-guide/curve-editor.md).

---

## 5. Add harmony (optional)

Enable the **Harmony Engine** to add one or more backing voices generated from your lead vocal:

1. Turn on **Harmony Enable**.
2. Choose a **Harmony Type** (3rd/4th/5th/octave intervals, Vocal Stack, Power Chord, Drone, Unison, and more).
3. Pick **Use Voice** (pitch-shifted live vocal) or **Synth** (synthesized tones such as Choir or Organ).

Learn more in [Harmony](../user-guide/harmony.md).

---

## 6. Try the included presets

OpenVoxTuner ships with factory plugin presets you can load as a starting point, including:

- **Natural Light** — subtle correction, transparent mode.
- **Modern Pop** — fast correction, bright formant.
- **Ballad Slow** — slow retarget, vibrato preserved.
- **Live Vocal** — balanced for live use.
- **Podcast Speech** — optimized for spoken voice.

There is also an A/B + morph system so you can compare two states and continuously interpolate between them.

---

## 7. MIDI

OpenVoxTuner works with MIDI in three ways: **following a MIDI note** (pitch target),
**sending MIDI notes out**, and **importing a MIDI file** as a pitch curve.

### 7.1 Tuning follows MIDI IN (MIDI target)

The **"Tuning follows MIDI IN"** item in the hamburger menu (bottom-left) makes an
incoming *held* MIDI note drive the correction target: as long as you hold a note,
the voice is tuned to that note's frequency instead of the nearest scale note.
This is useful to force a specific harmony or to retune a voice live from a
keyboard/piano-roll.

!!! note
    While a MIDI note is actively driving the correction, a pulsing amber
    **"FOLLOWS MIDI IN"** badge appears at the **top-right of the plot** in both the
    **visualizer** and the **Curve Editor**, and the Curve Editor also shows a
    **dashed amber line** at the pitch of the held MIDI note — so you can see at a
    glance which target is being applied. (In the visualizer, the green output line
    already reflects it.)

**How to get MIDI into the plugin (DAW routing).** OpenVoxTuner is an audio *effect*
(`acceptsMidi()`), so it has no MIDI input on a track — MIDI must be routed to its
channel. In every DAW the principle is the same: put OpenVoxTuner on your vocal
audio track, then send a MIDI/Instrument track to that channel.

| DAW | How to route MIDI to the plugin |
|-----|--------------------------------|
| **Studio One / Fender Studio Pro** | Put OpenVoxTuner on the vocal *Audio* track. Create an *Instrument* (or MIDI) track with your notes or set your keyboard as its Input, then in that track's **Output** section route it to the channel hosting OpenVoxTuner. The channel appears in the MIDI output list once the plugin has been scanned. |
| **Reaper** | Simplest. Place OpenVoxTuner on the vocal track and put your MIDI on that track (MIDI item or a second track routed via the I/O matrix "MIDI → track"). MIDI flows automatically to the FX chain. |
| **Cubase / Nuendo** | Insert OpenVoxTuner on the *Audio* track (VST3). Create a *MIDI* track whose **Output** points to the Audio track and selects OpenVoxTuner. |
| **Ableton Live** | Insert OpenVoxTuner on the vocal *Audio* track. Create a *MIDI* track whose **MIDI To** points to the Audio track and selects OpenVoxTuner. |
| **FL Studio / Bitwig / Pro Tools** | Route the MIDI/Instrument track's output to the channel carrying OpenVoxTuner (supported via track output routing). |

!!! note
    The MIDI input bus is always declared by the plugin so hosts (especially Studio
    One / Fender Studio Pro) can persist the routing between sessions. In those DAWs
    the channel becomes a valid MIDI output destination as soon as OpenVoxTuner has
    been scanned.

### 7.2 MIDI OUT

The **"MIDI Out"** toggle in the hamburger menu makes the plugin *emit* MIDI note
events so you can trigger a virtual instrument, arpeggiator, or hardware synth from
the detected and corrected vocals.

- The **corrected lead** note is sent on **MIDI channel 1** (velocity 127).
- **Harmony** voices are sent on **channels 2–9** (velocity 100), one per active
  harmony voice (up to 8).
- Emitted notes are **scale-locked**: they are always clean scale notes, independent
  of the "Follow Lead" toggle. Note-offs are sent whenever the detected pitch
  changes or when MIDI Out is disabled.

In Studio One / Fender Studio Pro, enable MIDI Out, then route the channel's **MIDI
Output** to another instrument track or to a hardware/soft synth to hear it.

### 7.3 Import a MIDI file as a pitch curve (Curve Editor)

You can turn a MIDI file into an editable pitch curve in the **Curve Editor**:

1. Switch to the **Curve Editor** tab.
2. Drag a `.mid` / `.midi` file from the OS file explorer onto the plugin window,
   or choose **"Import MIDI..."** in the Curve Editor hamburger menu.
3. If the file has several active channels, a dialog asks you to pick a channel and
   an extraction strategy (highest note / lowest note / loudest note / specific
   channel). The percussion channel (10) is always excluded.
4. The MIDI notes are converted into a **step-mode** curve (discrete notes, no
   glissando) and the view auto-fits the import. You can **undo** with Ctrl+Z.

Learn more in [Curve Editor](../user-guide/curve-editor.md).

---

## Troubleshooting

- **No correction heard** — make sure **Bypass** is off, **Amount** is not at 0%, and the pitch detector is receiving a clean signal (check the noise gate / input levels).
- **Robotic sound** — your **Speed** may be too low; raise it toward 50–200 ms for a more natural result.
- **Wrong notes** — check the **Key** and **Scale** settings, or enable key detection.
- **No harmony** — make sure **Harmony Enable** is on and a **Harmony Type** other than "None" is selected.

---

## Related

- [Correction Modes](../user-guide/correction-modes.md) — Auto vs. Graphic in depth.
- [Curve Editor](../user-guide/curve-editor.md) — drawing your own pitch curve, importing MIDI.
- [Harmony](../user-guide/harmony.md) — harmony voices and synths.
- [Default Parameters](../default-parameters.md) — every parameter, default, and range.
- [Section 7: MIDI](#7-midi) — tuning follows MIDI IN, MIDI OUT, MIDI import.
