# Changelog — 2026-08-24

## Docs: deduplicate architecture pages and fix factual drift

### Context
The public MkDocs site (ovtdocs.eiffelbs.ovh) exposes two Architecture pages:
`architecture.md` ("Overview") and `architecture/dsp-pipeline.md` ("DSP
Pipeline"). The Overview page carried older copies of the DSP content that had
drifted from both the code and the dedicated DSP Pipeline page.

### Changes
- **docs/architecture.md** (313 -> 215 lines):
  - Replaced the outdated simplified pipeline diagram (missing NoiseGate,
    HarmonyEngine, ReverbEffect) with the full stage list plus a link to the
    DSP Pipeline page, now declared the single source of truth for signal-flow
    details.
  - Removed the "Algorithms Used" section (YIN / PSOLA / formant / retarget
    prose duplicated by dsp-pipeline.md stages 2/4/5/6).
  - Removed the "Latency" section (table duplicated verbatim in dsp-pipeline.md).
  - Replaced the stale "Exposed Parameters" table with a grouped summary linking
    to `default-parameters.md`. Fixes vs the old table:
    - `correction_mode` default was listed as Modern (false); the code default
      is true / Transparent (`PluginProcessor.cpp` line ~606).
    - Missing newer parameters: `key_detect`, `key_source`, `companion_group`,
      `formant_strategy`, `harmony_gain_match`.
  - Updated "Future Work" (was "Future Phase"): LPC cross-synthesis P0/P1/P2 is
    implemented (`LpcFormantPreserver`) and no longer listed as future;
    remaining work = LPC hop/overlap-add framing (LP.7) + MUSHRA harness.
- **docs/architecture/dsp-pipeline.md**: added cross-links to the Architecture
  Overview and Default Parameters pages.
- **docs/formant-preservation-analysis-report.md**: fixed the 3 broken links
  reported by `mkdocs build --strict` (all pre-existing, unrelated to the edits
  above):
  - `../docs/architecture.md` -> `architecture.md` (wrong relative path).
  - `../Source/dsp/FormantPreserver.h` and `../Source/dsp/PitchShifter.cpp`
    converted to inline code (files outside the docs tree cannot be linked).
  - Added a status-update banner: LPC cross-synthesis is now implemented via
    `LpcFormantPreserver` (P0/P1/P2); sections describing the pre-LPC state are
    kept as historical reference.
- Local validation: `mkdocs build --strict` now exits with 0 warnings (previously
  3). Remaining build output is INFO-level only (pages outside nav, stale anchors
  in implementation plans).

### Impact on deployment
Pushing these changes to `main` triggers `.github/workflows/deploy-docs.yml`
(MkDocs build -> Cloudflare Pages). No URLs or nav entries changed, so no links
break; only page content shrinks/improves.

### Notes
- No source code changed; unit tests unaffected.

---

## Code hygiene: mojibake repair, dead debug hooks removal, legacy PitchDetector removal (2026-08-24 ~03:15 CEST)

### Context
Three actionable items from an external code review: double-encoded
(mojibake) text in `Source/` files, dead debug hooks around the PSOLA grain
counter, and a legacy YIN pitch detector superseded by `YinPitchDetector`.

### 1. Mojibake repair (`Source/`)
- Fixed double-encoded text (UTF-8 bytes re-read as cp1252 then re-saved as
  UTF-8) across 19 files (~850 runs), plus a second greedy incremental pass on
  `Source/ui/OVTLanguages.h` (2126 sequences) that handles runs mixing valid
  CJK with mojibake.
- English comments affected by the corruption were rewritten in clean English;
  localized UI strings in `OVTLanguages.h` keep their original languages with
  accents/CJK restored (verified: French "accordée", German "Kurven löschen",
  Japanese full sentences).

### 2. Dead debug hooks removal
- Removed `gPitchShifterGrainEvents` global atomic counter
  (`PitchShifter.h/.cpp`) — incremented but its only consumer copy
  (`lastObservedGrainCount`, `PluginProcessor.h/.cpp`) was never read.
- Removed the unused wrapper `OpenVoxTunerAudioProcessor::forceCreatePitchTestGrain()`.
- Kept the live debug chain intact: `dbg_test_grain` parameter -> UI button ->
  `PitchShifter::forceCreateTestGrain()`.

### 3. Legacy `PitchDetector` removal
- Deleted `Source/dsp/PitchDetector.h` / `.cpp` (legacy YIN implementation,
  unused by the pipeline) and `test/dsp/PitchDetectorTest.cpp`.
- Verified beforehand that `YinPitchDetector` contains all anti-octave logic in
  evolved form (fundamental-priority strategy below clarity 0.30 replacing the
  old octave-continuity heuristic) — no functional loss.
- Updated `CMakeLists.txt` (plugin + tests targets) and docs:
  `docs/reference/dsp-api.md` (Legacy note removed), `docs/architecture.md`
  (source tree), `docs/testing.md` (tree + sample output),
  `docs/implementation-roadmap.md` (historical entry annotated).

### Validation
- Grep confirms zero residual references to the removed symbols
  (`gPitchShifterGrainEvents`, `forceCreatePitchTestGrain`,
  `ovtdsp::PitchDetector`) outside dated historical documents.
- Full Release build OK (SharedCode, VST3, Standalone, OpenVoxTunerTests).
- Unit-test suite: **97 OK / 0 FAILED** (incl. PitchShifterOutput/Rms/Click suites
  exercising the modified `PitchShifter.cpp`).

---

## i18n: all remaining French comments translated to English (2026-08-24 ~12:45 CEST)

### Context
Before the "all comments/docs in English" rule was adopted, a large body of
French comments and display strings had accumulated across the codebase. This
work translates every remaining translatable French comment to plain ASCII
English so the whole source tree follows the project convention.

### Changes
- **Source/** (previous sessions): French comments purged from
  `Source/dsp/`, `Source/ui/`, and `Source/core/` (parameter definitions,
  ARA glue, DSP modules). Identifiers (APVTS parameter IDs) untouched.
- **test/** (this session): all 11 files containing French translated —
  `ScaleKeyboardComponentTest`, `PitchCurveEditorTest`, `FormantPreserverTest`,
  `HarmonyGainMatchTest`, `PitchShifterClickTest`, `PitchShifterOutputRmsTest`,
  `PitchShifterOutputTest`, `PitchCurveTest`, `RetargetEnvelopeTest`,
  `ScaleQuantizerTest`, plus a residual mojibake fix in
  `UpwardCompressorTest` (`â‰ˆ` -> `~`). `beginTest(...)` titles and
  `expect(...)` messages (display text) were translated too.
- **Mojibake repair** (`FormantPreserverTest.cpp`): double-encoded fragments
  (`empÃªche`, `tolÃ¨re`, `inÃ©vitable`, `alternÃ©`, `â€”`, `Ã—`, `Qâ‰¤`)
  rewritten as clean ASCII English prose.
- **Test console output internationalized** (`test/Main.cpp`):
  `Resultat : X OK, Y KO` -> `Result: X OK, Y FAILED`.
- Preserved on purpose: localized UI strings in `OVTLanguages.h` (accents are
  part of the translations), and the quoted user sentence in
  `PluginEditor.cpp` (~line 2244) documenting a dated bug report.

### Validation
- Grep sweeps over `Source/` + `test/`: zero matches for accented-character
  patterns and for common unaccented French word patterns.
- Full Release build OK (SharedCode, VST3, Standalone, OpenVoxTunerTests).
- Test suite output: `========================= Result: 97 OK, 0 FAILED
  =========================`.
- Docs updated: `docs/testing.md` sample output now shows the new English
  summary line.

## Refactor: PitchShifter implicit state machine made explicit (2026-08-24 ~21:20 CEST)

### Context
The attack-envelope / voice-detection logic of `PitchShifter` was spread over
six loose members (`attackGain`, `attackAlpha`, `slowAttackSamplesRemaining`,
`attackRampDownSamplesRemaining`, `hystVoiced`, `voiceDebounceCounter`) whose
interaction formed an undocumented implicit state machine. Hard to reason
about, easy to regress.

### Changes
- **`VoiceActivityDetector` struct** (`Source/dsp/PitchShifter.h`): hysteresis
  on raw f0 (on >45 Hz, off <35 Hz) + 256-sample (~6 ms) debounce extracted
  into a self-contained struct (`processSample()` / `reset()`). Logic
  preserved exactly.
- **`AttackEnvelope` struct** (`Source/dsp/PitchShifter.h`): explicit
  `Phase { Open, RampDown, RecoverSlow, RecoverNormal }` derived from named
  timers; arming helpers `armForOnset()` (ramp-down 20 ms / slow recover
  150 ms), `armForRatioJump()` (15 ms / 100 ms, no OLA restart),
  `snapToZero()` (block onset), `forceOpen()`, `clearTimers()`. The one-pole
  integration is bit-identical to the previous code (same constants, same
  operation order).
- **`restartGrainChainOnOnset()` helper** (`Source/dsp/PitchShifter.cpp`):
  centralizes `outPhase = 1.0` + `lastGrainCenter = 0.0` on voice onset
  (anti-burst "trumpet" behavior, Fixes J/K2/O rationale), independent of the
  envelope.
- **`process()` final gain dispatch** rewritten as three explicit modes:
  external driver (Fix AW path pushes a block-level target into
  `attackEnvelope.gain`, internal timers bypassed) / internal envelope /
  bypass (gain forced to 1).
- Stale comment in `Source/PluginProcessor.cpp` referencing the removed
  members updated; no functional change there.
- Public API unchanged.

### Intentional (inaudible) deltas
- `clearTimers()` is now called in `reset()` / `resetSoft()`: with the ring
  buffer emptied, a pending ramp-down/recover timer had no audible effect.
- `voiceDetector.reset()` added in `resetSoft()`: guarantees quiescence after
  the ring flush (previously only `reset()` cleared it).

### Validation
- Full Release build OK.
- Test suite output: `========================= Result: 97 OK, 0 FAILED
  =========================` — the PitchShifterOutput / PitchShifterOutputRms /
  PitchShifterClick suites confirm bit-identical behavior.
- Docs updated: roadmap section "8r. PitchShifter: explicit state machine
  refactor" and a new "Voice activity & attack envelope automaton" paragraph
  in `docs/architecture/dsp-pipeline.md` (PitchShifter stage).

## Housekeeping: stale build directories removed (2026-08-24 ~22:44 CEST)

Following the external AI project-analysis review ("cluttered local tree:
4 build directories"), audited the local workspace:

- `build/` (3.1 GB) — active, kept (last write: today's Release build).
- `build-test/` (200 MB, last write 2026-07-27), `build-tests/` (655 MB,
  last write 2026-08-15) — obsolete separate test-build trees; tests now
  build inside `build/` (`OpenVoxTunerTests_artefacts/Release/`). Deleted.
- `build-ovtchord/` (21 MB, last write 2026-08-15) — standalone sandbox for
  the ovtchord library; regenerable in seconds via the commands documented
  in `libs/ovtchord/CMakeLists.txt`. Deleted.

All three were already gitignored; ~876 MB of disk reclaimed and no risk
left of running stale binaries from an old cache. Also removed the duplicate
`/build/` entry in `.gitignore` (line 97 duplicated line 87). The ignore
entries themselves are kept so accidental regeneration stays ignored.

No source code touched; build/test validation not applicable.

