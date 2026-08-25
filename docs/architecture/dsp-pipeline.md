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
                                                     |
                                           +-----------------+ (optional)
                                           |  HarmonyEngine  |  shifted voices
                                           +-----------------+
                                                     |
                                           +-----------------+ (optional)
                                           |  ReverbEffect   |  post-processing (IEffect)
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

### 8. Post-processing effects (`ovtdsp::IEffect`)

Effects stacked after the pitch-correction + harmony mix, processing the final
buffer in place. `ReverbEffect` (`id = "reverb"`) wraps `juce::Reverb` and
smooths the enable gain to avoid clicks when toggled.

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
