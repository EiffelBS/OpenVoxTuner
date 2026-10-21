# Changelog — 2026-10-21

## Feature — Audio file import → editable pitch curve (drag & drop)

### Context

The Curve Editor could already be fed from outside the plugin: roadmap section 5b's MIDI import turns a
dropped `.mid` (or **Import MIDI...** in the Curve Editor menu) into a step-mode curve. What it could not
accept was a **recording** — the most natural starting point for a vocal take, because the sung line already
exists as audio and a score is an extra artefact to produce first. Until now the only routes into the editor
were drawing the curve by hand or transcribing the take to MIDI.

This feature closes that gap with the same interaction model: drop a file on the plugin window or pick it from
the Curve Editor menu, review what was detected in a dialog, then edit the resulting curve like any
hand-drawn one. It is the audio counterpart of `MidiImporter` — same static-only module pattern and the same
output convention (point times in seconds, step mode, undoable).

### What shipped

- **`Source/dsp/AudioCurveImporter.h` / `.cpp`** (new): `ovtdsp::AudioCurveImporter`, static-only.
  - `AudioImportInfo analyzeFile (const juce::File&, float minHz = 60, float maxHz = 1000)` decodes the file,
    runs the full detection and fills `numChannels`, `sampleRate`, `durationSec`, `numNotes`,
    `minDetectedHz`, `maxDetectedHz`, `isValid` and `errorMessage`.
  - `PitchCurve importFrom (const juce::File&, bool snapToScale, const juce::Array<int>& scaleIntervals,
    float minHz = 60, float maxHz = 1000)` returns the ready-to-edit curve, or an empty one for an
    unreadable, too long or pitch-less file.
  - Tuning constants are public so tests and the UI share one source of truth: `kMaxDurationSec = 600`,
    `kWindowSec = 0.046`, `kHopSec = 0.010`, `kMinNoteSec = 0.080`, `kMaxGapSec = 0.080`,
    `kNoteToleranceCents = 50`, `kDefaultMinHz = 60`, `kDefaultMaxHz = 1000`.
- **`Source/PluginEditor.cpp` / `.h`**: `isInterestedInFileDrag` accepts `.wav`, `.aiff`, `.aif`, `.flac` and
  `.ogg` (case-insensitive); `filesDropped` keeps the MIDI branch first and falls back to the first audio file
  in the drop; `handleAudioFileImport` applies the same Curve-Editor-tab gate as the MIDI path, runs
  `analyzeFile` and opens a confirmation dialog; `applyAudioImport` refuses a curve with fewer than 2 points;
  `audioImportFileChooser` backs a new **Import Audio...** menu entry placed right after **Import MIDI...** and
  filtered on the five extensions. A new `AudioImportDialogComponent` reuses the MIDI channel dialog's dark-card
  layout: one row per fact (duration as `%.2f s`, sample rate as `N Hz`, channel count, detected note count), the
  detected pitch span when notes exist, a **Snap to scale** toggle (default ON), the supported-formats hint
  ("WAV, AIFF, FLAC, OGG (MP3 is not supported)"), and a Cancel / Import pair with Import carrying the accent
  fill.
- **`Source/ui/PitchCurveEditor.h` / `.cpp`**: `importMidiCurve` renamed to `importCurve` (documented as "from
  an external source (MIDI or audio file)"). Both importers emit discrete notes, so the import forces step mode
  ON and records one undoable `CurveEditAction`. A `getScaleIntervals()` getter was added so the dialog can read
  the active scale at confirm time.
- **`Source/ui/OVTLanguages.h`**: 12 new keys — `menu.import_audio`, `audio.dialog_title`,
  `audio.error_wrong_tab`, `audio.error_no_notes`, `audio.error_too_few_notes`, `audio.duration`,
  `audio.sample_rate`, `audio.channels`, `audio.notes_detected`, `audio.snap_to_scale`, `audio.formats_hint`,
  `audio.btn_import` — translated in all six locales (EN / FR / DE / ES / JA / ZH).
- **`Source/dsp/YinPitchDetector.h` / `.cpp`**: `setOfflineMode(bool)` / `isOfflineMode()`. The default stays
  `false`, so the real-time detection path is unchanged.
- **`CMakeLists.txt`**: `AudioCurveImporter.cpp` added to the plugin target and, together with
  `YinPitchDetector.cpp`, to the test target; `juce_audio_formats` linked into `OpenVoxTunerTests`.
- **`test/Main.cpp`**: the new suite included, so the runner executes it with the rest.
- **`test/dsp/AudioCurveImporterTest.cpp`** (new): the suite described under **Testing**.

### Design facts

- **Decode → downmix → decimate.** `juce::AudioFormatManager` with `registerBasicFormats()` handles the four
  supported formats: **WAV, AIFF, FLAC and OGG-Vorbis**. **MP3 is not supported** — JUCE's MP3 decoder is
  disabled in this build — so the format is excluded from the drop target, the file-chooser filter and the
  dialog hint instead of failing silently after an import attempt.
- **Only the decimated mono track is kept.** The file is read in 8192-frame blocks at its native rate; channels
  are averaged to mono, then an integer boxcar decimation brings the analysis rate to about **12 kHz**
  (`round(sampleRate / 12000)`, clamped 1..16). That bounds YIN's cost while staying well above the Nyquist
  frequency of the vocal range, and it is what keeps a 10 min stereo 48 kHz take at ~29 MB (12 kHz mono, float)
  instead of the ~230 MB the native-rate buffer would need.
- **Frame track: 46 ms window / 10 ms hop** at the analysis rate, YIN clarity threshold **0.10** for offline
  analysis, default detection range **60 Hz – 1 kHz** clamped to 0.45 × the analysis rate. A note onset is
  therefore resolved to within one hop.
- **Segmentation rules.** A frame within **±50 cents** of the running median of the current note (last 9 frames)
  continues it; a silent frame closes it; consecutive segments within 50 cents separated by **80 ms or less**
  are merged (consonants, breaths, detector dropouts); notes shorter than **80 ms** are dropped as artefacts;
  each surviving note's pitch is the **median of all its frames**, which keeps vibrato and isolated outliers
  from dragging the note centre.
- **Note ends are estimated, not observed.** A note's last analysis window extends past its real end, so the end
  is taken as last voiced frame + half a window (**23 ms**), clamped to the file duration and then to the next
  note's onset: contiguous legato singing must not produce overlapping point pairs, which would read as a
  pitch spike.
- **Output convention.** Two points per note at its median pitch (start and end), times in **seconds**, exactly
  like `MidiImporter`. The DSP consumes beats; that pre-existing mismatch is intentionally not "fixed" here so
  both importers behave identically. Step mode is on. `addOrUpdatePoint` makes contiguous notes share the
  boundary point, so a legato line yields one point per boundary rather than two coincident ones.
- **Snap to scale.** With the toggle on, each note's median pitch is rounded through
  `PitchCurve::snapToIntervals` against the intervals read from the editor **at confirm time** — a Key/Scale
  change made while the dialog is open is honoured. An empty interval set means no snapping.
- **Limits and error text.** The **10-minute** cap is checked from the reader's length and sample rate before any
  block is read and reports "The audio file is longer than 10 minutes."; an unreadable file reports
  "Unsupported or unreadable audio file: <name>" and an empty one "The audio file is empty." A file with no
  voiced pitch is refused before the dialog opens ("No sung pitch was detected in this audio file."), and an
  import yielding fewer than 2 points is refused with its own message.
- **Offline mode, and why the live detector could not be reused as is.** The live YIN path carries two
  anti-octave heuristics that assume the live 4x-decimated window, in which YIN's first minimum can be the 2nd
  harmonic ("Case A" tests tau\*2 to recover the fundamental). Over the importer's full-band, non-decimated
  window the first minimum already *is* the fundamental, so those heuristics report every sustained tone
  exactly one octave low — verified on pure tones and on harmonic-rich, vibrato-modulated voice-like signals.
  `setOfflineMode(true)` disables them and also bypasses the internal 5-frame median filter: in batch mode the
  caller owns the frame track and applies its own per-note median, and the live median would smear note
  boundaries by two hops.
- **Message-thread work.** Both entry points are pure functions called from the message thread as one-shot work
  for an explicit user action, never from `processBlock()`; decoding costs roughly 1 s per 3 min of mono. The
  dialog's numbers come from `analyzeFile()` and Import re-runs the whole pipeline through `importFrom()`, so
  the file is decoded twice — the dialog stays stateless, and at that cost the second pass is acceptable for an
  explicit import.
- **i18n and user-facing text.** 12 keys across 6 locales; the Import button carries the accent fill
  (`setComponentID("primary")`) and Cancel stays standard; no visible string names a file path, class or
  decoder.

### Testing

New suite `test/dsp/AudioCurveImporterTest.cpp`, registered in `test/Main.cpp` and compiled into
`OpenVoxTunerTests`. It generates its material at runtime (`juce::WavAudioFormat` + `AudioFormatWriter`,
44.1 kHz / 16-bit except the duration group, which writes 8 kHz / 8-bit to keep the file small) in a scratch
folder next to the test executable — the same Low-integrity constraint `MidiImporterTest` works under. The
tones are vowel-like rather than pure sines (fundamental + 4–5 harmonics weighted 1/h, amplitude 0.4, plus a
0.01 noise floor) so YIN sees a voice-like spectrum.

Runner output for the suite, verbatim:

```
[ OK ] AudioCurveImporter (9 assertions)
[ OK ] AudioCurveImporter (18 assertions)
[ OK ] AudioCurveImporter (8 assertions)
[ OK ] AudioCurveImporter (7 assertions)
[ OK ] AudioCurveImporter (4 assertions)
[ OK ] AudioCurveImporter (5 assertions)
[ OK ] AudioCurveImporter (6 assertions)
[ OK ] AudioCurveImporter (9 assertions)
[ OK ] AudioCurveImporter (6 assertions)
[ OK ] AudioCurveImporter (6 assertions)
```

Suite total: **10 groups / 78 assertions / 0 failed**. Full `OpenVoxTunerTests` run:
`Result: 206 OK, 0 FAILED`. The generated WAVs are deleted by their RAII wrappers, so the scratch folder is
empty after the run.

The ten groups, in that order, cover:

- file metadata of a 44.1 kHz mono file (`isValid`, sample rate, channel count, duration within ±0.05 s, at
  least one note detected);
- a held four-note melody (A3 / C4 / E4 / G4, 0.5 s each, contiguous): four notes, pitch within 1 % at each note
  midpoint, boundaries within a few tens of milliseconds;
- note ends and silence: two separated notes produce two points per note and the first note's end stays inside
  ±40 ms of its true end;
- two seconds of silence (`isValid` with zero notes, empty curve) and two seconds of white noise (no ghost
  notes);
- short-note filtering: a 50 ms blip next to a 500 ms note yields a single detected note;
- snap on/off on a 250 Hz tone with C-major intervals (≈246.94 Hz snapped vs ≈250 Hz unsnapped);
- unreadable inputs (a missing path, garbage bytes under a `.wav` name): `isValid` false, non-empty error
  message, empty curve;
- stereo downmix: the same sine in both channels is detected correctly (`numChannels == 2`, pitch within 1 %);
- the 10-minute cap: a 601 s file is refused with the duration message, the check happening before analysis.

### Docs

- `docs/user-guide/curve-editor.md`: new **Importing a pitch curve from audio** section — drag & drop and menu
  entry, supported formats with the MP3 limitation, what the dialog shows, the **Snap to scale** option,
  2 points per note / step mode / undo, and the practical tip that a quiet single-voice recording gives the
  cleanest curve. The intro now names the three ways to build a curve.
- `docs/user-guide/quickstart.md`: section 7.3 covers MIDI and audio import, with a cross-reference to the new
  Curve Editor section.
- `docs/implementation-roadmap.md`: new section **5c. Curve Editor: Audio Import (Drag-and-Drop) — Implemented
  2026-10-21**, checklist in the style of 5b.
- `docs/testing.md`: the suite added to the `dsp/` tree, to the coverage sentence and to the temp-file
  paragraph.
