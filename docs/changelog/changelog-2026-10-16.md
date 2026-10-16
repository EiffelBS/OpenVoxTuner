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
- Deferred, not implemented here: ducking an effect return by the gate envelope, a shared
  `DelayLine` primitive for chorus/flanger/phaser.
- Observed gap, outside this scope: the A/B slot XML carries no `deess_enable` / `deess_amount`
  attributes (left over from #130), so the de-esser is not stored in slots.
