# Changelog — 2026-10-14

## Feature — Stereo Doubler of the corrected lead (`double_amount`)

### Context
A "double track" is the standard studio thickening of a lead vocal: one extra
copy of the same performance, slightly detuned, arriving a few milliseconds
late and panned to the opposite side. OpenVoxTuner had no such control — the
Harmony engine adds *interval* voices, and the Vocal Stack / Unison types add
voices from the harmony bus, which is not what a doubler is: a doubler must
follow the **corrected lead** exactly, in time, and stay audible when Harmony
is off.

Design constraints taken from `docs/harmony-pad-prerequisites-analysis.md`
(keep detunes below 10 cents) and from the Haas precedence rule (offset above
5 ms so the copy reads as width, not as an echo): a doubler added on top of
the dry signal raises the level, so the perceived loudness must be held
constant, and the L+R sum must not develop a phase hole.

### Changes
- **New DSP module `ovtdsp::Doubler`** (`Source/dsp/Doubler.h` + `.cpp`,
  registered in `CMakeLists.txt` for the plugin and for the tests). A
  two-channel fractional read-write delay line sized for 25 ms plus block
  headroom, with per-sample ramps for amount, copy level, pan gains and delay
  position, and an amount smoothing one-pole (τ = 60 ms).
  - `detuneCents(p) = p · 0.10` → 0 to 10 cents sharp; `detuneRatio(p) = 2^(cents/1200)`.
  - `haasDelaySamples(p, sr)` → up to 25 ms, floored at 5 ms once the knob is above zero. The offset is independent of `latency_mode`.
  - `copyLevel(p) = (p/100) · 0.6` → max −4.4 dB, never above the lead.
  - `gainCompensation(p) = 1 / √(1 + g²)` applied to the whole lead path, **independent of `harmony_gain_match`** (the doubler adds to the dry signal, so with match off the sum can exceed unity).
  - `panGains()` reuses the plugin pan law (angle `(pan+1)·π/4`, cos/sin, unity at center); the copy is spread to `+amount`, i.e. opposite to the centred lead. The copy is applied as `g · gL` / `g · gR`, so with `gL² + gR² = 2` the mean copy power is exactly `g²` and the compensation keeps the perceived RMS constant.
- **Dedicated second PitchShifter instance** (`doublerPitchShifter`,
  `Source/PluginProcessor.h`). Prepared in `prepareToPlay` with the same
  attack (30 ms) and the same latency (`setLatencyMs` inside the
  `applyLatencyMode()` loop, so it stays in sync when the mode changes). It is
  fed from the lead **after** `pitchShifter->process`, so the copy follows the
  corrected pitch, ratio and formant. The main shifter is never re-entered:
  the existing comment at `PluginProcessor.cpp` L2443-2447 forbids a second
  `process()` call in the same block (re-entry corrupts the grain scheduler
  state → scratchs / NaN). The copy ratio is `ratio · 2^(cents/1200)` clamped
  to 0.25–4.0, and `resetSoft()` runs on the 0 → active edge so the grain
  scheduler starts clean.
- **Core block placement** (`Source/PluginProcessor.cpp` L2476-2525): the
  doubler sits on the lead path *after* the lead shift and *before* the
  harmony mix, so it doubles the lead only — not the harmony voices (the
  effects chain acts on the final mixed buffer, so a post-mix doubler would
  have doubled the harmonies too).
- **Parameter `double_amount`** (float, `NormalisableRange(0, 100, 1)`,
  default **0**): APVTS entry, raw pointer cached, reset in `reset()`, written
  and read in the A/B slot XML, added to `MorphState` + snapshot + morph
  whitelist + lerp.
- **UI knob in the Harmony block** (`Source/PluginEditor.cpp/.h`): knob with a
  `%` readout, title label "Double", tooltip `tooltip.double_amount`, preset
  restore, slider colours. It is deliberately **not** disabled by
  `isHarmonyEnabled` — the doubler stays active when Harmony is off (a comment
  next to the harmony enable logic records that decision). The knob joins the
  adaptive 3-row harmony column, which shrinks the existing harmony knobs from
  44 px to ~36 px.
- **i18n**: `tooltip.double_amount` in all 6 locales (en, fr, de, es, ja, zh)
  in `Source/ui/OVTLanguages.h`; bytes verified as strict UTF-8, no BOM, no
  replacement characters.
- **CPU short-circuit**: at amount 0 the shifter is skipped entirely and the
  delay line only advances its write position (so a re-activation reads
  silence, not stale audio). Output is bit-identical to the pre-feature
  plugin, which keeps every existing preset unchanged.
- **Docs**: `double_amount` row in `docs/default-parameters.md`; "Stereo
  Doubler" section + controls row + a quick recipe in
  `docs/user-guide/harmony.md`; `test/dsp/DoublerTest.cpp` added to the suite
  tree in `docs/testing.md`; roadmap entry 8t.

### Tests
New `test/dsp/DoublerTest.cpp`, registered in `test/Main.cpp`. 4 groups,
47 assertions, on inharmonic multi-tone + noise (deterministic LCG):
1. **Amount mapping** — cents cap at 10, exact ratios (1.005827 at 100 %,
   1.002893 at 50 %), Haas floor 5 ms and 25 ms at 100 %, copy level ≤ 1 and
   0.6 at 100 %, pan law endpoints (center → 1,1; full right → 0,√2).
2. **Constant perceived RMS** — stereo RMS within ±2 dB of the amount-0
   reference for amounts 25 / 50 / 75 / 100 % (44.1 kHz, 256-sample blocks).
3. **Mono compatibility** — L+R sum within −1 dB of the reference for
   25 / 50 / 100 %, and all samples finite on a mono bus.
4. **Amount-0 bypass** — `isActive()` false, 8 blocks bit-identical
   (0 mismatches), then reset + 100 % is active and the copy path reads
   silence after the reset.

Build and suite results: `OpenVoxTunerTests`, the plugin SharedCode target and
`OpenVoxTuner_VST3` all build in Release. The full suite runs green in a single
process: **120 groups OK / 0 FAILED**, exit 0, including `Doubler` (4 groups)
and `MidiImporter` (14 groups).

The `MidiImporter` crash reported during this work is fixed. Two causes, both
outside the doubler: the test binary's image sits inside the workspace, so it
runs at **Low integrity** (`S-1-16-4096`) and cannot write to `%TEMP%` (Medium)
— every group needing a temp `.mid` failed on "Cannot open file"; the suite then
indexed an empty `PitchCurve`, and `getReference()` has no bounds check in
Release, so reading past the end was an access violation.
`test/dsp/MidiImporterTest.cpp` now writes its scratch files next to the
executable (`ovt_midi_scratch`, inside the ignored build tree) and guards every
point read against the curve size. `test/Main.cpp` installs a stdout logger:
with no logger installed, JUCE sends `Logger::writeToLog` to
`OutputDebugString`, which is why the crash printed nothing but "main start".
The last line printed before the process dies is now the crashing group, with no
memory dump required.

### Notes
- `UnitTestRunner::runTests()` clears its results at the start of every call, so
  passing several suite names on the command line prints only the **last**
  suite's results: multi-name runs detect crashes but hide the assertion
  failures of earlier suites. Verify each suite individually and check the exit
  code.
- The first stereo implementation applied the pan gains without the copy
  level, so the copy exceeded the lead (+2.8 dB at 25 % and 50 %); multiplying
  `g · gL` / `g · gR` fixed it and made the compensation formula exact.
- Deferred by design: detuning the Unison voices through
  `shiftedVoiceRatioSmoothers`, and a stereo width parameter on the harmony
  bus gains. Both belong to the 3-bus architecture work.
