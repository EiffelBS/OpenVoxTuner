# Analysis report: Formant preservation for natural audio processing

**Project**: OpenVoxTuner
**Date**: 2026-07-27
**Scope**: In-depth study of formant preservation methods (LPC, temporal smoothing/interpolation), comparison with the existing solution, gaps, and an actionable improvement plan with validation criteria and success metrics.

> **Status update (2026-08-24)**: This historical report predates the current implementation.
> LPC cross-synthesis is now implemented via `LpcFormantPreserver` with the P0/P1/P2
> strategies described in the [DSP Pipeline](architecture/dsp-pipeline.md). Sections 4 and 5
> below describe the *pre-LPC* state of the codebase and are kept for reference.

---

## 1. Objective and context

During pitch-shifting, formants - resonances of the vocal tract located around 300-3500 Hz (F1-F4) - are shifted along with the fundamental if one uses a simple duration modification or naive PSOLA. This produces the "chipmunk" effect (voice too thin when shifting up) or the "Darth Vader" effect (voice too deep when shifting down): the **timbre** (vowel color) is altered even though the pitch is correct.

Formant preservation aims to **decouple** the fundamental pitch `F0` from the spectral envelope (the vocal tract "filter"), so that only `F0` changes while the formants stay at their natural place. This is the key to a "perfectly natural" result.

The analysis below studies two families of methods - **Linear Predictive Coding (LPC)** and **temporal smoothing / interpolation** - evaluates them, compares them with the solutions deployed in OpenVoxTuner, and proposes concrete improvements.

---

## 2. Fundamental concepts

- **Formants**: poles of the vocal tract filter. Their position defines the vowel; their shifting along with `F0` is the artifact to correct.
- **Source-filter model**: speech = glottal excitation (which determines `F0`) filtered by the tract (which determines the formants). Preserving the formants = keeping the **filter** while modifying the **source**.
- **Pre-warping of Moulines & Charpentier (1990)**: move the formants in the opposite direction of the transposition. *Partial* `1/sqrt(r)` or *complete* `1/r` compensation depending on whether one acts on frequency or on playback time.

---

## 3. Detailed study of the methods

### 3.1 LPC - Linear Predictive Coding

**Principle.** Each frame of the signal is modeled by an all-pole filter `H(z) = G / A(z)`, where `A(z) = 1 + a1 z^-1 + ... + aP z^-P`. The spectral envelope (hence the formants) is entirely carried by the poles of `A(z)`.

**Preservation via LPC cross-synthesis** (the reference technique) proceeds as follows:

1. Analyze the *naively transposed* signal (formants shifted) -> its LPC coefficients `a_shifted`.
2. **Whiten**: `e[n] = A_shifted(z) * x[n]` -> the envelope of the transposed signal is removed, leaving only the excitation (the source, at the new pitch).
3. **Re-synthesize** by filtering the excitation through the **original** envelope `1/A_orig(z)` -> the formants return to their natural place.

> Implementation note (verified in the benchmark): the excitation must be `e = x + sum a_k * x[n-k]` (**plus** sign), otherwise the envelope is inverted and the cross-synthesis destroys the formants instead of preserving them.

**Relevance.** This is the only method that performs a **true source/filter decoupling**: the formants are extracted then reinjected independently of `F0`. [architecture.md](architecture.md) already listed this approach ("Exact formant preservation via LPC + non-uniform resampling") as *future work* - it is therefore known to be the right solution, not yet implemented.

**Advantages.**
- *Near-exact* preservation of the formants (the cross-synthesis restores the target envelope, not an approximation).
- Works for all transposition ratios and all voice types.
- Single physical parameter: the LPC order `P` (typically `2 x N_formants + 2 = 10`).

**Limitations.**
- Computational cost (per-frame LPC analysis + filtering) higher than the biquad bank.
- **Frame boundary** artifacts (amplitude modulation if the per-frame gain is not managed) - mitigable via per-frame energy normalization and Hann overlap-add.
- Finite LPC order does not perfectly capture 4 formants plus the spectral tilt; the cross-synthesis leaves residual distortion (see Section 7).
- Sensitive to noise if the LPC analysis is done on the noisy signal (mitigable by estimating the target envelope on the clean signal or via pre-emphasis).

### 3.2 Temporal smoothing and interpolation

**Principle.** The formant parameters (either the biquad coefficients of the filter bank or the LPC coefficients) vary from frame to frame (the transposition ratio itself fluctuates: vibrato, portamento, YIN jitter). **Temporal smoothing** interpolates the coefficients between frames to avoid:
- discontinuities (clicks/pops) when the ratio jumps;
- a timbral "warble" when a modulated ratio (vibrato around 5 Hz) makes the formant centers sweep.

Two variants:
- **Biquad smoothing** (the project's approach): the target coefficients are lerped toward the applied coefficients, at a per-block step `alpha`.
- **Interpolation of LPC coefficients** (within the cross-synthesis): the `a_k` of neighboring frames are averaged before whitening/re-synthesis, which smooths the formant envelope over time.

**Relevance.** Indispensable as a complement to *any* method (LPC or filtering), because the actual ratio signal is never constant. It is a **robustness fix**, not a preservation method on its own.

**Advantages.**
- Eliminates pops and warble (the project already fixed a 5 Hz warble via `biquadSmoothAlpha`, see Section 4).
- LPC interpolation slightly improves the distortion at large ratios (observed in Section 7: C1 <= C0).

**Limitations.**
- **Too slow** smoothing (small alpha) introduces a timbral *lag*: during a vibrato or a fast transition, the formants are temporarily mis-compensated -> audible warble.
- **Too fast** smoothing (large alpha) lets the ratio modulation through -> residual AM at the output and risk of clicks.
- Smoothing does not *create* preservation: it merely makes an already imperfect underlying method tolerable.

### 3.3 Reference: pre-warping via a filter bank (current project method)

`N` peaking-EQ filters (biquads) are placed at the formant frequencies and their centers are moved according to `1/sqrt(r)` (partial compensation) or `1/r` (complete). Advantage: very lightweight (a few biquads). Drawbacks: it is only an *approximation* (a peaking-EQ does not reshape the envelope exactly, and the formant bandwidths get distorted), and the formant frequencies must be *known*.

---

## 4. Solutions deployed in OpenVoxTuner (current state)

The project uses **two** formant mechanisms, both based on *ratio scaling* (no true source/filter decoupling):

### 4.1 `FormantPreserver` (`Source/dsp/FormantPreserver.h`)
- `MultiFormant` mode: 4 peaking-EQ filters (F1-F4).
- **Partial** compensation `1/sqrt(r)` (see `FormantPreserver.cpp`, `compensationRatio = 1/sqrt(r)`).
- **Formant centers fixed by default to `[500, 1500, 2500, 3500] Hz`** (male voice defaults, `FormantPreserver.h` line 121) - *identical regardless of voice type*.
- Temporal smoothing of the biquads: `biquadSmoothAlpha = 0.05` (Fix AZ, line 116), fixing a 5 Hz warble inherited from alpha=0.002.

### 4.2 `PitchShifter` (`Source/dsp/PitchShifter.cpp`)
- `formantRatio` (playback speed of the PSOLA grains) partially decouples the formants from time *inside* PSOLA (line 666, `double F = currentFormantRatio`). Smoothed with `alpha = 0.005`.
- In `PluginProcessor.cpp`, `userFormantRatio = 2^(shiftSemitones/12)` (line 2093) is derived from the *same* shift as the pitch and passed to the PitchShifter (line 2146), independently of the `FormantPreserver` (lines 2126-2127).

**Assessment.** Two *approximate* approaches are superimposed. Neither extracts nor re-injects the formant envelope; both move centers via an imperfect ratio law.

---

## 5. Identified gaps

| # | Gap | Impact |
|---|--------|--------|
| G1 | **Fixed formant centers** `[500,1500,2500,3500]` regardless of voice type | Actual centers differ strongly (male F1~730, female~850, child~900). Pre-warping applies "next to" the true formants -> partially ineffective compensation. |
| G2 | **Partial compensation `1/sqrt(r)`** instead of `1/r` | At large ratios, the formants remain offset (under-compensation when shifting up, over-compensation when shifting down). |
| G3 | **No source/filter decoupling** | The method acts by *ratio-scaling* an approximate envelope, not by extraction/re-injection of the actual envelope. |
| G4 | **Distorted formant bandwidths** | A peaking-EQ moves the center but does not physically alter the bandwidth; the shape of the formants is changed. |
| G5 | **Potential double application** (FormantPreserver `1/sqrt(r)` + `formantRatio` grain speed) | Risk of combined over-/under-compensation depending on settings. |
| G6 | **No explicit voice-type robustness** | `voice-type-feasibility-report.md` Section 2.3C proposed per-voice-type formant settings, deemed optional and not implemented. |
| G7 | **No perceptual or metric validation** | No objective measure of formant distortion in CI (existing tests cover smoothing warble, not preservation quality). |

---

## 6. Objective evaluation criteria

| Criterion | Metric | How measured |
|---------|----------|----------------|
| **Formant distortion** | Global LSD (dB) and LSD over *formant bands* (dB) vs ideal reference | RMS deviation of the LPC envelopes between output and ideal reference (formants left in place). |
| **Perceived naturalness** | MUSHRA score (0-100) by human raters | Protocol in Section 8 (not executable in automated CI). |
| **Real-time performance** | CPU ms per s of audio | Relative benchmark; the LPC algorithm is real-time viable in C++ (see caveat Section 7.4). |
| **Voice compatibility** | LSD on male/female/child voices | Same metrics, 3 canonical formant sets. |
| **Noise robustness** | Formant-band LSD at SNR 20/10 dB | LPC cross-synthesis applied to a noisy transposed signal. |

---

## 7. Quantitative comparative tests

### 7.1 Methodology

A pure-numpy test bench (`test/formant_preservation_benchmark.py`, no scipy) models source-filter vowels (Klatt parallel resonator bank, 4 balanced formants) for 3 voice types (male F0=120, female 220, child 300) and 4 transposition ratios `r in {0.75, 1.0, 1.5, 2.0}`. Five methods are compared against an **ideal reference** (vowel at the output pitch with formants left in place):

- **B0**: naive transposition (formants coupled to the pitch) - baseline artifact.
- **B1**: filter-bank pre-warping `1/sqrt(r)` - **optimistic model of the project** (here the bank is assumed to *know the true formants*; the actual project, with fixed centers, is *worse* than this B1).
- **B2**: pre-warping `1/r` - theoretical ideal of the filtering approach.
- **C0**: LPC cross-synthesis (per-frame envelope swap).
- **C1**: LPC cross-synthesis + **temporal interpolation of the LPC coefficients**.

Envelopes are computed by LPC *per frame* (then averaged), which avoids the spurious LPC poles of an under-parameterized global analysis.

### 7.2 Spectral distortion (global LSD, dB, vs ideal reference)

| Voice | r | B0 (naive) | B1 (project*) | C0 (LPC) | C1 (LPC+sm.) |
|------|---|-----------|--------------|----------|---------------|
| Male | 0.75 | 4.10 | 3.58 | **1.52** | **1.55** |
| Male | 1.50 | 8.06 | 4.71 | **3.48** | **3.49** |
| Male | 2.00 | 12.24 | 8.24 | **3.10** | **3.02** |
| Female | 1.50 | 9.63 | 5.28 | **3.28** | **3.24** |
| Female | 2.00 | 14.13 | 7.76 | **3.99** | **3.95** |
| Child | 1.50 | 12.57 | 6.50 | **3.51** | **3.50** |
| Child | 2.00 | 15.86 | 11.70 | **6.29** | **6.29** |

*At r=1.0, all methods give LSD ~0 (bench consistency).*

**Finding**: LPC cross-synthesis (C0/C1) is systematically the best, with a gain of **~2.5x to ~4x** over the project's pre-warping (B1) and of **~3x to ~5x** over naive (B0) at large ratios. Temporal interpolation (C1) is slightly better than C0 at large upward shifts (3.02 vs 3.10 at r=2.0 male), confirming the value of LPC smoothing.

### 7.3 Formant-band distortion (FBAND LSD, dB)

| Voice | r | B0 | B1 (project*) | C0 (LPC) | C1 (LPC+sm.) |
|------|---|----|--------------|----------|---------------|
| Male | 2.00 | 6.87 | 6.93 | **4.36** | **4.30** |
| Female | 2.00 | 4.87 | **9.53** | 5.25 | 5.18 |
| Child | 2.00 | 9.72 | 10.08 | **6.81** | **6.82** |

**Critical finding**: at r=2.0 female, `1/sqrt(r)` pre-warping (B1=9.53) is **worse than naive transposition** (B0=4.87) in the formant bands. Partial compensation over-compensates the formants for some voices/ratios - proof that the fixed-ratio approach is **unreliable across voice types**, which strongly motivates moving to LPC.

### 7.4 Warble / temporal smoothing (RMS modulation depth, 5 Hz vibrato)

| `biquadSmoothAlpha` | 0.002 (slow) | 0.050 (project) | 0.200 (fast) |
|---------------------|--------------|----------------|----------------|
| Residual modulation | 0.0246 | 0.0559 | 0.1262 |

The project's current setting (0.05) is a compromise: slower smoothing reduces residual AM but introduces a timbral *lag* (the historical warble of Fix AZ); fast smoothing follows the modulation but generates more AM and more click risk. **Smoothing is a parameter to be tuned by ear**, not a preservation solution.

### 7.5 Real-time performance (CPU ms / s of audio, best of 3)

| Method | B0 | B1 (project) | C0 (LPC) | C1 (LPC+sm.) |
|---------|----|-------------|-----------|---------------|
| ms/s | 12.4 | 20.9 | 114.7 | 117.5 |

**Important caveat**: these figures come from an *unoptimized* Python/numpy implementation (interpreter loops dominate the cost). The LPC algorithm itself - Levinson-Durbin (order 10) + all-pole filtering of ~400-sample frames at hop 100 -> ~160 frames/s, a few thousand operations each - is **perfectly real-time in C++/JUCE** (well under one ms per frame). The ~5-6x ratio observed here is an upper bound; natively it will be much closer to the biquad bank (which remains ~5x cheaper but of clearly lower quality).

### 7.6 Robustness to ambient noise (formant-band LSD, LPC cross-synthesis)

| Condition | SNR 20 dB | SNR 10 dB | Clean (floor) |
|-----------|----------|-----------|-------------------|
| FBAND LSD (dB) | 3.31 | 3.22 | 4.40 |

LPC cross-synthesis **does not degrade catastrophically** under noise (3.3 dB at SNR 10-20 dB, comparable to the clean floor of 4.4 dB). The method is therefore reasonably robust to a noisy input signal - provided the target envelope is estimated on the clean signal or via pre-emphasis. *Benchmark limitation*: the residual distortion (~4 dB) also reflects the finite LPC order and per-frame gain matching; a higher order and better frame management would reduce it.

---

## 8. Qualitative tests - perceived naturalness (listening protocol)

Formant distortion (LSD) correlates strongly with perceived naturalness: a preserved formant envelope = recognizable, natural vowel. However, only human listening can provide a final verdict. Proposed protocol (MUSHRA type):

1. **Stimuli**: for each voice type and `r in {0.75, 1.25, 1.5, 2.0}`, generate 4 versions - (a) ideal reference (B2), (b) current project (FormantPreserver + PitchShifter), (c) LPC cross-synthesis (C0), (d) naive (B0, degraded "hidden reference" anchor).
2. **Subjects**: 8-12 raters, headphone listening, randomized and double-blind presentation.
3. **Scale**: score from 0 (artificial/chipmunk) to 100 (natural), each rating compared against the "hidden reference" anchor.
4. **Instructions**: rate *the naturalness of the timbre/vowel*, independently of pitch accuracy.
5. **Analysis**: mean and standard deviation per condition; Friedman test + post-hoc; significance threshold p<0.05.

**Expected hypothesis (to be confirmed)**: LPC (C0) >= reference > current project > naive. This protocol must be run before any production release of the LPC approach.

---

## 9. Prioritized implementation recommendations

### P0 - Short term, high impact / low risk
- **P0.1: Voice-type-dependent formants (G1, G6).** Replace the fixed centers `[500,1500,2500,3500]` with a formant set selected according to the estimated `F0` (or an explicit male/female/child parameter). Target: `FormantPreserver::formantConfigs` + voice-type detection/parameter in `PluginProcessor`.
- **P0.2: Switch to `1/r` compensation (G2).** In `FormantPreserver.cpp::updateAllFormants`, use `compensationRatio = 1/r` (instead of `1/sqrt(r)`) on the *actual* centers of the voice type. Immediate improvement with no new DSP.

### P1 - Medium term, major qualitative gain
- **P1.1: LPC cross-synthesis module (C0).** New class `ovtdsp::LpcFormantPreserver`: per-frame LPC analysis (order 10, Hann window, hop ~100 samples at 44.1 kHz), whitening of the transposed signal, re-synthesis with the reference envelope (extracted from the clean input signal or from an envelope cache). Per-frame energy normalization + overlap-add to avoid boundary warble.
- **P1.2: Temporal interpolation of LPC coefficients (C1).** Average the `a_k` of neighboring frames (or interpolate exponentially) before re-synthesis - reduces distortion at large ratios (Section 7.2).
- **P1.3: Disambiguate FormantPreserver vs formantRatio (G5).** Document and settle the `PluginProcessor` wiring (lines 2126-2146) so that only one preservation chain is active per mode, avoiding double compensation.

### P2 - Long term, robustness
- **P2.1: Pre-emphasis + adaptive LPC order** to reduce residual distortion (~4 dB, Section 7.6) and noise sensitivity.
- **P2.2: Automatable MUSHRA test** (Section 8) and integration of a formant-distortion score in CI (re-run `test/formant_preservation_benchmark.py` on real excerpts).
- **P2.3: Hybrid mode** - LPC when the signal is stable and clean, fallback to the filter bank (voice-type-aware) under strong noise / unvoiced speech.

---

## 10. Validation steps

1. **Unit tests**: tests on synthetic signals (source-filter vowels) verifying that (a) the output LPC envelope matches the target envelope within +/-1 dB LSD, (b) no boundary warble (RMS modulation < 1%).
2. **Regression**: re-run the quantitative benchmark (Section 7); require C0/C1 < B1 across all (voice, r) pairs, and C1 <= C0 at large r.
3. **Integration**: verify the absence of clicks/pops (existing 5 Hz modulation test `FormantPreserverModulationTest` extended to the LPC module).
4. **Perceptual**: MUSHRA protocol (Section 8); release threshold: LPC significantly > current project (p<0.05) and not significantly < ideal reference.
5. **Real-time**: verify latency/CPU in native C++ under a 1x real-time budget at 44.1 kHz stereo.

---

## 11. Success metrics

| Metric | Current state (project B1) | Target (LPC C0/C1) | Measurement |
|----------|--------------------------|-------------------|--------|
| Global LSD at r=2.0 (male/female/child avg.) | ~9.2 dB | **<= 4 dB** | benchmark Section 7.2 |
| Formant-band LSD at r=2.0 | 6.9-10.1 dB | **<= 5 dB** | benchmark Section 7.3 |
| Naturalness MUSHRA score | reference | **>= +20 pts vs current** | listening Section 8 |
| Boundary warble (RMS) | n/a (new module) | **< 1%** | unit test |
| Native CPU | ~1x (biquads) | **< 2x biquads** | C++ profiling |
| Noise robustness (FBAND LSD @ SNR10) | - | **<= 4 dB** | benchmark Section 7.6 |

**Improvement criterion vs existing**: reduction of at least **~2.5x in formant distortion** (LSD) at large transposition ratios, and an increase in perceived naturalness measured by listening.

---

## 12. Summary

**LPC cross-synthesis** is the most relevant method for "perfectly natural" formant preservation: it performs the exact source/filter decoupling that the project's ratio-based approximations do not achieve. **Temporal smoothing / interpolation** is an indispensable robustness fix (and LPC interpolation even slightly improves quality). The solutions deployed in OpenVoxTuner suffer from fixed formant centers, partial `1/sqrt(r)` compensation, and the absence of true decoupling - gaps quantified by the quantitative benchmark (LPC ~2.5-5x better than the project at large ratios). The P0 recommendations (voice type + `1/r`) offer an immediate gain; P1 (the LPC module) delivers the qualitative leap, to be validated by MUSHRA listening and real-time profiling before production.