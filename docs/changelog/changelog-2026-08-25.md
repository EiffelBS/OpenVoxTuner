# Changelog — 2026-08-25

## Repo hygiene: dead-reference sweep, internal archive move, contributing fix (2026-08-25 ~03:55 CEST)

Second batch of follow-ups to the external AI project-analysis review,
covering the items decided with Jerome on 2026-08-24 late evening
(roadmap section "8s"). Build-system and documentation hygiene only —
no source code changed.

### Context
The git-tracked documentation referenced files that live in git-excluded
folders (historical daily changelogs under `docs/changelogs/`, superseded
planning documents moved to `docs/archive/` earlier this batch). On a fresh
clone those links are dead. Decision: no tracked file may reference an
excluded path; excluded material stays available locally only.

### Changes
- **Dead-reference sweep (`docs/implementation-roadmap.md`)**: all 24
  references to `docs/changelogs/changelog-*.md` rewritten as dated
  "dev log" mentions without file paths (batches 2026-07-17 x10,
  2026-07-23 x8, 2026-08-02 x2, 2026-08-03 x4). Quoted user sentences and
  historical test counts preserved verbatim.
- **Historical changelogs moved**: `git ls-files` confirmed none of the
  ~37 daily logs (2026-06-09 -> 2026-08-10) was tracked; the folder was
  moved from `docs/changelogs/` to `docs/internal/changelogs/`
  (covered by the existing `.gitignore` entry `docs/internal/`). No
  `.gitignore` change needed.
- **Contributing fix**: `CONTRIBUTING.md` + `docs/contributing.md` —
  the "add a changelog entry" instruction now points to the tracked
  `docs/changelog/` folder instead of the excluded `docs/changelogs/`.
- **Archived-plan reference fix** (`docs/implementation-roadmap.md`,
  MIDI Import section): path link to the archived
  `implementation-plan-midi-import-drag-and-drop.md` replaced by an
  "archived locally" note.
- Kept on purpose: the historical note in `changelog-2026-08-01.md`
  that documents the "no references to git-excluded changelogs" rule
  itself (it is the archive of the rule).

### Earlier items of the same batch (late 2026-08-24)
Already applied on 2026-08-24 evening, logged here for completeness:
- **i18n sweep 2**: remaining French text in `CMakeLists.txt` comments and
  the PowerShell / bash helper scripts translated to plain ASCII English.
- **test/ cleanup**: temporary `test/decode_settings.py` removed (its
  conclusion stays documented in roadmap entry HC.11);
  `test/dsp/click_count.txt` artifact added to `.gitignore`.
- **Legacy option removal**: `AUTOTUNE_BUILD_TESTS` CMake option deleted
  (leftover from the `autotune_clone` era); tests target builds
  unconditionally like every other target.
- **Docs archival**: five superseded planning documents moved to the local
  git-excluded `docs/archive/` folder; the two active plans stay tracked.
- Roadmap updated with section "8s. Repo hygiene & docs archival
  follow-up (2026-08-24 -> 2026-08-25)" (entries HS.1-HS.6).

### Validation
- Repo-wide grep: zero references from tracked files to git-excluded paths
  (except the intentional 2026-08-01 rule note).
- Full Release rebuild OK (SharedCode, VST3, Standalone, OpenVoxTunerTests;
  a sandbox restriction blocked unrelated NVIDIA ShadowPlay log writes at
  the very end of the msbuild run — build outputs were all produced).
- Unit-test suite: `Result: 97 OK, 0 FAILED` (incl. PitchShifterOutput /
  PitchShifterOutputRms / PitchShifterClick suites exercising the modified
  click test).

---

## Roadmap accuracy audit vs codebase (2026-08-25 ~05:04 CEST)

Verified every unchecked roadmap item against the actual codebase, and
sampled checked items for removed features.

### Findings and fixes
- **Backlog "Note name labels on additional piano keys (D, E, F, G, A, B)"**
  was still unchecked although implemented (`Source/ui/PianoKeyboard.cpp`:
  white-key labels C/D/E/F/G/A/B with octave suffix on C, height-gated
  >= 20 px). Marked [x] with an implementation note.
- **Section 9 "Pitch Visualizer improvements documentation"**: annotated
  (archived locally, no longer in the tracked tree) after yesterday's
  docs archival.
- **Stale header** "Last updated: 2026-07-31" refreshed to the current date.

### Confirmed correct (no change)
- Bookmark positions, responsive small-screen layout, accessibility:
  no matching code found — correctly unchecked.
- MIDI Import "Validation": no `MidiImporter` test exists — correctly
  unchecked (the drag-and-drop feature itself is implemented and checked
  in section 5b; only the automated test coverage is missing).
- LP.7 MUSHRA harness: pending by design (listening panel + corpus).
- LV2 format: under reflection, correctly open.
- Website: no swipe support, no testimonials component, no user-facing
  theme toggle button (only automatic prefers-color-scheme handling in
  `Layout.astro`) — all three correctly unchecked.
- Checked items sampled (piano-roll mode, spectral/EQ view, PresetGallery,
  MIDI target, key detection, Dark/Light theme, image export): all still
  present in the codebase. Deprecated features (FlexTune, attack-aware
  correction) properly struck through.

### Audit correction (user feedback, ~05:15 CEST)
The initial audit wrongly reported "touch gestures: no matching code" —
the grep patterns missed JUCE's `mouseMagnify` API. Pinch-to-zoom IS
implemented on macOS trackpads (`mouseMagnify` overrides on
PitchVisualizer + PitchCurveEditor, Ctrl/Cmd+wheel equivalent, and
pinch-vs-smooth-scroll disambiguation). Roadmap item updated to [~]
"largely implemented"; only raw multi-touch touchscreen finger gestures
remain open (JUCE emulates single-touch mouse there).

### Backlog cleanup decision (user, ~05:20 CEST)
The two stale wishlist items "Bookmark positions" (save/restore
frequency-range view presets) and "Responsive layout for small screens"
(adapt plugin UI at tiny host window sizes) were removed from the backlog
and from section 4, on Jerome's decision.

---

## Tests: MidiImporter validation suite (2026-08-25 ~07:26 CEST)

Soldered the last open item of roadmap section 5b (MIDI Import): the
missing automated test coverage.

### New file `test/dsp/MidiImporterTest.cpp` (14 sub-tests)
Self-contained synthetic MIDI generation (in-memory `juce::MidiFile` with
480 TPQN + explicit 120 BPM tempo meta written to per-test temp files,
RAII-deleted). Coverage:
- `analyzeFile`: monophonic single-channel summary; channel 10 percussion
  excluded from the summary and note counts; multi-channel listing sorted
  by channel with per-channel min/max/count/duration.
- Rejection paths: garbage bytes ("Invalid MIDI file format"), nonexistent
  path ("Cannot open file"), syntactically valid file with no notes.
- `importFrom`: monophonic sequence -> one point per note at exact times
  and frequencies; polyphonic reduction Highest/Lowest with staggered
  extents (documents the sweep-line transition point emitted when a
  selected note ends while another sustains); Loudest strategy;
  SpecificChannel filtering; percussion-only -> empty curve; sub-20 ms
  artefact notes filtered; dangling note-on (analyze counts it, import
  needs a paired note-off); garbage file -> empty curve.

### Build integration
`Source/dsp/MidiImporter.cpp` added to the `OpenVoxTunerTests` target
sources in `CMakeLists.txt`; test included in `test/Main.cpp`.

### Validation
Full suite: **111 OK / 0 FAILED** (was 97 + 14 new). Two initial test
expectations were corrected during bring-up (percussion note counting;
transition-point emission on note boundaries) — no product code changed.

### Docs
Roadmap section 5b Validation checked with details; `docs/testing.md`
tree + coverage list + sample output updated.
