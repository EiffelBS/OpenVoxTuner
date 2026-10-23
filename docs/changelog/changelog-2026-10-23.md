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
  declares 10–30 ms and keeps a ~4.5 ms per-sample ratio smoother. Those are
  Phase 2 (shorter analysis window/hop, bass caveat) and Phase 3 (a low-latency
  shifter) of the plan; they are not part of this change.
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
