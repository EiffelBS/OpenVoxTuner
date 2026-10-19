# DSP Pipeline

OpenVoxTuner is an **audio effect** (not an instrument): it receives mono/stereo
audio and returns it transposed in pitch according to a chosen musical scale.
The entire real-time signal chain lives in the `ovtdsp::` namespace and is
driven from a single entry point, `OpenVoxTunerAudioProcessor::processBlock()`.

The module map / source tree and the exposed-parameter reference live on the
[Architecture Overview](../architecture.md) and
[Default Parameters](../default-parameters.md) pages respectively; this page is
the single source of truth for signal-flow details.

## Signal flow overview

```
                     +-----------------+  f0_in
 Audio In ---------> | NoiseGate       | ------------+
   (mono/stereo)     +-----------------+             |
                                                     v
                                           +-----------------+
                                           | YinPitchDetector|  f0_in (Hz)  or 0.0
                                           +-----------------+
                                                     |
                                                     v
                                           +-----------------+
                                           | ScaleQuantizer  |  f0_target = nearest scale note
                                           | (key, scale)    |
                                           +-----------------+
                                                     |
                                                     v   target ratio = f0_target / f0_in
                                           +-----------------+
        FormantPreserver (pre-shift) ----> | RetargetEnvelope|  Speed / smoothing
                                           +-----------------+
                                                     |
                                                     v
                                           +-----------------+
                                           |  PitchShifter   |  PSOLA (formant-preserved)
                                           +-----------------+
            LEAD BUS  ============================>   |   DeEsser, Delay
                                                     |   sends: Room + Hall
                                           +-----------------+ (optional)
                                           |  HarmonyEngine  |  shifted voices -> harmonyBuffer
             HARMONY BUS  ===================>          |   sends: Room + Hall
                                                     |   -> ROOM / HALL shared rooms
                                                     |      returns (ducker) -> master
                                           +-----------------+
                                                     |
                                           +-----------------+
                                           |  MASTER BUS     |  final mixed buffer
                                           +-----------------+
                                                     |
                                                     v
                                           +-----------------+
                                           | OutputLimiter   |  trim (-12..+12 dB),
                                           |                 |  true-peak limiter, -1 dBFS
                                           +-----------------+
 Audio Out <--------------------------------------------------------+
```

The pipeline is **always the same** regardless of whether ARA2 is bound. ARA
only *augments* the input stage (host key/scale metadata and a waveform cache);
it never bypasses or re-routes the DSP chain.

## Stage by stage

### 1. NoiseGate (`ovtdsp::NoiseGate`)

Applied to the input *before* pitch detection, to prevent the detector locking
onto room noise / breath between phrases.

- RMS-based level detection with a **sample-accurate** smoothed envelope
  (`rmsCoeff`, ~10 ms) so the gate tracks the real onset even when a voice
  starts mid-block.
- **Hysteresis** avoids chattering near the threshold: the open threshold is
  set 6 dB above the close threshold (`openThreshold = thresholdLinear * 2.0f`).
- Smooth attack (~15 ms) and release (~50 ms) one-pole coefficients; gain state
  persists across buffers.
- `process()` is a no-op when disabled.

### 2. YinPitchDetector (`ovtdsp::YinPitchDetector` implements `IPitchDetector`)

YIN fundamental-frequency estimation (de Cheveigne & Kawahara, 2002). Steps:

1. **Difference function** `d(tau) = sum (x[j] - x[j+tau])^2`
2. **Cumulative mean normalized difference** `d'(tau)`
3. **First minimum below threshold** (clarity threshold, default `0.05`)
4. **Parabolic interpolation** for sub-sample precision
5. **Anti-octave-error correction** via octave continuity
6. **Median filtering** (window of 5) for outlier rejection

Returns `f0` in Hz, or `0.0` when unvoiced. Detection range is `30 – 1000 Hz`
by default and can be narrowed at runtime with `setFrequencyRange()` (used by
the Voice Type selector to constrain the search to a vocal register).

### 3. ScaleQuantizer (`ovtdsp::ScaleQuantizer`)

Projects the detected `f0` onto the nearest note of the selected scale.

- `hzToSemitones()` / `semitonesToHz()` convert relative to A4 = 440 Hz.
- `setKey()`, `setScale()` and `setCustomIntervals()` (for `Scale::Custom`)
  rebuild the interval list; `quantize()` returns the nearest in-scale Hz, or
  the input Hz if no note belongs to the scale.

### 4. RetargetEnvelope (`ovtdsp::RetargetEnvelope`) — the "Speed" control

A 1st-order IIR (exponential) smoother that shapes how fast the pitch follows
the quantized target:

```
y[n] = y[n-1] + alpha * (target - y[n-1])
alpha = 1 - exp(-dt / tau)   where  tau = speedMs / 1000
```

- `Speed = 0 ms`  → instant correction (robotic / T-Pain style).
- `Speed = 50 ms` → fast but smooth (typical default).
- `Speed = 200 ms` → slow, very natural (almost no correction).

`processBlock()` is **buffer-size-independent**: it applies the time constant
using the actual block length, so `Speed` behaves identically at 64 or 1024
samples (the per-sample variant would scale the effective time constant by the
block size and make Speed nearly inert on small buffers).

### 5. PitchShifter (`ovtdsp::PitchShifter` implements `IPitchShifter`)

Simplified **PSOLA** (Pitch-Synchronous Overlap-Add) — the only pitch shifter
in the codebase. No phase vocoder is used (SWIPE, PYIN, RubberBand and
SoundTouch were evaluated and removed).

**Grain / pitch-mark description:**

1. **f0 detection** via `YinPitchDetector`.
2. **Pitch-mark detection**: for each fundamental period
   (`period = sr / f0`), advance an output phase and create a grain when the
   phase wraps.
3. **PSOLA grain**: for each analysis pitch mark, extract a Hann-windowed
   grain centered on the mark, whose length is scaled by the formant ratio.
4. **Re-positioning**: place the grain at the synthesis position, using
   correlation (`findBestOffset`) to align it with the previous grain for a
   smooth overlap-add.
5. **Overlap-Add (OLA)**: add the grains into the output buffer with a Hann
   window / 1-period hop, satisfying the COLA condition over stationary
   regions. Grain length is scaled by `2 * max(Tin / F, Tout)` so the formant
   ratio `F` preserves vocal-tract resonances. The algorithm is O(N) in the
   number of pitch marks.

A KBD (Kaiser-Bessel-derived) window overlap sum is measured once in
`prepare()` to avoid over-gain/clipping. `smoothedF0` (a block-aware one-pole,
TC ≈ 290 ms) smooths the target period so sudden note onsets don't produce a
discontinuity in the OLA spacing (click).

**Voice activity & attack envelope automaton:** two private structs
(`VoiceActivityDetector`, `AttackEnvelope`) make the former implicit state
machine explicit. The VAD applies hysteresis on the raw f0
(on above 45 Hz, off below 35 Hz) with a 256-sample debounce (~6 ms at
44.1 kHz). The attack envelope gain follows a one-pole smoother with two time
constants: normal (the `attack_ms` setting) and slow (80 ms). Three events
drive its cycle: a **block onset** (silence → voiced) snaps the gain to zero
then reopens over `attack_ms`; a per-sample **onset** (voiced edge or f0 jump
larger than ~2 semitones) ramps the gain down over 20 ms and recovers slowly
over 150 ms; a **ratio jump** (>3 % pitch-ratio delta between blocks) does the
same over 15 ms / 100 ms without restarting the OLA chain. An onset also
restarts the grain chain (`outPhase = 1`, `lastGrainCenter = 0`) to prevent
burst artifacts. When the external attack driver is active (deprecated
Attack-aware mode), the processor pushes a block-level target gain directly
and the internal timers are bypassed; when the envelope is disabled or
`attack_ms == 0`, the gain is forced to 1.

**Latency:** reported via `setLatencySamples()`; `PitchShifter::setLatencyMs()`
clamps the requested latency to **8–40 ms**. The `latency_mode` parameter picks
one of four presets:

| latency_mode            | Latency (ms) |
|-------------------------|--------------|
| Direct Monitoring       | 10           |
| Low Latency (default)   | 12           |
| Quality                 | 20           |
| Safe                    | 30           |

The shifter default before a mode is applied is 20 ms.

### 6. FormantPreserver (`ovtdsp::FormantPreserver`)

Compensates the formants (vocal-tract resonances) that PSOLA would otherwise
shift along with the pitch (the "chipmunk" effect).

- Runs **before** PSOLA; a 2nd-order Butterworth low-pass / peaking-EQ chain
  whose cutoff moves *opposite* to the transposition
  (`formantRatio = 2^(semitones/12)`, compensation `1/sqrt(ratio)` or `1/r`).
- `Mode::Legacy`, `Mode::MultiFormant` (F1–F4) and `Mode::Allpass` variants.
- `Strategy::Current` (partial `1/sqrt(r)` with fixed male-default centers) and
  `Strategy::P0` (full `1/r` with voice-type-aware formant centers). P1/P2
  (LPC cross-synthesis) are handled by `LpcFormantPreserver`.
- Biquad coefficient smoothing is **buffer-size independent** (`biquadSmoothAlpha
  = 0.05`, ~115 ms TC) so formant tracking stays stable across block sizes.
- `LpcFormantPreserver` analysis framing is **buffer-size independent**: a fixed
  24 ms Hann analysis frame (1058 samples at 44.1 kHz) held in a per-channel
  circular accumulator, with the LPC coefficients recomputed only every hop
  (10 ms default, 20 ms for harmony voices). The cross-synthesis itself is applied
  causally per sample (whitening `e = A_shifted(z)·x`, re-coloring `1/A_orig(z)`),
  so **no latency is added** and `setLatencySamples` stays 0. Measured: the output
  is identical for DAW block sizes 64 to 1024, and the RMS ratio at ratio 1.0 is
  0.999998. The module is currently disabled in the plugin (`useLpc` falls back to
  P0); see `docs/internal/plans/lpc-hop-overlap-add.md` for the measured status
  and the open cross-synthesis quality issue at ratio != 1.

### 7. HarmonyEngine (`ovtdsp::HarmonyEngine`)

Optional shifted-voice generation based on the active scale (`HarmonyType`
enum, `None` + 21 types, e.g. `ThirdBelowAbove`, `VocalStack3/4`,
`PowerChord`, `Drone`, `UnisonOctaves4`).

- `getHarmonyNotes()` computes per-voice frequencies from a quantized base
  frequency, key, scale intervals and the harmony type.
- `renderHarmonies()` synthesizes the voices (per-voice phase accumulators,
  smoothstep attack/release envelopes) directly into the output buffer.
- When the NoiseGate is active, per-voice attack is clamped to a short
  "gate-follow" time (~12 ms) so the harmony swells together with the gated dry
  signal instead of arriving late.

### 8. Effects buses (`ovtdsp::EffectsBus`), the shared reverb rooms and the output stage

Effects are not stacked on one global list any more: the processor owns **two** insert
`ovtdsp::EffectsBus` instances, tapped at two points of the chain, plus **two shared reverb
rooms** (`ReverbEffect` instances) that are fed by four sends instead of being registered on a bus.

| Bus | Tap point | Registered effects | Driving parameters |
|-----|-----------|--------------------|------------------|
| Lead | corrected lead voice, **before** any harmony is added | `DeEsser`, then `DelayEffect` (tempo-synced stereo ping-pong) | `deess_enable`, `deess_amount`, `delay_enable`, `delay_division`, `delay_mix`, `delay_feedback` |
| Master | final mixed buffer, **after** the blend | `SaturationEffect` (drive + warmth), then `OutputLimiter` | `saturation_enable`, `saturation_drive`, `saturation_tone`, `output_trim`, `limiter_enable` |

The harmony voices have **no insert bus**: they own two sends into the shared rooms and
nothing else, so the harmony path is the blend plus its reverb.

- **The reverb is a bus pair, not an insert.** `roomReverb` and `hallReverb` are tuned from
  `ReverbTuning.h` — ROOM 0.35 / 0.50 / width 1.0 (RT60 ≈ 1.18 s), HALL 0.70 / 0.30 / width 1.0
  (RT60 ≈ 2.43 s, the legacy Main room restored bit for bit). Four sends feed them:
  `applyLeadSends()` (lead → ROOM + HALL) and `applyHarmonySends()` (harmony → ROOM + HALL).
  A send adds to the room and **never attenuates the dry voice**, which is what makes the
  amounts safe to raise while singing; the old in-place wet mix multiplied the dry by `1 - mix`.
- **The lead send is taken after `leadBus.apply`**, so the de-esser sees only the dry lead and the
  tempo-synced echoes ring in the same space as the voice with no second room and no second
  parameter pair. The lead send runs through a 150 Hz one-pole low-cut, so breath rumble does not
  ring the room; the harmony send is unfiltered.
- **The returns are summed into the master before the output stage**, each scaled by
  `kReturnTrim = 0.7` (≈ -3 dB), so a unity send cannot sit at +5 dB over the voice and pump the
  limiter on every phrase.
- **One sidechain ducker guards both returns** (`ovtdsp::ReverbDucker`, `reverbDuckDepth`). Its key
  is the **corrected lead, taken before the lead bus effects** — never the noise gate, and never the
  delay output: an echo must not re-trigger the duck. The key is a per-sample stereo-linked peak
  envelope (attack 15 ms, release 300 ms) normalised against a -14 dBFS vocal peak, so the depth
  knob means the same thing at any input gain; 100 % ducks up to 12 dB while the lead sings and the
  room blooms back between phrases. Depth 0 is a true bypass.
- **The old gate-linked ducking is gone.** It multiplied the wet by `NoiseGate::getCurrentGain()`,
  so the room only moved when the gate closed and stayed still while the singer was singing — the
  opposite of what a ducker does, and the reason the single Mix knob never sounded like the legacy
  one.
- **Tuning lives in one place**: `Source/dsp/ReverbTuning.h` holds both room tunings and the
  "ready to sing" default sends, shared by the processor, the factory presets and the unit tests
  that measure the RT60 each tuning produces.

**The master bus runs `ovtdsp::SaturationEffect` first**, ahead of the two room returns, so each
room's tail is summed into an already-shaped path and never passes through the clipper itself. The stage is a soft clipper, `y = tanh(k x x) / tanh(k)` with `k = 1 + 9 x drive`:
that form keeps unity gain for quiet signals at every setting, so loudness comes from the baked
makeup curve (measured through the whole stage on a -18 dBFS sine, never estimated), not from the
curve. Drive **0** is a transparent bypass — `tanh(x)/tanh(1)` is not linear, so drive 0 short-
circuits the stage instead of running it. The shaper runs at **2x oversampling** through a 33-tap
Blackman half-band polyphase interpolator/decimator pair (group delay 16 samples at the base rate),
which is what keeps the added harmonics under Nyquist; no plugin latency is declared. A one-pole
low-pass at the oversampled rate, corner smoothed from 8000 Hz to 5000 Hz by `saturation_tone`,
rolls off the top of the distortion, and a subtractive high-pass near 20 Hz after decimation keeps
the output DC-free. Inputs beyond full scale (an ARA buffer can exceed ±1) are compressed into the
curve before shaping, so a hot master does not clip hard.

One consequence is documented on purpose: because the reverb return is summed into a path carrying
that 16-sample filter delay, the two add with a comb ripple spaced ~4 kHz (first notch near 2 kHz).
It is inaudible on voice at normal drive and was accepted over adding an alignment delay line; no
latency is declared to the host.

`ovtdsp::OutputLimiter` runs **last** on the master bus: an output trim in dB
(`-12 .. +12`) followed by a true-peak limiter with a linked-channel one-pole envelope
(instantaneous attack, 120 ms release) and a `0.891` linear ceiling = **-1 dBFS**. The detector
reconstructs the inter-sample crest through a **65-tap** 4x oversampling FIR, so the gain closes on what a
DAC outputs, not only on stored samples: measured on a full-scale square wave the reconstructed
crest sits +2.1 dB above the sample peak, and up to +4.6 dB on broadband material. A shorter window
(33 taps) under-reported that overshoot — it saw +2.4 dB where a wider estimator saw +4.4 dB — so the
tap count is what lets the ceiling be met against the right number. Reconstructing
a crest needs the samples after it, so the stage looks ahead by its filter group delay (**8 samples**,
about **0.17 ms**) and that latency IS declared through `setLatencySamples`, added to the pitch shifter's.
The gain closes on a target slightly below the ceiling (`targetLinear` = -1.22 dBFS, `kSafetyMargin` 0.975):
the detector reads the input crest while the soft clip shapes the output and adds ringing the detector
cannot see (measured +0.19 dB on noise, +0.07 dB on a square), so closing on the ceiling itself landed
true peaks at -0.81 dBFS. With the margin, measured true peaks are -1.15 dBFS (square) and -1.03 dBFS
(noise burst). A soft-clip stage after the gain bounds residual crests toward the ceiling. The trim
scales the final result only, leaving the harmony `1/sqrt(1+N)` gain-match intact.

The sleep path runs the two insert buses on the cleared buffer and lets the ducker's release
and both rooms' tails decay, then returns; `OutputLimiter` passes a silent buffer through
untouched. The harmony path is the one exception it states explicitly: its buffer
deliberately keeps the last blended take, so the harmony activity peak below is stored as
zero rather than read from a signal that no longer reaches the output. The harmony send is
**not** tapped in the sleep path: its last block already fed the rooms once, and re-sending
it would let the tail run twice.

### Activity taps (what each path carried)

The editor shows one activity dot per Effects column and two per Reverb voice column (both
sends of a voice watch the same path), sized by a **peak read at the exit of the path that
column or room acts on**, so the dot answers
"is anything travelling here" - a question no control value can answer, because a mix
at 0 % and a disabled stage look identical on screen. Three numbers are written on the
audio thread, once per block, and read by the editor timer at ~30 Hz:

| Atomic | Written at | Reports |
|--------|-----------|---------|
| `leadActivityPeak` | end of `applyLeadEffects()`, send excluded | the corrected voice after its own effects |
| `harmonyActivityPeak` | end of `applyHarmonySends()`, before the mix | the blended voices leaving their own path |
| `masterActivityPeak` | `applyMasterEffects()`, after both room returns, before trim + limiting | the blend the master path carries |

The taps are inside the three helpers rather than at their call sites, so the live path
and the sleep path report through exactly the same code. `blockPeak()` scans the block,
which the silence detector already does once per block. Nothing here paints: the dots
smooth their own release on the message thread, and `meterPeak` (the limiter's own
reading, further down the same stage) keeps answering the separate question "how hot
did the limiter have to work".

## Corrective blend & modes

- **Amount** (`0..1`) blends between passthrough and fully corrected pitch
  (`0` = dry).
- **Auto (Live) mode**: target comes from automatic scale quantization.
- **Graphic (Curve Editor) mode**: target comes from `ovtdsp::PitchCurve::getPitchAt(t, f0_in)`
  evaluated at the transport time; the rest of the chain (amount, retarget,
  formants, PSOLA) is unchanged.

## References

- de Cheveigne, A., & Kawahara, H. (2002). *YIN, a fundamental frequency
  estimator for speech and music*. JASA.
- Moulines, E., & Charpentier, F. (1990). *Pitch-synchronous waveform
  processing techniques for text-to-speech synthesis using diphones*.
  Speech Communication.
- Zölzer, U. (2011). *DAFX: Digital Audio Effects* (2nd ed.). Wiley.
