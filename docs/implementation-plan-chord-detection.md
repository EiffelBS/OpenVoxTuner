# Implementation plan — Standalone chord detection library (MIDI + audio)

Status: **in progress** — library `libs/ovtchord/` created. Plan for a
self-contained, plugin-independent chord detection library. Primary goal: build
the library as an autonomous module with **no modification** to the existing
OpenVoxTuner plugin or its UI at this stage.

> **Progress (2026-08-09)** :
> - **Phase 1 (MIDI)** : ✅ implemented and tested — MIDI 1.0 parsing, note
>   tracker, matching engine (19 templates), public API, JSON export.
> - **Phase 2 (audio)** : ✅ core implemented and tested — WAV 16/24-bit, bandpass +
>   gate + AGC, chroma via spectral peak extraction (internal FFT), wired to the
>   same `chord_engine`. **MP3 : ✅** (minimp3 vendored, `mp3_reader`).
> - **Phase 3 (API unification)** : ✅ partial — handle API + callbacks +
>   `process_midi` / `process_audio` / `process_audio_file`.
> - Tests : **18 tests, 65 checks, 0 failure** (`ctest` in `build-ovtchord/`).
> - The plugin `Source/` and `test/` are **not modified**.
>
> **Internal chroma benchmark (2026-08-10)** : `tools/benchmark_chords.cpp`
> (CMake target `ovtchord_benchmark`, option `OVTCHORD_BUILD_BENCHMARK`).
> Result on 11 reference chords (C, Cm, C7, Cmaj7, Cm7, Csus4, Csus2, C6,
> Cdim, Caug, C5) generated as sums of sines :
> - **Accuracy : 11/11 (100 %)**.
> - **CPU : ~2.1 ms/chord** (window 8192, hop 1024, incl. signal generation).
> - **Aubio decision : NOT adopted** — the internal chroma reaches 100 % on the
>   reference set with negligible CPU cost and **zero dependency**. The Aubio leg
>   is kept in the benchmark behind `#if defined(OVTCHORD_HAVE_AUBIO)` as a
>   template for a possible future comparison. Aubio is not installed on the dev
>   machine.
>
> **Chroma fixes surfaced by the benchmark (2026-08-10)** :
> - **Parabolic interpolation** of spectral peaks (sub-bin frequency) : at
>   4096 samples/44.1 kHz one FFT bin ≈ 10.8 Hz (~0.5 semitone at 155 Hz), which
>   classified Eb3 as D. Fixed in `chroma.cpp`.
> - **Harmonic summation** (fundamental + harmonics 1/2/3/4) : reinforces the
>   pitch class and tolerates missing fundamentals in real audio.
> - **Default window 4096 → 8192** (`audio_processor.h`) : resolution
>   ~5.4 Hz/bin, needed to separate close notes (e.g. F3/G3 a major second apart)
>   in the low register.
> - **`minNotes=2` in the benchmark** : allows 2-note power chords (C5) in the
>   reference set (the engine default stays 3).
>
> **Plugin integration scope (decision 2026-08-10, implemented)** — the
> integration is done in the plugin :
> - **MIDI** : MIDI chord detection (ovtchord) → `setChordOverride`, active only
>   when « Tuning follows MIDI IN » is enabled (the incoming MIDI is already
>   parsed). Latency < 10 ms (instant). Applies in **plugin AND standalone**.
> - **Audio** : audio chord detection (ovtchord) → `setChordOverride`, **only on
>   the sidechain bus** (polyphonic accompaniment), mirroring the existing
>   `sidechainKeyDetector` (`key_source` = "Sidechain"). The ~186 ms latency is
>   acceptable because it is **off the live voice path**. Applies **only in
>   plugin mode** (standalone does not route the sidechain bus).
> - **Excluded** : chord detection on the main input (monopitch voice) — useless
>   (monopitch) and unacceptable live (latency).
> - **UI** : a « Chord detection » toggle in the **wrench/Advanced** menu
>   (existing `advancedMenu` submenu) to enable/disable chord detection (MIDI
>   and/or sidechain).
> - **Source priority** (avoids conflicts on `setChordOverride`) :
>   **MIDI (« Tuning follows MIDI IN » active + MIDI signal present) > ARA
>   (chord track) > Sidechain > scale**. « Tuning follows MIDI IN » is an
>   explicit user choice for MIDI to control the tuning → top priority while a
>   MIDI signal is incoming. Without a MIDI signal, fall back to scale + ARA
>   chord (chord-aware tuning). MIDI processing is not gated by ARA in
>   `processBlock`, so « Tuning follows MIDI IN » also applies in ARA mode (if
>   the DAW routes MIDI).
>
> **Implementation (2026-08-10)** : `ScaleQuantizer` gains a « live » chord
> override (`setLiveChordOverride`/`clearLiveChordOverride`) that takes priority
> over the ARA windows. `PluginProcessor` creates two ovtchord contexts (MIDI +
> sidechain) and feeds them in `updateLiveChordOverride()` (called per block).
> Parameter `chord_detect_enable` + toggle in the wrench/Advanced menu (6
> languages). **Allocation-free real-time audio path** : pre-allocated scratch
> buffers (`workFrame`, `chromaVec`, `mag`) reused ; `extract`/
> `pitchClassSetFromChroma` write into caller-provided buffers (no more return by
> value).

---

## 0. Objective and scope

Create a self-contained C++ library (`ovtchord`) able to detect chords from two
sources :

- a **MIDI stream** (MIDI 1.0 standard),
- an **audio signal** (WAV/MP3 files or real-time stream).

The library is **independent** of the OpenVoxTuner plugin and of any UI : no
import of host-plugin modules, no display calls, no JUCE dependency. Future
integration into the plugin will go through a static public API, without
touching the existing architecture.

> **Scoping note** : the current plugin already uses a « chord-context override »
> mechanism (`ScaleQuantizer::setChordOverride`) fed by ARA. This library is
> designed to provide the **same information** (the chord symbol + the
> pitch-class set) from MIDI/audio, so it can later be injected into that
> mechanism without modifying it.

---

## 1. Preparation phase and technical specifications

### 1.1 Input / output data formats

#### MIDI input (MIDI 1.0 standard)
- Handled messages : `Note On` (0x90, velocity > 0), `Note Off` (0x80) and
  `Note On` at velocity 0 (equivalent to Note Off), `All Notes Off` (0x7B),
  `All Sound Off` (0x78), `Reset All Controllers` (0x79).
- Input structure : a stream of timestamped events
  `{ timestamp (ms or ticks), status, data1, data2 }`, or a raw MIDI buffer
  (bytes) to parse.
- Velocity : used as a **weight** in the active-note set (a very weak note can
  be ignored via a configurable threshold), but not in the chord recognition
  itself.
- Output : the state of the **active notes at time t** (pitch-class set +
  octave).

#### Audio input
- File formats : **WAV** (PCM 16/24-bit, mono/stereo) and **MP3** (decoding via
  a dedicated dependency, see §1.3).
- Real-time stream : float buffer `float*` + `numSamples` + `sampleRate`.
- Supported sample rates : **44.1 kHz** and **48 kHz** (others are resampled
  internally to 44.1 kHz).
- Bit depth : **16/24-bit** (converted to normalized `float` [-1, 1]).
- Output : detected pitch-class set + confidence + time window.

#### Common output (MIDI + audio)
Normalized result structure :
```cpp
struct ChordResult {
    int rootPitchClass;          // 0=C .. 11=B
    std::vector<int> pitchClasses; // full set (0..11)
    std::string symbol;          // e.g. "Cmaj7", "Csus4", "G7"
    float confidence;            // 0..1
    double timestamp;            // position (ms or ticks depending on source)
};
```

### 1.2 Chords to detect and recognition rules

Target set (with inversions) :

| Family | Symbols | Intervals (relative to root) |
|---|---|---|
| Major | `C`, `Cmaj7`, `Cmaj9`, `C6` | {0,4,7}, {0,4,7,11}, {0,4,7,11,14}, {0,4,7,9} |
| Minor | `Cm`, `Cm7`, `Cm9`, `Cm6` | {0,3,7}, {0,3,7,10}, {0,3,7,10,14}, {0,3,7,9} |
| Seventh | `C7`, `C9`, `C13` | {0,4,7,10}, {0,4,7,10,14}, {0,4,7,10,14,21} |
| Half-diminished | `Cm7b5` | {0,3,6,10} |
| Diminished | `Cdim`, `Cdim7` | {0,3,6}, {0,3,6,9} |
| Augmented | `Caug`, `Caug7` | {0,4,8}, {0,4,8,10} |
| Suspended | `Csus2`, `Csus4`, `C7sus4` | {0,2,7}, {0,5,7}, {0,5,7,10} |
| Power | `C5` | {0,7} |
| Inversions | `C/E`, `C/G`, … | same set, bass ≠ root |

**Recognition rules (common MIDI/audio) :**
1. Build the **pitch-class set** (modulo 12) of the active notes.
2. For each candidate root (0..11), compare the set to each chord template
   (subset / superset matching with tolerance).
3. **Ambiguity handling** : the same set can match several chords (e.g. {0,4,7}
   = C major, but also Am7 without the 5th). Prioritize by :
   - completeness (the template that explains the most notes),
   - parsimony (the simplest template),
   - context (most likely root via the pitch-class profile, see §3.2).
4. **Inversions** : if the bass (lowest note in MIDI, or estimated root in
   audio) differs from the root, annotate `root/bass`.
5. **Anti-false-positive validation** : require a minimum number of notes and a
   minimum confidence ; smooth over time (see §3.1.3).

### 1.3 Allowed external dependencies

The library is **C++17** and **without UI / host-plugin dependency**. The
libraries proposed in the statement are mostly **Python/JS** and do not suit a
self-contained C++ library. Recommended choices :

| Need | Proposed in statement | C++ recommendation | Justification |
|---|---|---|---|
| Audio pitch/chroma | librosa (Python), essentia (C++/Python) | **Internal implementation** (primary) ; **Aubio** (optional) | Chroma extraction (FFT + logarithmic mapping onto 12 classes) is simple to implement and sufficient for chord recognition. Aubio is kept only if a benchmark shows clearly better accuracy/performance (decision criterion : see §3.2). Essentia is heavy ; librosa is Python → unusable in C++. |
| MP3 decoding | — | **minimp3** (header-only) | Light, dependency-free, CC0 license. **Validated.** |
| MIDI parsing | web-midi-api, midi.js (JS) | **Internal implementation** (≈200 lines) | MIDI 1.0 parsing is trivial in C++ ; no dependency needed. web-midi-api/midi.js are JS → unusable. |
| Tests | — | **Catch2** (header-only) | Standard, light, cross-platform. |

**Strict rule** : no dependency on JUCE, the plugin, or any GUI library. The
allowed dependencies are limited to Aubio (optional, replaceable by internal
DSP), minimp3 and Catch2 (tests only).

### 1.4 Performance constraints

| Constraint | Value | Remark |
|---|---|---|
| Detection latency | **≤ 10 ms** (MIDI) ; **progressive sliding** (audio) | MIDI : instant. Audio : **sliding** detection — a sliding window (50–100 ms) is analysed and a **progressive output** is emitted (the result stabilizes over frames) rather than a fixed-latency binary result. This compromise is **retained** (see §3.2). |
| Memory | **< 50 MB** (compiled library) | Easy to hold (no heavy ML model). |
| CPU | Real-time on one core | Internal chroma : ~1–5 % of a core at 44.1 kHz. |
| Cross-platform | **Windows, macOS, Linux** | C++17 + CMake, no OS-specific API. |

---

## 2. Standalone library architecture

### 2.1 Modular architecture

Strict separation into modules, each with a single responsibility :

```
libs/ovtchord/
├── include/ovtchord/          # public API (stable headers)
│   ├── ovtchord.h             # single entry point
│   ├── types.h                # ChordResult, MidiEvent, AudioFrame, enums
│   ├── midi_parser.h          # MIDI module API
│   ├── audio_processor.h      # audio module API
│   ├── chord_engine.h         # common engine API
│   └── export.h               # result export API
├── src/
│   ├── midi/                  # MIDI 1.0 parsing + active-note tracking
│   │   ├── midi_parser.cpp
│   │   └── note_tracker.cpp
│   ├── audio/                 # decoding, preprocessing, pitch/chroma extraction
│   │   ├── audio_decoder.cpp  # WAV + MP3 (minimp3)
│   │   ├── preprocess.cpp     # bandpass, gate, normalization
│   │   └── pitch_extractor.cpp# Aubio or internal chroma
│   ├── core/                  # common recognition engine
│   │   ├── chord_engine.cpp   # matching + ambiguities + validation
│   │   └── chord_templates.cpp# chord templates
│   ├── api/                   # public facade, lifecycle, callbacks
│   │   └── ovtchord.cpp
│   └── export/                # result serialization
│       └── export.cpp         # JSON (internal implementation, no dependency)
├── tests/
│   ├── unit/                  # per-module unit tests
│   └── integration/           # integration tests (called from a host)
├── tools/                     # build utilities / data generation
└── CMakeLists.txt             # standalone target (no link to the plugin)
```

**Dependencies between modules** (one-way, no cycle) :
`midi/` and `audio/` → `core/` → `api/` → `export/`.

### 2.2 Full isolation from the plugin and the UI

- **Static API** : the library exposes only free functions
  (`ovtchord_init`, `ovtchord_start`, `ovtchord_stop`, `ovtchord_set_callback`,
  `ovtchord_process_midi`, `ovtchord_process_audio`, `ovtchord_get_result`,
  `ovtchord_shutdown`) and pure data structures. No class for the plugin to
  instantiate, no inheritance.
- **No import** of host-plugin modules (no `PluginProcessor.h`, no JUCE, no
  `ovtdsp`).
- **No display call** or UI event handling : the library touches neither
  `juce::Graphics`, nor `Component`, nor UI threads.
- **Thread-safety** : the API is designed to be called from the audio thread
  (process) and the UI thread (configuration) ; shared data is protected by
  internal mutexes or atomics, without blocking the audio thread (no allocation
  in the real-time path).

### 2.3 Project structure

- The library lives in `libs/ovtchord/`, **separate** from `Source/` (plugin)
  and `test/` (plugin tests), to avoid any pollution.
- Standalone `CMakeLists.txt` : `add_library(ovtchord STATIC ...)` + option
  `OVTCHORD_BUILD_TESTS` (OFF by default). No link to the plugin targets.
- Unit and integration tests are in `tests/`, compiled by a separate
  `ovtchord_tests` target.

---

## 3. Sequential feature implementation

### Phase 1 — MIDI chord detection module

1. **MIDI 1.0 parsing** (`midi_parser.cpp`)
   - Decode messages (status byte, running status, data bytes).
   - Handle Note On/Off, velocity 0, All Notes Off, etc.
   - Produce a list of timestamped events.
2. **Active-note tracking** (`note_tracker.cpp`)
   - Maintain a set of held notes `{ note, velocity, timestamp }`.
   - Update on each Note On/Off.
   - Apply a velocity threshold to ignore spurious notes.
3. **Matching engine** (reused by audio, see `core/`)
   - Convert active notes into a pitch-class set.
   - Match against the templates (§1.2) with ambiguity handling.
4. **Validation** (`chord_engine.cpp`)
   - Require a minimum number of notes and a minimum confidence.
   - Temporal smoothing : only declare a chord if it is stable over N frames
     (removes transient spurious notes).

### Phase 2 — Audio chord detection module

1. **Decoding** (`audio_decoder.cpp`)
   - WAV (PCM 16/24-bit) : direct read.
   - MP3 : decoding via minimp3.
   - Resample to 44.1 kHz if needed.
2. **Preprocessing** (`preprocess.cpp`)
   - **Bandpass** filter (e.g. 60 Hz – 4 kHz) to isolate the harmonic zone.
   - **Noise removal** : spectral gate or noise-floor subtraction.
   - **Volume normalization** (RMS or peak) for stable extraction.
3. **Fundamental extraction** (`pitch_extractor.cpp`)
   - **Option A (retained)** : **chroma** extraction (projection of the spectrum
     onto the 12 pitch classes) via **internal implementation** (FFT +
     logarithmic mapping). The chroma directly gives the pitch-class set, without
     explicit polyphonic detection.
   - **Aubio vs internal decision criterion** : implement the internal chroma
     first, then compare it to Aubio on a set of reference files (recognition
     accuracy + CPU cost). Aubio is adopted only if it brings a measurable gain ;
     otherwise stay internal (zero dependency).
   - **Option B** : polyphonic multi-F0 detection (more expensive, more accurate
     on tight chords). Reserved for future evolution.
4. **Common engine** : the chroma/pitch-class set feeds the **same**
   `chord_engine` as MIDI → consistent results.

> **Progressive sliding detection (audio)** : instead of a fixed-latency binary
> result, a sliding window (50–100 ms) is maintained and an output that
> **stabilizes progressively** is emitted : each frame updates the pitch-class
> set and the confidence, and the `ChordResult` is only declared « stable » after
> N consecutive frames. This gives a low perceived latency (the result appears
> quickly) while filtering transient false positives.

### Phase 3 — API unification and standardization

1. **Normalized lifecycle**
   - `ovtchord_init(config)` : configuration (source, thresholds, templates).
   - `ovtchord_start()` / `ovtchord_stop()` : start/stop detection.
   - `ovtchord_process_midi(events)` / `ovtchord_process_audio(frame)` : inputs.
   - `ovtchord_get_result()` : last result.
   - `ovtchord_shutdown()` : release.
2. **Callback system** (no strong coupling)
   - `ovtchord_set_callback(on_chord, user_data)` : the library notifies the
     calling code on each detection, via a function pointer + context.
   - The callback is called on the caller's thread (no imposed internal thread),
     which avoids any coupling and any lifecycle problem.

---

## 4. Tests and validation

### 4.1 Unit tests (`tests/unit/`)
- **MIDI parsing** : reference MIDI files (Note On/Off, running status,
  velocity 0, All Notes Off) → verify the active-note state.
- **MIDI detection** : known MIDI sequences (C, Cm, C7, Csus4, inversions)
  → verify the symbol and the pitch-class set.
- **Audio pitch/chroma extraction** : test audio files (synthesized chord
  sounds) → verify the extracted pitch-class set.
- **Chord matching** : case table (set → expected chord, ambiguities, false
  positives).

### 4.2 Integration tests (`tests/integration/`)
- A test host (console) calls the library via the public API, as the plugin
  would, and verifies that :
  - the results are correct,
  - no crash, no memory leak,
  - the library has **no impact** on a host that does not use it (no residual
    threads, no polluted globals).

### 4.3 Performance measurements
- **Latency** : time between input (MIDI/audio) and result output.
- **CPU** : % of a core at 44.1 kHz in real time.
- **Memory** : RSS of the loaded library.
- Validate the §1.4 constraints (latency ≤ 10 ms — to confirm for audio, memory
  < 50 MB).

### 4.4 Verification of no plugin modification
- `git status` / `git diff` on `Source/` and `test/` (plugin) : **no
  modification** at the end of the implementation.
- The library lives only in `libs/ovtchord/`.

---

## 5. Final deliverables

1. **Compiled library + documented source code**
   - `libs/ovtchord/` with documented public API (Doxygen on the headers).
   - Artifact : `libovtchord.a` / `ovtchord.lib` + headers.
2. **Test report**
   - MIDI and audio detection success rate (per chord family).
   - Performance measurements (latency, CPU, memory).
3. **Integration documentation**
   - `docs/integration-ovtchord.md` : how to call the library from the plugin
     (code example), without modifying the existing architecture — in particular
     how to inject the `ChordResult` into the existing
     `ScaleQuantizer::setChordOverride` mechanism.

---

## Risks and points to validate

- **Audio latency** : the strict ≤ 10 ms constraint is unrealistic for reliable
  detection (50–100 ms window). **Retained decision** : **sliding detection with
  progressive output** (the result stabilizes over frames) — see §1.4 and §3.2.
- **Dependencies** : the libraries listed in the statement (librosa, essentia,
  web-midi-api, midi.js) are Python/JS and not suited to a self-contained C++
  library ; replaced by minimp3 (validated) + internal chroma (Aubio optional
  depending on the benchmark) — see §1.3.
- **Audio quality** : chord detection on a real signal (voice + accompaniment)
  is probabilistic ; the success rate will depend on the material.
