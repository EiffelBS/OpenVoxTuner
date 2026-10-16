# Changelog — 2026-10-16

## Feature — Three effects buses (lead / harmonies / master) + output trim & limiter

### Context

The plugin had **one** effects list applied to the final mixed buffer, so a single
`ReverbEffect` had to serve two roles with opposite needs. The mix formula in
`ReverbEffect` is `dry * (1 - mix) + wet * mix`: raising the wet mix attenuates the dry
signal by the same amount. A lead voice needs a short, centred room for intelligibility;
a 4-voice pad needs a long, wide room for amplitude. One shared room forces one size on
both, and a long room on the shared bus washes out the lead — and any tempo-synced delay
copy that comes later (#132), because the reverb tail is decorrelated energy fed back
into everything.

The design rule adopted is **the bus IS the role**. Effects stay in-place processors
(`ovtdsp::IEffect`, no send/return architecture, no new interface); what changes is
*where* they are applied and *which parameters drive them*. Three tap points in
`processFx`, each with its own `ovtdsp::EffectsBus` owning its own instances, so the same
class can run twice with different fixed settings. `ReverbEffect::getId()` stays
`"reverb"`: the bus, not the id, decides which parameter pair drives it.

This is also the pre-work for #130 (de-esser), #131 (doubler) and #132 (tempo-sync
delay): each of them needs a defined place in the chain, and "on the lead", "on the
harmonies" and "on the master" are those places.

### Changes

- **New `ovtdsp::EffectsBus`** (`Source/dsp/EffectsBus.h`, header-only, so no CMake change):
  `addEffect()`, `prepare()`, `reset()`, `apply(buffer, wiring)`, `size()`. The wiring is
  `std::function<bool (IEffect&, bool& enabled, float& wetMix)>` — it computes the enable
  flag and wet mix for one effect from that bus's parameters (and may configure the
  instance), returning `false` to skip it for this block. Effects run in registration
  order, in place.
- **Three tap points wired in `processFx`** (`Source/PluginProcessor.cpp`):
  `applyLeadEffects(buffer)` right after the doubler block, on the corrected lead **before**
  any harmony is added; `applyHarmonyEffects(harmonyBuffer)` **before** the harmony mix;
  `applyMasterEffects(buffer)` **after** the blend. The first step was behaviour-preserving:
  the unit test proves an empty bus passes the buffer through bit-identically, and that
  running all effects on the master reproduces the legacy single-loop dispatch exactly.
- **Reverb split into two instances**: lead instance room size 0.35 / damping 0.50 / width
  0.5 (short, centred); pad instance room size 0.85 / damping 0.40 / width 1.0 (long,
  wide). New `ReverbEffect::setStereoWidth(float)` alongside the existing `setRoomSize` /
  `setDamping`, which previously had no call sites and now have real ones. Precedent for two
  instances of one DSP on two buses: `formantPreserver` + `formantPreserverHarmony`.
- **Four APVTS parameters** (`Source/PluginProcessor.cpp`): `pad_reverb_enable` (bool,
  default **false**), `pad_reverb_mix` (0–1, default **0.30**), `output_trim`
  (**−12 .. +12 dB**, 0.1 steps, default **0**), `limiter_enable` (bool, default **true**).
  Raw pointers cached in `PluginProcessor.h`. `reverb_enable` / `reverb_mix` are kept as the
  **lead** reverb, so existing saved states and factory presets map cleanly and no parameter
  is orphaned.
- **Gate-envelope ducking**: the lead reverb's wet mix is multiplied by
  `noiseGate.getCurrentGain()`, which returns 1.0 when the gate is off, so the bypass stays
  neutral. The pad is deliberately **not** extra-ducked: the harmony mix downstream is already
  multiplied by the smoothed gate gain, so ducking the pad room a second time would double-dip.
- **New `ovtdsp::OutputLimiter`** (`Source/dsp/OutputLimiter.h`, header-only, not an
  `IEffect`): output trim in dB (clamped −12..+12) then a peak limiter with a linked-channel
  one-pole envelope — 0.7 ms attack, 120 ms release — and `ceilingLinear = 0.98`
  (~ −1.68 dBFS). Applied **last** inside `applyMasterEffects`. Deliberately
  **non-lookahead**: the gain at sample *i* is computed only from samples ≤ *i*, so the stage
  adds **no latency** and `setLatencySamples` is never touched (the dynamic-latency trap that
  produced the 576↔960 ping-pong in Live 12). The trim scales the final result only, leaving
  the harmony `1/sqrt(1+N)` gain-match intact.
- **Release-envelope bug caught by the test**: the first version wrote
  `env += releaseCoeff * (env - scaled)`, which moves the envelope **away** from the target
  (exponential divergence) instead of toward it; corrected to `(scaled - env)`. The monotonic
  release test is what exposed it.
- **Persistence**: A/B slot XML gains `padReverbEnable`, `padReverbMix`, `outputTrim`,
  `limiterEnable` attributes, each read with a default (HC.11 rule: every attribute read needs
  a default, so an old slot file loads cleanly). Morphing: the four ids are in
  `captureState`, `getMorphParameterIds()` and `applyMorph` (`lerp` for the floats,
  `lerpOrStep` at 50 % for the bools). Factory presets: `studioStack` pad mix 0.35,
  `leadVocal` pad mix 0.30; trim/limiter entries deliberately not added (defaults true / 0 dB).
- **UI in the Effects block** (`Source/PluginEditor.cpp/.h`): the block grows to **4 columns**,
  never a third row — column 3 = **Pad** (power toggle + mix knob), column 4 = **Limiter**
  (power toggle + Output Trim knob printing `" dB"`). The frame width model (`effectW`) and
  `numEffectCols` are both driven by the column count, so the folded frame keeps its two-cell
  width. Output Trim is deliberately NOT greyed out by the limiter switch. i18n × 6 locales
  (labels, tooltips, MIDI-learn name), undo-gesture listeners, theme colours, `loadSlot`
  restores the four values.

### Tests

- **New `test/dsp/EffectsBusTest.cpp`** (10 tests, registered in `test/Main.cpp`), built on a
  deterministic `GainEffect` whose output can be computed in closed form — no RNG, no host state:
  - an empty bus leaves the buffer **bit-identical**;
  - a wiring that returns `false` skips the effect (call count 0) and passes the buffer through;
  - running all effects on the master bus is **bit-identical** to the legacy single-loop dispatch,
    including the `1/sqrt(1+3)` harmony gain — the refactor is proven sound, not asserted;
  - a lead tap scales the lead only, and each instance is called exactly once per block in
    registration order;
  - **non-lookahead**: every sample before a spike passes at unity gain, the spike lands on the
    ceiling within 1e-5, the release is monotonic, and the peak never exceeds the ceiling;
  - trim −6 dB is an exact gain with the limiter off;
  - channels are linked: a quiet channel is ducked by `ceiling / 1.40` because of its loud neighbour;
  - a silent buffer stays **exactly** zero and the envelope stays under 1e-3;
  - an impulse at index 100 stays at index 100 — nothing before it is touched, no smear.
- Suite: **146 OK / 0 FAILED**. Plugin target (`OpenVoxTuner`) builds green.

### Notes

- The master bus is intentionally **empty** today. It is the home for shared sends to a common
  reverb (the card that adds them waits on this one) and for tape saturation as master glue (#135).
  *(Filled later the same day by the soft saturation stage documented below — the send room arrived first,
  so this line was true when it was written.)*
- Deferred, not implemented here: ducking an effect return by the gate envelope, a shared
  `DelayLine` primitive for chorus/flanger/phaser.
- Observed gap, outside this scope: the A/B slot XML carries no `deess_enable` / `deess_amount`
  attributes (left over from #130), so the de-esser is not stored in slots.

## Feature — Soft saturation on the master output (`saturation_drive` + `saturation_tone`)

### Context

The master bus built yesterday was left empty on purpose: it is the home for glue. This card fills
it with a soft clipper and a warm corner, named **Drive** and **Warmth**. The naming rule is part of
the delivery: never "tape", because nothing here promises tape behaviour — no hysteresis curve, no
compression, no head-print. A name that over-promises is a defect, not a marketing choice.

Three constraints decided the design before any code was written:
1. **Aliasing is the only real DSP risk.** `tanh` on a voice at 50 % drive or more generates energy
   above Nyquist, which folds back as inharmonic mud. So the shaper runs at 2x oversampling.
2. **Drive 0 must be a transparent bypass, not a shaper value.** `tanh(x)/tanh(1)` is not linear — an
   amplitude-1 sine loses −3.3 dB through it — so drive 0 short-circuits the stage entirely.
3. **Makeup must be measured, not estimated.** The loudness added by a clipper is not the small-signal
   gain; guessing it produces a knob that makes the track louder than the bypassed signal.

### Changes

- **New `ovtdsp::SaturationEffect`** (`Source/dsp/SaturationEffect.h`, 397 lines, header-only, so no
  CMake entry; included from `PluginProcessor.h` next to `DeEsser.h`). Shaper
  `y = tanh(k x u) x shaperScale` with `k = 1 + 9 x drive` and `shaperScale = makeup / k`, which keeps
  unity gain for quiet signals at every `k`: the knob changes colour, not level.
- **2x oversampling through a polyphase half-band pair**: Blackman-windowed sinc at ω = π/2 in the 2fs
  domain, **L = 33** taps normalized to Σh = 1 (measured before shipping: centre tap 0.4999686,
  Σ interpOdd 1.0000627, Σ decimOdd 0.5000313 — DC in gives DC out at unity; stopband −68 dB beyond
  1.6x). L = 25 measured only −39 dB and was rejected. Ring buffer of 32 samples per channel, group
  delay **16 samples** at the base rate, no plugin latency declared.
- **Warm corner**: one-pole low-pass on both shaped streams at the oversampled rate,
  `coeff = 1 − exp(−2π fc / 2fs)`, fc from 8000 Hz (warmth 0) to 5000 Hz (warmth 1), smoothed once per
  block by `BlockAwareOnePole` (tau 60 ms, prepared at the base rate). Measured −3.38 dB at 5400 Hz with
  warmth 1 against theory −3.2 dB. One pole state per channel, walked in stream order.
- **DC blocked by construction**: subtractive one-pole high-pass near 20 Hz after decimation
  (`OnePoleLowCut` per channel). Residual ≤ 2.2e−4 on a sine and on a ramp.
- **Headroom for ARA buffers** (which can exceed ±1): input clamped to ±4, then compressed above full
  scale as `u = x x 2 / (1 + |x|)` — continuous at unity, monotonic, so a hot master enters the curve
  soft.
- **Bypass**: gate `enabled && drive > 0`, 25 ms crossfade computed per block as
  `1 − exp(−(numSamples/fs)/tau)`. The first version computed the coefficient in `prepare()` and applied
  it once per block, which made the fade ~6.4 s; fixed to time-based. Once the fade reaches zero the
  stage is skipped entirely, so drive 0 costs nothing and is bit-transparent from the first block.
- **Makeup baked from measurement**: a 21-point curve taken through the FULL stage (upsample → shaper →
  warm pole → decimate) on a −18 dBFS sine at warmth 0.4, interpolated over `drive x 20`. Drive 0 is
  exactly unity and every setting lands within **0.012 dB** of it. The near-linear estimate was far too
  small: the raw loss of `tanh(k x x)/k` on that sine reaches −6.39 dB at drive 1.
- **Wiring**: registered on the master bus in `applyMasterEffects`, which runs **before**
  `sharedReverb.processReturn`, so the shared room's tail is summed into a shaped path and never passes
  through the clipper; trim + limiter stay after it, so added loudness is guarded.
- **Three APVTS parameters**: `saturation_enable` (bool, default **false**), `saturation_drive`
  (0–1, default **0.35**), `saturation_tone` (labelled **Saturation Warmth**, 0–1, default **0.40**);
  raw pointers cached in `PluginProcessor.h`.
- **UI**: Effects block grows to **6 columns** — column 6 carries the power toggle (which carries the
  effect name) and Drive / Warmth as stacked labelled knobs, same pattern as the send room. Open band
  width chevron + 504 px, closed chevron + 176 px; row grows by column only. Grey-out on disable keeps
  knob values; tooltips and labels refresh in the i18n pass.
- **i18n × 6 locales**: `label.saturation`, `label.saturation_drive`, `label.saturation_warmth` and the
  three enable/drive/warmth tooltips (en Saturation / Drive / Warmth, fr Saturation / Drive / Chaleur,
  de Sättigung / Drive / Wärme, es Saturación / Drive / Calidez, ja サチュレーション / ドライブ / ウォーム,
  zh 饱和 / 驱动 / 温暖). Bytes verified as strict UTF-8, no BOM, no U+FFFD.
- **Persistence**: `MorphState.saturationEnable` / `saturationDrive` (0.35) / `saturationTone` (0.40),
  capture lines, ids in `getMorphParameterIds()`, lerps for the floats and `lerpOrStep` at 50 % for the
  bool; A/B slot XML gains `saturationEnable` / `saturationDrive` / `saturationTone` written and read back
  with defaults (false / 0.35 / 0.40), so an old slot file loads cleanly; preset apply maps the two
  sliders and the toggle.
- **Factory presets**: the three presets that promise nothing added (`correctionPure`, `transparentDub`,
  `liveScene`) name saturation **off** explicitly — on stage the plugin sits in a live monitor path, so
  nothing here adds loudness. `studioStack` 0.25 / 0.6, `spokenPodcast` 0.25 / 0.7 (warmth high keeps the
  added harmonics under the de-esser band), `broadcastVoice` 0.3 / 0.6, `breathyVocals` 0.15 / 0.8,
  `leadVocal` 0.35 / 0.5; `creativeRobot` is the deliberate exception at drive 0.45 with warmth 0.2, which
  is the effect's point.

### Tests

- **New `test/dsp/SaturationEffectTest.cpp`** — 11 suites, 816 assertions:
  - drive 0 leaves the buffer **bit-identical**, and disabling fades out to bit-identical;
  - makeup within ±1 dB of unity on a −18 dBFS sine at 200 Hz and 1 kHz for drives 0 / 0.25 / 0.5 / 0.75 / 1
    (measured worst deviation **0.012 dB**);
  - DC offset zero on a sine and on a ramp, measured over whole periods only;
  - headroom: continuity across the ±1 crossing and lower gain for a hot (+6 dBFS) input than for a full-scale one;
  - aliasing: with drive 1 at 7 kHz, the third harmonic is above the fundamental's 3 % (the shaper works)
    and the alias bins sit under 1.78 % of the fundamental (the oversampling kills them);
  - warmth: HF energy at 5400 / 9000 / 12600 Hz drops to ratio 0.84 when warmth goes from 0 to 1;
  - drive rises monotonically, stereo channels are independent, wet mix 0 gives unity, and the measured
    peak delay equals `getInternalDelaySamples()` = **16 samples** at the base rate.
- **Measurement lesson**: spectral ratios must be taken with Goertzel/DFT over **integer-cycle windows**
  (orthogonal bins). An earlier probe used a two-pole resonator with ~229 Hz bandwidth; its skirt leaked
  the huge fundamental into distant bins at ~−26 dB, and that contamination — not real aliasing — produced
  failures. The DFT helper self-checks: bin amplitude reads exactly 0.5 for a 0.5-amplitude integer-cycle
  sine, orthogonal bin 6e-16.
- Suite: **174 OK / 0 FAILED**, exit 0. Plugin target (`OpenVoxTuner`) builds green in Release.

### Notes

- Accepted consequence, documented in `docs/architecture/dsp-pipeline.md`: the reverb return is summed into
  a path carrying the 16-sample filter delay, so lead and return add with a comb ripple spaced ~4 kHz
  (first notch near 2 kHz). Inaudible on voice at normal drive; an alignment delay line stays the option if
  it ever needs killing. No plugin latency is declared.
- Deferred by design: per-bus saturation on the lead only, and a full "console" mode (crosstalk + mixer
  compression). Neither is promised by the current knob set.
