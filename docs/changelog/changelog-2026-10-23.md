# Changelog — 2026-10-23

## Feature — Hard Tune (Cher / T-Pain) factory preset

### Context

The autotune-as-an-instrument sound ("Cher / T-Pain") was reachable by hand but
had no factory entry point. The closest preset, **Creative Robot**, did not
actually deliver it: it is built from the factory-default state plus its
overrides, and it named neither the correction mode nor the speed. The factory
`correction_mode` is Transparent (which scales Amount by 0.8 and floors Speed at
30 ms) and the factory `speed` is 20 ms, so the preset loaded as a *soft* robot
while its description promised a hard tune.

### What shipped

- **New factory plugin preset `Hard Tune (Cher / T-Pain)`** in
  `Source/PluginEditor.cpp` (`getFactoryPluginPresetNames()` /
  `getFactoryPluginPresetOverrides()`), inserted after Creative Robot in the
  list order. Overrides: `speed=0`, `amount=1`, `correction_mode=0` (Modern),
  `flex_tune=0`, `humanize=0`, `vibrato_preserve=0`, `formant_enable=1`,
  `formant_mode=1` (MultiFormant), `formant=2` st, `formant_strategy=4`
  (Precise), Harmony/Doubler/Delay/Reverb/Gate/Upward-Comp/De-Esser off,
  Saturation on at drive 0.40 / warmth 0.25, Limiter on.
- **Creative Robot now pins `speed=0` and `correction_mode=0`**, so the preset
  finally matches its own description ("hard tune with a strong formant
  shift").
- **Docs**: `docs/reference/presets.md` gains the table row and a
  "Hard tune (Cher / T-Pain)" section (recipe, T-Pain variant, production
  notes, engine note); `docs/user-guide/correction-modes.md` gains the
  Transparent clamp in the correction-mode table, the "0 ms" nuance in the
  speed table and a workflow tip; `docs/default-parameters.md` now points at
  `docs/reference/presets.md` instead of repeating a stale preset list (it
  still named seven presets that no longer exist).

### Design facts

- **Modern is mandatory, and it is not a sound-flavoured value.** Transparent
  multiplies Amount by 0.8 and clamps Speed to ≥ 30 ms
  (`PluginProcessor::syncParameters` / `processBlock`), so a hard-tune preset
  that leaves the mode alone can never sound hard. This was the Creative Robot
  defect: its Transparent load happened through the default state, not through
  any override.
- **One preset, not two.** The Cher and T-Pain flavours differ by the Formant
  switch (`+2 st` vs off, or `+1 st`) and by the Saturation drive; shipping two
  near-duplicate entries would crowd the first screen. The T-Pain variant is
  documented in the preset reference instead.
- **Key/scale and latency mode are deliberately not overridden.**
  `applyPluginPresetState()` preserves `key`, `scale` and `key_detect` (the
  user's musical choice), and the preset leaves `latency_mode` at the factory
  default so loading it never changes the session's monitored latency
  (Direct Monitoring is a live decision, Quality a mixing one).
- **`formant_strategy = Precise`** keeps the timbre uncoloured (allpass path):
  the hard-tune character must come from the pitch snap and the formant shift,
  not from a tone-colouring strategy.
- **Saturation is the only master stage on, and the De-Esser stays off.** The
  added harmonics are the edge the preset wants; de-essing them would undo it
  (same rule Creative Robot already followed).
- **Speed 0 is still bounded by the 80 ms anti-jitter one-pole** applied after
  the retune envelope (`Source/PluginProcessor.h`, `speedFloor`;
  `test/dsp/SpeedFloorTest.cpp` measures 90 % in 20–45 blocks at 256 samples).
  The preset therefore gives a very fast glide rather than a mathematically
  instantaneous jump. The Hard Tune mode shipped later the same day removes that
  floor — see the next section.

### Testing

- `cmake --build build --config Release --target OpenVoxTuner_Standalone`:
  PluginEditor.cpp compiled and the standalone linked (MSVC, Release).
- `cmake --build build --config Release --target OpenVoxTunerTests` then
  `OpenVoxTunerTests.exe`: **Result: 222 OK, 0 FAILED**.
- The factory preset table lives in `PluginEditor.cpp`, which the test target
  does not compile, so no automated test exercises the new entry; the load is
  checked by the build and by the preset-reference documentation. A manual
  recette (load the preset, read the panel values) remains to be run.

## Feature — Hard Tune mode: the snap path

### Context

The preset delivered the recipe, but the engine still smoothed the correction:
`Speed 0` only reached 90 % of the target in ~184 ms, because the `speedFloor`
one-pole (80 ms) runs unconditionally after the retarget envelope, and the
PitchShifter's attack envelope moves its output gain on every onset and on every
ratio step. A true "Auto-Tune as an instrument" snap needed an engine path, not
another preset value.

### What shipped

- **New APVTS parameter `hard_tune`** (bool, default **false**), declared right
  after `correction_mode` in `Source/PluginProcessor.cpp`. Off by default, so
  every existing session and every factory preset that does not name it keep the
  current behaviour.
- **Processor** (`processBlock` / `syncParameters`):
  - the retarget `speedFloor` is bypassed with `processBypassed()` (which still
    stores the live ratio, so leaving the mode resumes from the right value);
  - the Transparent clamp (Amount × 0.8, Speed floor at 30 ms) is ignored while
    the mode is on — the mode IS the hard path, the `correction_mode` value only
    matters when it is off;
  - `pitchShifter->setRatioJumpEnvelopeEnabled(false)` for the lead shifter and
    for the doubler copy: the ratio-jump arming is the one that would now fire on
    every semitone (the floor no longer hides the > 3 % steps), and its duck +
    recovery is exactly the swell a snap must not have. The ONSET arming stays
    ON: it masks the OLA reorganisation at note attacks (see the click
    measurement in the design facts);
  - the `FormantPreserver` is disabled (it is a zero-latency IIR, so this is a
    CPU/transient/artefact choice, not a latency one) and is `reset()` when the
    mode is left so stale biquad states cannot click;
  - the creative Formant shift keeps working (it lives inside the pitch
    shifter), so the Cher colour survives the mode.
- **UI**: the Modern/Transparent console switch is now a **three-position**
  switch, **Modern | Transparent | Hard**, inside the same 178 × 30 strip. The
  segment widths are MEASURED from the translated labels at paint time (so the
  Japanese "トランスペアレント" cannot clip) and the accent pill animates both
  position and width. One click writes `correction_mode` and `hard_tune` through
  `pushUndoAroundCall`, so a mode change is a single undo entry. The host
  parameters stay independent, so automation on either one keeps working.
- **Morph / A-B**: `hard_tune` is captured in `MorphState`, stepped at 50 % by
  `applyInterpolatedState`, listed in `getMorphParameterIds()`, persisted as a
  `hardTune` attribute in the A/B slot XML and applied by `applyMorphState()`.
- **Preset**: Hard Tune (Cher / T-Pain) now enables `hard_tune`.
- **Docs**: presets reference (mode row + engine note rewritten), user guide
  (three-mode table and the hard-tune tip), default parameters, architecture
  parameter list, preset-morphing table (also corrected its stale "custom scale
  excluded" row — the notes have been morphed since the coverage test landed).
- **Test**: new `test/dsp/HardTuneAttackTest.cpp` (4 groups) — the ratio-jump
  duck exists on the normal path, disappears with the mode, comes back when the
  mode is left, and the onset mask keeps a note attack click-free.

### Design facts

- **A separate parameter, not a Speed value or inference.** `Speed < 0` would
  overload a time constant with an engine switch, and inferring the mode from
  `Speed == 0` would silently change the sound of every existing session that
  uses Speed 0 for a soft reason. The floor also exists for a reason (it
  absorbs YIN per-block jitter); the mode must be explicit and opt-in.
- **The floor bypass alone is not enough, but the whole envelope cannot go.**
  With the raw ratio step passing through, the shifter's ratio-jump detector
  (>3 % per block) would arm the envelope on every semitone, so the mode must
  silence that path. The first implementation disabled the whole envelope and
  the click test caught the cost: a note onset then jumps by 0.39 of full scale
  instead of 0.10, because the ONSET arming is also the click mask for the OLA
  grain reorganisation. The mode therefore disables only the ratio-jump arming
  (`setRatioJumpEnvelopeEnabled`), and the onset mask keeps working. Measured:
  the ratio-jump ramp uses the 80 ms slow alpha, so its "15 ms ramp" is a ~17 %
  amplitude dip followed by a slow recovery — subtle, but precisely the swell
  that makes a fast tune sound soft.
- **What still bounds the snap.** The YIN estimate averages a 4096-sample window
  (~93 ms at 44.1 kHz) and refreshes every 2048 samples (~46 ms); the PSOLA
  declares 10–30 ms and keeps a ~4.5 ms per-sample ratio smoother. Phase 2 (the
  next section) shortened the analysis the same day; the shifter is Phase 3 and
  is not part of these changes.
- **The formant stage is not a latency lever in OVT.** Auto-Tune's Classic Mode
  disables its formant/throat modelling, but OVT's preserver is a zero-latency
  IIR cascade and the only declared latency is the shifter plus the limiter's
  8 samples. Bypassing it buys CPU, cleaner transients and the raw chipmunk
  artefact, not milliseconds.

### Testing

- `cmake --build build --config Release --target OpenVoxTuner_Standalone`: OK
  (PluginEditor.cpp, PluginProcessor.cpp and PitchShifter.h compiled, standalone
  linked).
- `cmake --build ... --target OpenVoxTunerTests` then the runner:
  **Result: 226 OK, 0 FAILED** (222 before, +4 HardTuneAttack groups), including
  `MorphCoverage` (the new parameter is classified) and
  `TranslationCompleteness` (the new "Hard" label exists in all six locales).
- The processor itself is not compiled into the test target, so the
  `hard_tune` wiring in `processBlock` is covered by the build plus code review,
  and the DSP behaviour it relies on is covered by `HardTuneAttack`. A manual
  recette (switch to Hard, play a glide, then leave the mode) remains to be run.

## Feature — Hard Tune analysis path (Phase 2)

### Context

Phase 1 removed the engine's own smoothing (speed floor, ratio-jump duck), which
left the pitch DETECTOR as the dominant delay. Shortening the analysis window is
not free - it can destabilise bass notes and it costs CPU - so it was measured
first: a new test replays the processor's exact analysis chain and reports the
time to *report* a note change for each candidate configuration.

### Measurements (test/dsp/DetectorResponseTest.cpp)

200 -> 220 Hz step, Tenor search range, harmonic-rich voice-like signal:

| Config | Window | Hop | Median | Lock time | YIN cost |
|--------|--------|-----|--------|-----------|----------|
| Live | 4096 | 2048 | 5 | **218.3 ms** | 0.47 ms/s |
| Live, median 3 | 4096 | 2048 | 3 | 171.8 ms | 0.48 ms/s |
| Live, median 1 | 4096 | 2048 | 1 | 125.4 ms | 0.47 ms/s |
| **Hard Tune** | **2048** | **512** | **3** | **55.7 ms** | 0.89 ms/s |
| Hard, median 1 | 2048 | 512 | 1 | 44.1 ms | 0.89 ms/s |
| Hard, hop 1024 | 2048 | 1024 | 3 | 78.9 ms | 0.45 ms/s |

"Lock time" is the first reported pitch within 15 cents of the new note that
stays within 30 cents for 200 ms. Jitter on the steady note is 0.0-0.1 cents for
every config (synthetic tone; the real figure is dominated by the signal).

### What shipped

- **`YinPitchDetector::setMedianFrames(1..5)`**: the outlier-rejection median
  becomes configurable (default 5 = the live behaviour, unchanged); the ring is
  read in chronological order so a shorter window only looks at the newest
  frames.
- **Hard Tune analysis path** (`computeInputPitch` / `syncParameters`): while
  `hard_tune` is on, the analysis uses a 2048-sample window refreshed every 512
  samples and a 3-frame median; the live path keeps 4096/2048/5. The FIFO stays
  4096 samples - only the analysed slice moves, starting `windowSize` back from
  the write head.
- **The anti-octave guard is hop-scaled**: its persistence counter is expressed
  in analysis frames, so Hard Tune uses 12 frames instead of 3 to keep the same
  ~140 ms register-change window (3 x 2048 == 12 x 512 samples). Without this the
  shorter hop would let octave glitches through in ~35 ms.
- Docs updated (preset reference engine note, user guide, parameter reference).

### Design facts

- **The 5-frame median was a large, invisible share of the delay.** At the live
  46 ms hop it spans ~232 ms and only moves after two frames: dropping it alone
  takes 218 -> 125 ms. The window+hop cut then takes 218 -> 56 ms.
- **Median 3 is the compromise**: median 1 would save another ~12 ms but only
  smooths one frame; on real, noisy voice the 3-frame median keeps the hard-tune
  staircase without exposing per-frame outliers.
- **CPU is not a concern.** YIN costs 0.47 ms of CPU per second of audio on the
  live path and 0.89 ms/s in Hard Tune (Tenor range; Universal is higher because
  the lag search is wider). That is under 0.1 % of one core either way, so the
  4x analysis rate is affordable.
- **Bass is safe.** The short window's half-length is 256 samples at 11.025 kHz,
  so YIN can represent periods down to 43 Hz; G2 (98 Hz) measures 0.0 cents with
  the same 55.7 ms lock. E2 at the exact bottom of the Bass range (82.41 Hz) is a
  pre-existing range edge - its period equals `maxLag` - not a short-window
  regression, and the test says so explicitly.
- **Why not a 1024-sample window**: it would cut the representable bass to 86 Hz
  and break Bass/Baritone. 2048 is the shortest window that keeps every Voice
  Type usable.
- **What still bounds the snap**: the PSOLA's declared latency (10-30 ms) and its
  ~4.5 ms per-sample ratio smoother, plus the averaging a pitch estimate
  inherently needs. A sub-5 ms response remains Phase 3 (a low-latency shifter).

### Testing

- `cmake --build build --config Release --target OpenVoxTuner_Standalone
  OpenVoxTunerTests`: OK.
- Full suite: **Result: 228 OK, 0 FAILED** (226 before, +2 DetectorResponse
  groups / 8 assertions), including the bass case and the YIN-cost bound.
- The measurement test is deterministic (fixed signal, fixed hop grid); the YIN
  cost is logged and only bounded loosely (3x) because wall-clock timing is not
  bit-stable across machines.

## Analysis — Phase 3: low-latency pitch shifter benchmark

### Context

Phase 2 left the pitch shifter as the last large delay in the Hard Tune path. Its
declared latency is 10-30 ms, and "below 5 ms" had never been measured. Before
writing a new shifter, the shipped one and the structural limits were measured.

### What shipped (analysis only, no production code)

- **`tools/bench_pitchshift_latency.cpp`** (new target `bench_pitchshift_latency`):
  reports declared vs **effective** latency (AM-envelope cross-correlation), CPU,
  level, click stress, pitch accuracy, spectral distortion and amplitude warble
  for the shipped shifter at declared 8/10/12/20/30 ms and at 82/120/200/400 Hz,
  plus a two-tap crossfaded delay-line prototype at ~2.5 and ~4 ms.
- **`docs/low-latency-shifter-analysis.md`**: the audit, the measurements, the
  candidate families (with sources), and the Phase 3 recommendation and
  acceptance criteria. Added to the mkdocs "Development" nav.
- **`tools/render_hard_tune_ab.cpp`** (new target `render_hard_tune_ab`): renders
  one Hard Tune correction trajectory through the OLA shifter (with and without
  the preset formant colour) and through the two-tap probe (2.5 and 4 ms),
  level-matched, anonymised as `take_01..04.wav` + `key.txt` so the decision is
  made by ear on a real take. The analysis document gains the listening
  procedure and the decision rule.

### Measurements

| Config | Effective latency | CPU | Warble |
|--------|-------------------|-----|--------|
| Shipped OLA, declared 8 ms, 200 Hz | **16.71 ms** | 5.7 ms/s | 1.8 % |
| Shipped OLA, declared 10 ms, 200 Hz | **16.87 ms** | 6.5-8.5 ms/s | 1.8 % |
| Shipped OLA, declared 10 ms, 82 Hz | **29.93 ms** | 3.5-5.2 ms/s | 14.7 % |
| Shipped OLA, declared 30 ms, 200 Hz | **31.41 ms** | ~10 ms/s | 1.8 % |
| Doppler two-tap, avg 2.5 ms | **2.47 ms** | ~0.4 ms/s | 2.7 % |
| Doppler two-tap, avg 4.0 ms | **3.97 ms** | ~0.4 ms/s | 5.4 % |

### Design facts

- **The declared latency is a lower bound.** The grain is rendered forward from
  the creation sample and its window peaks half a grain later, so the true delay
  is `declared + 1.25 periods (× formant ratio) − pitch-mark alignment`. That is
  +6.9 ms at 200 Hz and +19.9 ms at 82 Hz for the declared 10 ms mode. The PDC
  declaration therefore understates the real delay by 6-20 ms depending on the
  register; this is a **separate finding** to decide on (conservative declaration
  and/or read-position compensation), not part of this analysis.
- **The architecture cannot reach 5 ms.** The grain needs half a grain of
  lookahead, so `declared >= 1.25 × min period`; at 82 Hz that is 15.2 ms before
  any margin, and the 10 ms mode already loses 5.8 dB and adds 14.7 % warble
  there (future-read degradation). Only the delay-line family has a latency
  independent of F0; every pitch-synchronous method needs at least one period.
- **A 2.5-4 ms path exists and is cheap.** The two-tap prototype measures
  2.47/3.97 ms at ~0.04 % of a core, with level, pitch and spectral metrics not
  worse than the shipped shifter at 200 Hz. The objective metrics do **not**
  measure the family's flanging/phasiness, so the prototype is an existence
  proof, not a product decision: a listening test is mandatory.
- **A/B renderer, second pass**: the first listening pass used tune-perfect
  material (both the original phrase and the recette take are equal-temperament
  takes), so the correction was only 10-20 cents of vibrato, every OLA take
  sounded like the dry take and only the Formant +2 take stood out (its wobble
  is the creative formant stage, not the tuner). The renderer now generates a
  deliberately detuned phrase (+38/-42/+27/-35/+44 cents, mean |correction|
  34.7 cents), fixes a phase bug in the phrase's vibrato, and adds
  `--fixed <semitones>` for a constant-ratio shifter stress set. Chain verified
  on a steady 224.9 Hz tone: OLA takes land on 220 Hz within 1 cent, the
  Doppler 2.5 ms probe at +7 cents. A real out-of-tune take remains required
  for the decision.
- **Latency-declaration matrix** (declared mode x fundamental, `bench_pitchshift_latency
  --latency-matrix`): the four shipped modes really run at ~17/18/21/31 ms for a mid voice
  and at ~30/33/37/34 ms for a bass voice, so the honest replacement is either the
  mid-voice set or a per-Voice-Type table (Bass 31/35/38/38, Tenor 23/24/24/36, ...).
  The matrix and the proposed values are in Section 1.3 of the analysis document; the
  four-mode ladder loses most of its meaning once the numbers are honest.
- **Recommendation on record**: a separate `LowLatencyPitchShifter` (2 taps,
  cubic interpolation, correlation-aligned splices, fixed 4 ms declared latency
  decided in `prepare()`), used only by Hard Tune; formant preservation is not
  available at that budget (bypass or tilt the knob); ratios > ~3 st route to the
  quality path. Acceptance criteria and risks are in the analysis document.

### Testing

- `cmake --build build --config Release --target bench_pitchshift_latency`: OK,
  benchmark runs to completion and its sanity gates pass (effective latency
  measurable, +1 semitone within 25 cents, level within 6 dB, Doppler faster than
  the 10 ms mode).
- No production source was touched, so the unit suite is unchanged (228 OK).
- Wall-clock CPU figures vary run to run on this machine (shared desktop); only
  the ranking is used.
## Analysis - Phase 3 verdict: no low-latency shifter

### Listening result

The author listened to both A/B sets (the detuned Hard Tune phrase and the
`--fixed 2` stress set). The two OLA takes sound transparent - the dry take with
the corrected pitch - while the two delay-line takes (2.5 ms and 4.0 ms) sound
audibly degraded: "doubled" (the two crossfaded taps comb-filter the voice,
adding a second image) and "bitcrushed" (the fractional-delay interpolation
artifacts). The earlier "wobble" on the Formant +2 take did not reappear on the
fixed (correctly phased, detuned) phrase, so the creative formant stage is not
implicated.

### Decision

**NO-GO for the two-tap delay-line low-latency path.** The family's predicted
cost is audible and unacceptable, and the gain (17 ms -> 2.5 ms) does not buy
back the quality - all the more so as the detection stage now dominates the
chain (56 ms note-change response, Phase 2). The shipped OLA shifter keeps its
role and its 10-30 ms modes; the remaining latency work is the PDC declaration
fix (analysis document, Section 1.3), not a new shifter.

### What remains open

- The PDC declaration: replace 10/12/20/30 with the honest values (mid-voice
  17/18/21/31 ms; the per-Voice-Type table for full correctness).
- The residual vibrato: the 46 ms analysis window cannot follow a 5 Hz vibrato,
  so the Hard Tune path leaves a phase-shifted vibrato residual. This is a
  detection limitation, not a shifter one, and deserves its own measurement.
- No further shifter work: a phase-vocoder path needs about 12 ms (not better
  than the OLA for this purpose), and the two-tap family is closed.
## Fix - honest PDC latency declaration

### Context

The plugin declared its internal latency-mode value (10/12/20/30 ms) to the host,
but the measured effective delay of the pitch-synchronous OLA shifter is
`internal + 1.25 periods - pitch-mark alignment`: 16.7 ms for a 200 Hz voice in
the 10 ms mode and 29.9 ms at 82 Hz. The DAW therefore aligned the other tracks
6-20 ms off, and the error changed with the sung register, so no fixed nudge
could correct it.

### What shipped

- **`Source/dsp/LatencyDeclaration.h`**: the measured effective delay per
  (latency mode, Voice Type), rounded up by 1 ms and held monotone in both
  directions (a higher mode never declares less, a lower register never declares
  less than a higher one). Universal declares the Bass floor (82.41 Hz); below
  that the declaration is optimistic and documented as such.
- **`applyLatencyMode()`** now calls `setLatencySamples()` with that table plus
  the limiter's 8-sample lookahead, instead of the shifter's internal latency.
  The applied-value cache includes the Voice Type, so changing it re-applies the
  declaration.
- **Options > Latency** shows the declared value for the current Voice Type,
  e.g. "Low Latency  (~24 ms)" for a tenor.
- The internal modes stay 10/12/20/30 ms (state compatibility); only the value
  reported to the host changes.
- New `LatencyDeclarationTest` (6 groups): monotonicity in both directions,
  Universal = Bass floor, sane range, sample conversion at 44.1/48/96 kHz,
  out-of-range clamping.
- Docs: user-guide latency table, default parameters note, analysis Section 1.3
  marked implemented.

### Design facts

- Declaring the register floor means the corrected voice is never late; notes
  higher in the register land early by up to ~10 ms. That is the unavoidable
  price of one PDC value for a pitch-dependent delay.
- The creative Formant shift grows the grain by its ratio and can add a few ms
  to the true delay; it is not in the table because the knob is not a
  PDC-stable setting.
- The two shortest modes now declare within 1-2 ms of each other: the mode count
  is a processing-character choice, not a latency ladder. Simplifying it is a
  separate product decision (it changes a choice parameter stored in sessions).

### Testing

- `cmake --build build --config Release --target OpenVoxTuner_Standalone
  OpenVoxTunerTests`: OK.
- Full suite: **234 OK / 0 FAILED** (228 before, +6 LatencyDeclaration groups,
  140 assertions).
- The runtime declaration itself is not exercised by the test target (it does
  not compile the processor); the table and the conversion are, and the
  processor wiring is covered by the build plus code review.
## Change - three latency modes

### Context

The honest effective latencies showed that the 10 ms and 12 ms modes differ by
less than 1 ms at a mid/high voice (17 vs 18 ms at 200 Hz, 13 vs 13 ms at
262 Hz) and by ~2 ms in the bass. Four user-facing choices therefore suggested a
latency ladder that does not exist; the extra entry only added a decision.

### What shipped

- **Three modes** in **Options > Latency**: **Fast** (internal 10 ms),
  **Standard** (20 ms), **High quality** (30 ms), each labelled with the declared
  latency for the current Voice Type (e.g. "Standard  (~27 ms)" for a tenor).
- **State compatibility**: the `latency_mode` parameter keeps its four values
  (removing one would corrupt old sessions and user presets). The legacy 12 ms
  value (normalised 1/3) is migrated to Fast on load in three paths:
  `setStateInformation()`, `applyPluginPresetState()` (user presets) and the A/B
  slot restore; a stale morph slot that stored 12 ms is also mapped to Fast.
- **Default** latency mode is now Fast (was Low Latency 12 ms) so a fresh
  instance never sits on a choice the menu cannot display.
- Dropping the fourth menu entry is the only user-visible change; the DSP is
  untouched.
- Translations: three new menu keys in all six languages
  (Fast/Standard/High quality); completeness test still green.

### Testing

- `cmake --build build --config Release --target OpenVoxTuner_Standalone
  OpenVoxTunerTests`: OK.
- Full suite: **234 OK / 0 FAILED** (TranslationCompleteness included, so the new
  labels are present in every locale).

### Follow-ups

- The host automation list still shows the parameter's four original labels
  ("Direct Monitoring", "Low Latency", "Quality", "Safe"). Rewriting them means
  changing the parameter definition, which breaks state compatibility; it is left
  for a future major version.
## Analysis - Hard Tune vibrato residual

### Question

The Hard Tune preset (Speed 0, Vibrato 0) promises a flat, robotic pitch. How
much of a singer's vibrato does the chain actually remove?

### Method

New `tools/bench_vibrato_residual.cpp` (target `bench_vibrato_residual`): a
synthetic 220 Hz harmonic tone with a sinusoidal vibrato (3-7 Hz, 15-50 cents)
rendered through the same chain as the plugin (YIN 2048/512 median 3, chromatic
quantisation, instant ratio, OLA shifter at 10 ms with the ratio-jump duck off);
input and output vibrato depth are measured with the same fine YIN tracker.

### Findings

- The shipped chain does not flatten vibrato. Residual (output/input depth):
  82-77 % at 3 Hz, ~100 % at 4 Hz, 125-119 % at 5 Hz, 143-138 % at 6 Hz,
  157-153 % at 7 Hz. From ~4 Hz upward the output vibrato is *deeper* than the
  input.
- The residual follows `2 * A * sin(pi * f_vib * tau)` with **tau ~= 41-42 ms**,
  the detector lag (half of the 46 ms analysis window + hop + median 3 + block
  hold). It is independent of the vibrato depth.
- The shifter's internal 290 ms f0 one-pole is not the cause: bypassing it
  (`setF0SmoothingEnabled(false)`, a new experimental API, default ON) changes
  the residual by 1-2 points only.
- With a zero-lag (ideal) detector the residual drops to 13-33 % (3-12 cents),
  so the detector lag is the dominant contributor and the achievable floor is
  the rest of the chain.
- Detector settings matter in the expected direction: median 1 improves (5 Hz:
  92 %), a 4096 window worsens (187 %).

### Options recorded (not implemented)

1. Shorten the Hard Tune detector further (1536/1024, hop 256, median 1):
   estimated tau 20-26 ms, residual at 5 Hz ~60-77 %; bass stability (82 Hz,
   1.9 periods in 1024 samples) must be measured before shipping.
2. Derive the ratio from the local period at the shifter's grain read position
   (aligned by construction, ~0 residual) - a real DSP change.
3. Delay the audio by tau (clean math, unacceptable latency).
4. Accept and document that the Hard Tune path preserves vibrato.

Recommendation: measure option 1 with the benchmark plus a listening pass; if
not convincing, option 2 as a small dedicated project.

### Testing

- Bench builds and runs; its sanity gates pass.
- Documented in `docs/hard-tune-vibrato-residual.md` (Development nav).
## Analysis - Hard Tune vibrato residual, option 1 (shorter analysis)

### Measurement

The vibrato-residual benchmark gained the option-1 detector configs and a
steady-tone detection-accuracy section. The shortest register-safe candidate is
**1536/256/median 1**: residual at 5 Hz drops from 122 % to 58-62 %, at 7 Hz from
156 % to 81-84 %, and the detector stays accurate at 82.41 Hz (0.1 cents mean,
max 5, no gross errors). A **1024** window reaches 37-38 % at 5 Hz but cannot
detect the 82.41 Hz bass note at all (decimated half-window 11.6 ms < 12.14 ms
period), so it would need a per-voice-type fallback. 1536/256/median 3 is worse
than median 1 (73 % at 5 Hz): the median itself adds lag.

### Cross-check

`render_hard_tune_ab --fast-detector` (1536/256/1) on the 5 Hz / 30 cent vibrato
tone: the OLA takes measure 16.0 cents of vibrato instead of 33.2 (dry 27.7),
matching the benchmark's ~58 % residual. Listening sets:
`build/ab_hard_tune_short/` vs `build/ab_hard_tune/`, and
`build/ab_vibrato_short/` vs `build/ab_vibrato/`.

### Decision

Recommend wiring 1536/256/median 1 as the Hard Tune detector config (mode-scoped,
register-safe, ~2x improvement) and judging it by ear; a per-voice-type 1024
window and the option-2 ratio source remain the paths to a truly flat vibrato.
Not wired into the plugin yet: the bool `fastDetector` lives in the A/B renderer,
not in the processor.

### Testing

- Bench builds, runs and passes its sanity gates.
- Renderer builds and renders both option-1 sets.
