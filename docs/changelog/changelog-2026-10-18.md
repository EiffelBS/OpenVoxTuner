# Changelog — 2026-10-18

## Feature — Tempo-synced stereo ping-pong Delay on the lead bus, driven by the first combo of the effects row

### Context

No verbatim user quote: this is a feature addition, not a bug report. The gap it closes is stated in
`docs/implementation-roadmap.md` and `docs/reference/dsp-api.md`: the two split rooms made the lead intelligible
and the pad wide, and the reference already warned that one shared insert "would force one room on both and wash
the tempo-sync delay copies" — a copy effect was planned for exactly this slot, and the ARA2 tempo extraction had
been sitting **unconsumed by DSP** ("Experimental — not yet consumed by DSP", roadmap line 42 verbatim).

The stage is a **tempo-synced ping-pong delay on the LEAD bus**, ordered **DeEsser → Lead room → Delay**: an
additive send, so the corrected lead keeps its full dry level and the echoes add on top; fed to the shared room
through the existing lead-send tap (`applyLeadSend` runs after `leadBus.apply`, which is why a delay insert lands
ahead of the send), so a short slap-back and the ping-pong tail ring in the same space as the voice.

Design facts, all pinned from the shipped `Source/dsp/DelayEffect.h` / `.cpp`:

- **Declared latency is unchanged — zero for this stage.** Nothing calls `setLatencySamples`; the only live call
  site stays `applyLatencyMode()`'s `pitchShifter->getLatencySamples() + outputLimiter.getLatencySamples()`.
  Declaring the copy length as plugin latency would put the whole voice behind the echo.
- **Additive send, dry at unity**: `out = in + bypassGain * (mix * wetMix) * tail`, never a crossfade. A knob
  wired as "dry + wet" is what makes an echo read as "inaudible at full mix"; here only the send contribution
  mixes. With `delay_enable` off and `bypassGain` decayed below `kBypassFloor = 1e-4`, the output buffer is left
  **bit-identical** while the lines keep feeding, so re-enabling resumes on the current timeline.
- **Division indices map to beat fractions, not to seconds**: `{0.5, 0.75, 1.0, 1.5, 2.0}` beats for labels
  `{"1/8","1/8.","1/4","1/4.","1/2"}`, so `copySamples = round((60/bpm) * fraction * sr)`. At 120 BPM the five
  indices are **12000 / 18000 / 24000 / 36000 / 48000** samples, and halving the tempo doubles the copy.
- **A copy-length change is a two-tap crossfade over 25 ms** (`kRampSeconds`), not a ramped read pointer. The tap
  being read is frozen at `crossCopySamples` and the new distance is faded in. Ramping the pointer itself was the
  first implementation and it glides the pitch: a 1/4 → 1/2 change moves 12 000 samples in 1 200 steps, i.e. 10
  samples per step, measured as a **0.107** worst sample-to-sample jump on sustained material — audible as a zip.
  The crossfade measures **0.0058**.
- **Clamped to [floor, 6 s]**: floor = `max(1, round(0.04 * sr))` = 1920 samples at 48 kHz, ceiling from
  `maxCopySeconds() = (60/20) * 2 = 6 s`; `prepare()` sizes both lines as the next power of two ≥
  `maximumBlockSize + maxCopySamples + 1` (524 288 samples at 48 kHz / 512), so `process()` allocates nothing.
- **Ping-pong is cross-coupling, L→R and R→L, input included**: the input enters the *opposite* line and the tails
  cross, which makes the cycle R → L → R one copy apart per pass. Feeding each line its own tail (plain feedback)
  and writing the input straight into its own side is the shape that fails: the first repeat then lands on the
  source's side and the second arrives one copy late. A centred voice therefore bounces from the first repeat on.
- **DC is blocked by construction**: each line's feedback path runs through `OnePoleLowCut` at **150 Hz** — a pole
  at DC, so an offset drains inside the loop instead of being stored.
- **`reset()` re-targets the copy for the current bpm/division and seeds the read there.** Seeding at the comb
  floor instead pins the stage at 40 ms for the whole session: hosts call `reset()` from `prepareToPlay()`, and
  only a later `setBpm`/`setDivision` sets the dirty flag. This was the second defect the first green run caught.

Parameters, ids verbatim as attached by the UI (`Source/PluginEditor.cpp`): `delay_enable` (bool, default off),
`delay_division` (choice {"1/8","1/8.","1/4","1/4.","1/2"}, default index 2 = "1/4"), `delay_mix` (0..1, default
**0.30**), `delay_feedback` (0..1, default **0.35**). The wiring hands the effect `wetMix = 1.0` and drives the
echo level through `setMix (delay_mix)`, so the mix knob is the effect's own, not a bus crossfade.

ARA tempo: when an ARA source is bound (`hasAraTempo()` true) the stage takes `getAraTempo()` as its BPM; with no
bound source it runs on the host/standalone tempo register (`bpm.load()`, clamped 20..400 inside the effect), so
the same knob keeps musical meaning in both paths and extraction stops being "Experimental, not consumed".

### Changes

- **`Source/dsp/DelayEffect.h` / `.cpp`** (new): `ovtdsp::DelayEffect : public IEffect`, `getId() = "delay"`,
  `getName() = "Delay"`; `setBpm(float)` / `setDivision(int index)` / `setMix(float)` / `setFeedback(float)`
  (clamped ≤ 1.0); diagnostics `getCopySamples()`, `getTargetCopySamples()`, `getLineCapacity()`, `isBypassed()`.
  Two delay lines with cross-coupled feedback and cross-fed input, per-channel `OnePoleLowCut` at 150 Hz inside the
  loop, fractional read-out with linear interpolation, two-tap crossfade for copy changes, one-pole enable gain
  (`bypassGain`). The header pins the contract the tests assert: additive send with the dry path at unity, zero
  declared latency, `reset()` re-targeting the current division.
- **`Source/PluginProcessor.cpp` / `.h`**: the four parameters registered in the APVTS next to the de-esser and
  saturation blocks, with `delayDivisionChoice = dynamic_cast<juce::AudioParameterChoice*> (...)` read through
  `getIndex()` (the house rule for choices; `round(load() * n)` is fragile after a non-normalised state restore).
  `leadBus.addEffect (std::make_unique<ovtdsp::DelayEffect>())` is registered **last** (DeEsser → Lead room →
  Delay), and `applyLeadEffects` gains the `"delay"` branch: `setBpm (hasAraTempo() ? getAraTempo() : bpm.load())`,
  `setDivision (index)`, `setMix`, `setFeedback`, `enabled = delay_enable > 0.5f`, `wetMix = 1.0f`. The sleep path
  needs no new code — `applyLeadEffects` on the cleared buffer drains the stage. No latency declaration is touched.
- **`Source/dsp/PresetMorpher.h`**: `MorphState` gains `delayEnable` / `delayDivision` / `delayMix` /
  `delayFeedback`; capture snaps the division with `round(value * 4)`, the whitelist carries all four ids, mix and
  feedback lerp, and the enable switch and the division step at 50 % (`lerpOrStep`), the division scaled by 4.
- **`Source/PluginEditor.cpp` / `.h`** (first combo of the effects row): the effects frame gains a **5th column**
  when open (width 5 columns / 4 gaps, `numEffectCols = effectsOpen ? 5 : 2`, folded stays 2): the enable toggle
  carrying the name, a Division label + combo strip, then Mix and Feedback labelled knobs. Attachments for all
  four ids, grey-out of the combo and both knobs when `delay_enable` is off, a `delay_mix` entry in the MIDI Learn
  menu, the slot restore lines (`setChoice`/`setSlider` for the four attributes), and `delay_enable = 0` in every
  factory preset map. The theme-list passes also pick up the de-esser and saturation entries that were missing.
- **`Source/ui/OVTLanguages.h`**: `kLabelDelay`, `kLabelDelayDivision`, `kLabelDelayMix`, `kLabelDelayFeedback`
  and `kMidiLearnDelayMix`, translated in all six locales (en / fr / de / es / ja / zh).
- **`CMakeLists.txt`**: `Source/dsp/DelayEffect.cpp/.h` registered on the plugin target and the effect's TU added
  to the tests target, which drives it directly.
- **`Source/PluginProcessor.cpp` A/B slots**: `delayEnable` / `delayDivision` (int) / `delayMix` / `delayFeedback`
  written in the slot XML and read back with the parameter defaults (off, 2, 0.30, 0.35), in the de-esser and
  saturation template, so an old slot reads as off at the defaults.

### Tests

`test/dsp/DelayEffectTest.cpp` (new suite, registered in `test/Main.cpp`, helpers `dvx`-prefixed because Main.cpp
is one translation unit). Runner output, verbatim:

```
A: disabled stage touched blocks [-1..-1] out of 32, decay budget ends at block 24
B: idx 0 fraction 0.5 target 12000, echo onset at offset 12001, delta 1
B: idx 1 fraction 0.75 target 18000, echo onset at offset 18001, delta 1
B: idx 2 fraction 1 target 24000, echo onset at offset 24001, delta 1
B: idx 3 fraction 1.5 target 36000, echo onset at offset 36001, delta 1
B: idx 4 fraction 2 target 48000, echo onset at offset 48001, delta 1
C: 60 BPM quarter copy target 48000, echo onset at offset 48001
D: feedback 0.5, echoes e1=0.999964 e2=0.490261 e3=0.240364, ratios 0.490278 / 0.490278
D0: feedback 0 single echo peak 0.999964, L-side RMS past the echo window 0
E: L input echo windows [L,R] = [0, 0.0795467] [0.0313389, 0] [1.26117e-44, 0.0123858]
E: R input mirror windows [L,R] = [0.0795467, 0] [0, 0.0313389]
F: biggest sample-to-sample jump across the division ramp: 0.00575938
F: arrival peak in the new-copy window 0.399924, biggest peak after the settle window 0
G: worst |mean| over 4096-sample windows 0.0230503, first window peak 0.4, last window peak 0.679981
H: bpm 400 idx0 target 3600, floor 1920, echo onset at offset 3601
H: bpm 20 idx4 target 288000, prepared capacity 524288
H: bpm 20 idx4 biggest |sample| over a 3 s run: 0.8
H: mono input echo energy L 0.490261, R 0.999964
R: tail left at the end of the run 0.867715, peaks after reset over 10240 silent samples L 0 R 0
```

```
Result: 190 OK, 0 FAILED
```

What the suite covers, and what the numbers above prove:

- **Bit-identical bypass**: a disabled stage writes nothing once the enable ramp and the tail have decayed
  (budget from `round(0.025 * sr)` = 1200 samples at 48 kHz), reports `isBypassed()`, and the `wetMix = 0` path is
  a no-op over 40 blocks.
- **Copy mapping for all five divisions at 120 BPM**: the target equals `round(fraction * (60/120) * sr)` exactly,
  and the L-input echo lands in R at that offset ±3 samples — measured delta **1** sample on every index, which is
  also the zero-latency proof (the dry impulse stays on its own sample).
- **Tempo sync**: at 60 BPM the quarter copy is exactly 2× the 120 BPM one and the arrival follows it.
- **Feedback law**: successive repeat ratios follow the knob (e1 = 0.99996, e2 = 0.49026, e3 = 0.24036 → ratios
  0.4903 / 0.4903 for feedback 0.5, inside the deliberate 0.25..0.85 band because each pass also carries the
  150 Hz low-cut); feedback 0 leaves a single echo with the L side silent past it (RMS 0).
- **Ping-pong image**: L input alternates R → L → R by a factor ≥ 3 per window, and an R-only input mirrors it.
- **Copy-change smoothness**: idx2 → idx4 at 120 BPM on sustained tone moves at most **0.0058** per sample across
  the crossfade, and the new copy has settled inside the ~35 ms window (nothing louder after it).
- **DC / sub-bass guard**: a 60 Hz tone at feedback 0.9 keeps a running mean of 0.023 per 4096-sample window and
  never explodes — the pole in the loop drains the offset instead of storing it.
- **Clamping and robustness**: bpm 400 × idx0 = 3600 samples stays above the 1920 floor with a measurable echo;
  bpm 20 × idx4 = 288000 samples fits the prepared 524288 capacity; every sample is finite and below 3.0 over a
  3 s run; the dry sample is bit-identical (so nothing wrote out of range); a mono input produces echo energy on
  both sides; `reset()` empties both lines (tail 0.868 before, 0 afterwards).

The first green-run attempt found and fixed three real defects, all now locked by these assertions: a
`juce::SmoothedValue` API misuse (`nextValue()` instead of `getNextValue()`), `reset()` seeding the copy at the
comb floor for the rest of the session, and the feedback topology above (own-tail feedback instead of a crossed
pair, which produced 0.490 → 0 instead of an alternating series). Three test windows were also mis-calibrated
(they measured to the end of the recording, so a legitimate impulse edge or a repeat beyond the window read as a
failure); they now target the arrival they assert.

### Docs

- `docs/implementation-roadmap.md`: ARA tempo line flipped from "not yet consumed by DSP" to consumed by the lead
  delay, plus a full entry for the stage (chain order, division fractions, crossfade, clamps, the four ids).
- `docs/reference/dsp-api.md`: new `ovtdsp::DelayEffect` section — additive send, zero latency declaration, fed to
  the shared room by the existing lead-send tap, two-tap crossfade for copy changes, DC blocked by the 150 Hz
  low-cut in the loop, diagnostics.
- `docs/architecture/dsp-pipeline.md`: the LEAD chain and the effects-bus table now read
  `DeEsser → Lead room → Delay`, with the parameter list and the rationale paragraph.
- `docs/testing.md`: the suite added to the tree and to the coverage paragraph.
