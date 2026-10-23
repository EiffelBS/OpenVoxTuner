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
  instantaneous jump; bypassing the floor for a true snap is a separate DSP
  change, not part of this preset.

### Testing

- `cmake --build build --config Release --target OpenVoxTuner_Standalone`:
  PluginEditor.cpp compiled and the standalone linked (MSVC, Release).
- `cmake --build build --config Release --target OpenVoxTunerTests` then
  `OpenVoxTunerTests.exe`: **Result: 222 OK, 0 FAILED**.
- The factory preset table lives in `PluginEditor.cpp`, which the test target
  does not compile, so no automated test exercises the new entry; the load is
  checked by the build and by the preset-reference documentation. A manual
  recette (load the preset, read the panel values) remains to be run.
