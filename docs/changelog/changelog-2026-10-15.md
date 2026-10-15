# Changelog — 2026-10-15

## Feature — De-Esser: sibilance control on the corrected lead (`deess_*`)

### Context
A de-esser tames the burst energy of the "s", "t", "f" consonants — the 5–8 kHz
region where a vocal gets harsh, and where upward compression does the most
damage: it lifts every quiet "s" to the same level as the vowels. OpenVoxTuner had
no control for it.

The design rule from the card is a band-limited detector: **only the high band is
heard**. A full-band detector would let plosives, breaths and loud low notes duck
the whole voice, which is the classic de-esser misuse. So the module is a band-pass
front-end (2-pole high-pass in series with 2-pole low-pass around the chosen
centre), a smoothed RMS detector on that band only, fast attack / slow release,
reduction capped by a Max reduction knob, hysteresis + per-sample smoothing so
switching produces no click — the same shape as `NoiseGate`, on a narrower band.

Placement was decided by measurement, not by taste: the de-esser sits **before**
`ReverbEffect`, so it de-esses the corrected lead voice and never the room tail.
A reverb tail is decorrelated high-frequency energy; feeding it to the detector
keeps the effect engaged after the singer has stopped hissing, ducking the whole
mix, and it ducks the body of the voice through the wet signal (pumping). The
bench numbers below are the evidence.

### Changes
- **New DSP module `ovtdsp::DeEsser`** (`Source/dsp/DeEsser.h`, header-only,
  `IEffect` interface — no `juce_dsp` dependency, so the test target compiles it
  without linking `juce::BiquadFilter`).
  - **Band**: 2-pole RBJ high-pass at `centre × 0.7` in series with a 2-pole
    low-pass at `centre × 1.3`, Q = 0.7071, coefficients stored pre-normalised by
    `a0` (house convention, as `FormantPreserver.cpp`). Edges clamped to
    300 Hz … nyquist − 200 Hz with a minimum 200 Hz span; the default 7 kHz centre
    gives 4900 – 9100 Hz. Measured band gain: ≈ 0.35 at 3 kHz, ≈ 0.778 at 7 kHz.
  - **Detector**: smoothed **per-sample RMS** of the band over the first two
    channels (τ = 10 ms) — the exact `NoiseGate.h` convention. The first version
    used a rectified mean, which reads 0.637 × amplitude for a sine against RMS
    0.707 × amplitude; switching to RMS made the detector behave like every other
    level-sensitive stage in the plugin.
  - **Hysteresis**: open at `2 × thresholdLinear` (+6 dB), close at
    `thresholdLinear`.
  - **Gain**: attack 5 ms toward `10^(−maxReduction/20)`, release 80 ms toward 1,
    per-sample ramp.
  - **Bypass**: 25 ms ramp with the `bypassGain < 1e-4` early-out, so toggling the
    power never clicks; bypassed output is bit-identical to the input.
- **Four APVTS parameters** (`Source/PluginProcessor.cpp`): `deess_enable` (bool,
  default **false**), `deess_threshold` (−60 – 0 dB, default **−25**),
  `deess_max_reduction` (0 – 24 dB, default **12**), `deess_frequency`
  (3000 – 12000 Hz, 100 Hz steps, default **7000**). Raw pointers cached in
  `PluginProcessor.h`; the effects loop reads them with `wetMix = 1.0f`. The
  branch is wired in **both** effects loops — the silent pass-through path and the
  main path; wiring one only makes the two paths diverge.
- **Chain order**: `effects.push_back<DeEsser>()` **before** `ReverbEffect`
  (`PluginProcessor.cpp` L791-798). Gate → upward comp → pitch/formant →
  **de-esser** → reverb.
- **UI in the Effects block** (`Source/PluginEditor.cpp/.h`): the block grows to
  **4 columns** (width 230 → 292 px) — row 1 Gate / Compress / Reverb / Formant,
  row 2 Doubler / **De-Esser** power toggle + Threshold knob / Depth knob / Freq
  knob. The block grows by columns, never by rows, so every knob keeps its full
  size. The two knobs with no switch of their own use the Harmony caption-label
  pattern. The enable switch greys the three knobs in the 30 fps timer, exactly
  like the sibling effects; values (`dB` / `Hz`) appear only in the popup display.
- **i18n**: `label.deesser` + `tooltip.deesser_enable`, `tooltip.deesser_threshold`,
  `tooltip.deesser_max_reduction`, `tooltip.deesser_frequency` in all 6 locales
  (en **De-Esser**, fr **Désesseur**, de **De-Esser**, es **Dessersor**, ja
  **デエッサー**, zh **齿音消除器**). Bytes verified as strict UTF-8, no BOM, no
  replacement characters.
- **Persistence**: `MorphState` fields (`deessEnable`, `deessThreshold` = 0.5833333,
  `deessMaxReduction` = 0.5, `deessFrequency` = 0.4444444 — normalized, as every
  APVTS value in the morph), capture, `applyInterpolatedState` (lerp + `lerpOrStep`
  for the boolean), `getMorphParameterIds()`, and the explicit A/B slot restore in
  `loadSlot` (`setSlider` × 3 + `setChoice`).
- **Factory plugin presets**: the presets that add top-end energy switch the
  de-esser on — `studioStack` −30 dB / 9 dB / 7 kHz, `spokenPodcast` −28 dB /
  10 dB / 7.5 kHz, `broadcastVoice` −20 dB / 16 dB / 6.5 kHz, `leadVocal` −25 dB /
  12 dB / 7 kHz; `correctionPure` and `transparentDub` set `deess_enable = 0`
  explicitly (their names promise no dynamics on the lead). `liveScene` and
  `breathyVocals` keep the default off.

### Placement bench
`tools/bench_deesser_placement.cpp` (new console target in `CMakeLists.txt`) plays a
synthetic voice — a 180 Hz harmonic stack with 7 band-limited sibilant bursts —
through three chains and measures the output: sibilance RMS (5.5 / 6.5 / 7.5 kHz
band-pass bank), total RMS, the same two over the last 400 ms of silent input
(the tail), a body band (400 / 800 / 1600 Hz) in the tail, and the peak-to-trough
swing of the de-esser gain in the tail window. Settings: threshold −35 dB, max
reduction 12 dB, centre 7 kHz, reverb room 0.4 / damping 0.6 / wet 0.3.

```text
chain     sibilance   total    tail sibilance  tail body    tail gain swing
bypass      +0.00 dB    +0.00 dB     +0.00 dB       +0.00 dB    0.0000
before     -8.78 dB    -5.01 dB    -10.52 dB       -4.28 dB    0.0451
after      -8.76 dB    -6.48 dB     -4.81 dB       -4.60 dB    0.1043
detector peaks (band RMS, dBFS): voice -36.95  burst -21.24  tail -82.85   close -35.00 dB  open -28.98 dB
PLACEMENT GATES PASS
```

Reading: the shipped order ("before") cuts the burst sibilance by 8.78 dB, cleans
the **tail** harshness 5.71 dB deeper than the alternative (−10.52 vs −4.81), keeps
1.47 dB more loudness, and pumps the tail 2.3× less (gain swing 0.0451 vs 0.1043).
The voice body alone peaks at −36.95 dBFS, below the −35.00 close point, so a loud
low note never triggers the detector; the bursts peak at −21.24, above the −28.98
open point. The tail detector peak is −82.85 dBFS: with the de-esser before the
reverb, the room never feeds the detector.

### Tests
New `test/dsp/DeEsserTest.cpp`, registered in `test/Main.cpp`: 8 groups /
1039 assertions, on the `UpwardCompressorTest` motif (deterministic tones at
44.1 kHz, 512-sample blocks):
1. **Bypass bit-identical** — 1024 assertions: de-esser off leaves the buffer
   exactly as it came in.
2. **Band isolation** — a 3 kHz tone is not de-essed (band gain ≈ 0.35, detector
   stays under the close point).
3. **Reduction depth** — a hot band tone ducks to the Max reduction floor
   (expected RMS = amplitude × 0.7071 × 0.25119).
4. **Hysteresis** — between the close and open levels the gate stays shut.
5. **Stereo link** — the loudest channel ducks both channels equally.
6. **Release** — silence lets the gate open again.
7. **Click-free power toggle** — applied gain continuity `< 0.01` per sample, plus
   a sample-continuity bound of 0.80.
8. **Disabled restores full level** — the tone returns to its input level.

Two bugs the tests caught:
- The first cascade wrote the high-pass and the low-pass coefficients onto the
  **same** 2-pole section, so the band was a low-pass at 9100 Hz only: a 3 kHz
  tone passed at ~1.0 and engaged the detector (measured outRms 0.0533 = 0.2121 ×
  floor). Fixed by splitting `BandSection` into `Section high, low` in series;
  post-fix probe: 3 kHz detector 0.0638717 / gain 1 / engaged 0, 7 kHz detector
  0.413414 / gain 0.251192 / engaged 1.
- The click test initially measured **output sample delta**, which is meaningless on
  a 7 kHz sine: its natural slope is `2·A·sin(πf/sr)` ≈ 0.765 (probe printed
  maxDelta 0.764861 against sineMaxDelta 0.797865). The test measures the
  continuity of the **applied gain** instead.

Build and suite results: full suite in one process — **133 groups OK / 0 FAILED**,
exit 0, including `DeEsser` (8 groups). The plugin SharedCode target builds clean in
Release; `bench_deesser_placement` passes all gates with exit 0.

### Notes
- Windows `min` / `max` macros break `juce::max(...)` inside test code (`C2589` +
  `C2059`); plain `if (d > maxDelta) maxDelta = d;` is the safe form.
- A failed build leaves the **stale** test executable in place: always read the
  build tail before running it, otherwise a green run proves nothing.
- Deferred by design: a de-esser sidechain input (external detector reference),
  which belongs to the 3-bus architecture work; and a split-band mode that ducks
  only the band instead of the whole voice.
