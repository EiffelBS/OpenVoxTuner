# Feasibility Analysis: 8 New Pad Presets for Harmonies

**Date:** 2026-07-28
**Status:** Pre-deployment, for analysis

---

## 1. Analysis of the Existing Architecture

### 1.1 Current Harmony Pipeline

The harmony engine (`HarmonyEngine.h/cpp`) works according to this diagram:

```
Input f0 (Hz) + Scale Info
       |
       v
getHarmonyNotes() -> [f1, f2, f3, f4] (freqs)
       |
       v
Per-voice loop (per-block, per-sample):
  - Phase accumulation (sin-based oscillator)
  - Tone waveform selection (switch on toneMode 0-5)
  - Amplitude envelope (smoothstep attack, one-pole decay)
  - Stereo panning (fixed per voice: R, L, RR, LL)
  |
  v
Output buffer -> Gain matching -> Mix with dry signal
```

### 1.2 Major Technical Constraints

**The `HarmonyEngine::renderHarmonies()` operates in a `for (int i = 0; i < numSamples; ++i)` per-sample loop.** Every sample must be computed in O(1) with minimal complexity.

**Available DSP resources:**
| Resource | Availability | Current usage |
|-----------|-----------------|--------------|
| `std::sin()` (sine oscillator) | Yes (JUCE) | Fundamental + harmonics |
| 2nd, 3rd, 4th harmonics | Yes (`sin(n*p)`) | All presets |
| Amplitude envelope | Yes (smoothstep+one-pole) | Global attack/release |
| Standalone LFO | **No** | No such module |
| IIR filter (LP/HP/BP/PEAK) | **No** (except FormantPreserver) | No tone filtering |
| Effects (Delay/Reverb/Chorus/Distortion) | Single reverb (post-pipeline) | Not used for tones |
| Amplitude modulation | Yes (ring mod on vocoder) | Vocoder-like only |
| Saturation | Yes (`tanh()`) | Synth Lead only |
| Persistent phase accumulation | Yes (per-voice `phase` variable) | All presets |
| Per-voice state | `phases[]`, `amplitudes[]` | 2 vectors |

### 1.3 Parameters Usable for Synthesis

The only modulation parameters available **in real time per sample** are:
1. **Phase** (`phase` / `p`): the oscillator's sole internal time variable
2. **Harmonics** `sin(2*p)`, `sin(3*p)`, `sin(4*p)`: 3 variants
3. **`color` parameter** (0.0-1.0): global continuous parameter per preset
4. **Amplitude envelope** (`amp`): per-note LFO envelope (not per sample)
5. **tanh() saturation**: usable non-linearity
6. **Persistent per-voice state**: `phases[]` (1 double vector)

### 1.4 Critical Limitations

| Limitation | Detail | Impact |
|--------|---------|--------|
| **No filter** | No bandwidth control, no resonance | A true "formant sweeping" cannot be created |
| **No LFO** | No independent low-frequency oscillator | A real "breathe" (modulation < 1 Hz) is impossible |
| **No delay** | No sample memory | No chorus, phaser, delays |
| **No wavetable** | Only sin/n*sin | Native complex waveforms cannot be created |
| **Per-sample** | Every sample must be O(1) | Any memory > 1 double per voice is a CPU risk |
| **Max 4 harmonics** | sin(4*p) | No harmonics above the 4th |

### 1.5 Proposed Synthesis Strategy

Given these constraints, all new sounds must be implemented **exclusively** via:
- **Weighted combinations** of the existing harmonics (base, h2, h3, h4)
- **Phase modulation** (slow phase drift to emulate an LFO)
- **tanh() saturation** (warmth, soft distortion)
- **Amplitude modulation** (ring modulation, amplitude sweep)
- **Dynamic panning** (movement between left/right channels via phase)
- **Interpolation between harmonics** (simulating filtering via spectral redistribution)
- **Persistent per-voice states** (1 to 2 doubles max, for slow state)

**No new external DSP module is required.** Implementation happens inside `renderHarmonies()` only.

---

## 2. The 8 Proposed New Pad Presets

### 2.1 No. 6: "Shimmer"

**Sound identity:** Ethereal pad with a slight octave doubling above and a resonant glow. Comparable to a synthesizer shimmer pad (Roland Juno Shimmer, Prophet Shimmer).

**Synthesis chain:**
```
Shimmer = 0.45*f1 + 0.35*f1detuned + 0.12*h2 + 0.05*h3 + 0.03*(2f1)
f1detuned = sin(phase * (1 + slowPhaseWander))
slowPhaseWander = sin(phase * 0.008) * 0.003  // ultra slow
(2f1) = sin(2*f1_phase)  // 2nd "sub-octave" via phase doubling
```

**Technical parameters:**
- Slow phase drift (`sin(phase*0.008)*0.003`) emulated via phase modulation
- 7th component: high-frequency octavization
- No saturation
- Longer release envelope (release +20%)

**Integration:** 1 additional persistent state (`shimmerPhase`) in `phases[]`

**Color parameter:** Modulates the phase drift width (`0.008` +/- `0.004*color`)

---

### 2.2 No. 7: "Warm Sub-Pad"

**Sound identity:** Deep, enveloping pad centered on the lows and the fundamental. Comparable to an analog-style pad (Yamaha CS-80 bass pad, Sequential Prophet deep pad).

**Synthesis chain:**
```
Warm = 0.55*f1 + 0.25*f1sub + 0.12*h2 + 0.06*tanh(0.8*f1)
f1sub = sin(0.5 * phase)  // sub-oscillator one octave below
```

**Technical parameters:**
- Sub-oscillator one octave below (`sin(phase/2)`)
- Light tanh saturation on the fundamental for warmth
- Energy mostly in the low end of the spectrum (< 500 Hz)
- Wide stereo image (extended L/R)

**Integration:** 0 additional persistent state (sub computed directly)

**Color parameter:** Modulates the saturation (`tanh(0.8*color*f1)`) and the sub strength (`0.25*color`)

---

### 2.3 No. 8: "Glass"

**Sound identity:** Crystalline, metallic sound with sharp high harmonics. Comparable to a glass harmonica, a synthetic glockenspiel, or the Korg Prophecy "crystal" patch.

**Synthesis chain:**
```
Glass = 0.15*f1 + 0.10*h2 + 0.40*h3 + 0.25*h4 + 0.10*tanh(1.5*h3)
```

**Technical parameters:**
- Weighting heavily oriented toward harmonics 3 and 4
- Light saturation on h3 to emphasize the metallic character
- Very fast attack (short smoothstep), medium release
- No modulation, pure and direct sound

**Integration:** 0 additional state

**Color parameter:** Shifts the h3/h4 balance (`0.40 +/- 0.15*color` on h3, `0.25 +/- 0.15*color` on h4)

---

### 2.4 No. 9: "Breath"

**Sound identity:** Pad inspired by vocal breath, without pronounced formants, gently undulating. Comparable to a breath pad from Native Instruments "Rozart Voices" or a "Session Brass" pad.

**Synthesis chain:**
```
Breath = (0.6*f1 + 0.2*h2 + 0.15*h3 + 0.05*h4) * breathEnv
breathEnv = 0.5 + 0.5 * sin(phaseSlow)
phaseSlow += phaseInc * breathSpeed  // slow accumulator
breathSpeed = 0.0008 +/- 0.0004*color  // ~1-2 Hz at 44.1kHz
```

**Technical parameters:**
- Independent slow phase accumulator (`phaseBreath` to be added)
- Breath envelope applied over the entire signal
- Gentle weighting favoring the fundamental and 2nd harmonic
- No saturation

**Integration:** 1 additional persistent state (`phaseBreath` per voice, randomly initialized to avoid synchronization)

**Color parameter:** Modulates the breath speed (`0.0008 +/- 0.0004*color`) and the envelope depth

---

### 2.5 No. 10: "Analog Pad"

**Sound identity:** Warm analog-style pad built on a synthetic triangle wave, comparable to a Juno-60 pad, Oberheim OB-6, or Korg MS-20 pad.

**Synthesis chain:**
```
// Triangle approximation via sine sum
TriangleApprox = f1 - 0.25*h2 + 0.11*h3 - 0.06*h4  // truncated Fourier series
Analog = 0.7*TriangleApprox + 0.2*f1 + 0.1*f1detuned
f1detuned = sin(phase * (1 + detune))
detune = sin(phase*0.01) * 0.002  // ultra-slow phase drift
```

**Technical parameters:**
- Triangle waveform approximation (Fourier series truncated to 4 terms)
- Slight phase drift (drift characteristic of analog synths)
- Light tanh saturation (`tanh(1.1*Analog)`)
- Medium envelope (attack 35ms, release 80ms)

**Integration:** 0 additional state (the drift term is computed from the existing `phase`)

**Color parameter:** Modulates the amount of drift (`0.002*color`) and of saturation

---

### 2.6 No. 11: "CinePad"

**Sound identity:** Grand orchestral/cinematic pad, wide and immersive, comparable to a Hans Zimmer or Two Steps From Hell pad.

**Synthesis chain:**
```
CinePad = 0.35*f1 + 0.25*f1detuned + 0.15*h2 + 0.12*h3 + 0.08*h4 + 0.05*tanh(0.6*h2)
f1detuned = sin(phase*(1+0.008)) + sin(phase*(1-0.008)) // wide double detune
```

**Technical parameters:**
- Triple doubling (fundamental + 2 wide detunes)
- Balanced weighting across all harmonics
- Light saturation on h2 for body
- Long envelope (attack 50ms, release 120ms)
- Wide stereo image

**Integration:** 0 additional state (static detunes based on `phase`)

**Color parameter:** Modulates the detune width (`0.008 +/- 0.004*color`) and the saturation

---

### 2.7 No. 12: "Wobble"

**Sound identity:** Pad modulated by a characteristic fast beating (analogy: detune LFO, wobble-bass style but in the mid/high range). Comparable to a Moog vibrato wobble or a very light flanger.

**Synthesis chain:**
```
// Wobble via oscillating phase
WobbleBase = 0.5*f1 + 0.3*h2 + 0.15*h3 + 0.05*h4
wobblePhase = sin(phaseSlow)
wobblePhase += phaseInc * wobbleSpeed
wobbleSpeed = 0.006 +/- 0.003*color  // ~8-15 Hz at 44.1kHz

// Fast phase modulation
wobble = WobbleBase + 0.04 * wobblePhase * sin(2*f1_phase)
```

**Technical parameters:**
- Independent slow phase accumulator (`phaseWobble` to be added)
- The accumulator modulates a small amount of harmonic content (light PM)
- Speed ~8-15 Hz (audible but not very fast)
- No saturation

**Integration:** 1 additional persistent state (`phaseWobble` per voice)

**Color parameter:** Modulates the speed (`0.006 +/- 0.003*color`) and the modulation depth

---

### 2.8 No. 13: "ReverseReverb"

**Sound identity:** Pad with a characteristic progressive swell (like reverse reverb). The sound rises organically in intensity, simulating a reversed-reverb effect. Comparable to a Cocteau Twins or Radiohead pad.

**Synthesis chain:**
```
ReverseBase = 0.5*f1 + 0.2*h2 + 0.15*h3 + 0.1*h4
reverseEnv = 1.0 - exp(-phaseRevRev)
reverseEnv = max(0.0, min(1.0, reverseEnv))
reverseEnv = smooth(reverseEnv)  // smooth envelope

phaseRevRev += phaseInc * revSpeed
revSpeed = 0.003  // ~1 cycle per bar (fast)
```

**Technical parameters:**
- Independent slow phase accumulator (`phaseRevRev` per voice)
- Rising exponential envelope applied to all samples
- Balanced weighting of fundamental + mid harmonics
- Light tanh saturation

**Integration:** 1 additional persistent state (`phaseRevRev` per voice, initialized to 0 on restart)

**Color parameter:** Modulates the speed (`0.003 +/- 0.002*color`) and the saturation

---

## 3. Technical Feasibility Verification

### 3.1 Feasibility Table

| No. | Preset | Extra States | CPU Complexity | Feasibility | Remarks |
|-----|--------|-------------------|-----------------|-------------|-----------|
| 6 | Shimmer | 1 (`shimmerPhase`) | Low (~+2 sin/cycle) | Yes | Direct phase modulation |
| 7 | Warm Sub-Pad | 0 | Very low | Yes | Direct `sin(phase/2)` computation |
| 8 | Glass | 0 | Very low | Yes | Simple combination |
| 9 | Breath | 1 (`phaseBreath`) | Low (~+2 sin/cycle) | Yes | Slow accumulator + multiplier |
| 10 | Analog Pad | 0 | Low | Yes | Fourier series approximation |
| 11 | CinePad | 0 | Low | Yes | Multiple static detune |
| 12 | Wobble | 1 (`phaseWobble`) | Medium (~+3 sin/cycle) | Caution | 3 additional sin calls |
| 13 | ReverseReverb | 1 (`phaseRevRev`) | Low | Yes | exp() + accumulator |

### 3.2 Per-Voice Memory Evolution

| State | Before | After pad addition |
|-------|-------|----------------|
| `phases` (double) | 8 bytes | 12-16 bytes (if 1-2 slow states added) |
| `amplitudes` (float) | 4 bytes | 4 bytes (unchanged) |
| `targetAmps` (float) | 4 bytes | 4 bytes (unchanged) |
| `attackSamplesRemaining` (int) | 4 bytes | 4 bytes (unchanged) |
| `attackTotalSamples` (int) | 4 bytes | 4 bytes (unchanged) |
| `attackStartAmp` (float) | 4 bytes | 4 bytes (unchanged) |
| `voicePrevGate` (uint8) | 1 byte | 1 byte (unchanged) |
| **Total per voice** | **~29 bytes** | **~33-37 bytes** (+14% max) |

**Memory risks:** Negligible. Per-voice memory increases by at most 14%.

### 3.3 CPU Risks

The only real risk is **Wobble** (No. 12), which adds 3 extra `sin()` calls per sample.

**Estimate:**
- `sin()` on x86_64 with SSE/AVX: ~15-25 cycles
- 4 voices x 3 sin x 44100 samples/s ~ 5.3 million sin/s per block
- On a 2.5 GHz CPU: ~0.2% extra core usage per block
- **Impact:** Negligible on any modern CPU, even first-generation ones

### 3.4 Integration into the Existing Architecture

**Files to modify:**
| File | Changes |
|---------|--------------|
| `HarmonyEngine.h` | add 1-2 `std::vector<double>` for slow states (max 2) |
| `HarmonyEngine.cpp` | 1. `prepare()`: resize vectors. 2. `renderHarmonies()`: additional switch cases for 6-13. 3. `setVoiceGate()`: reset slow states if needed |
| `PluginProcessor.cpp` | nothing (no new parameter, `toneMode` already exists) |
| `PluginProcessor.h` | nothing |
| `PluginEditor.cpp` | 1. Add the names in the Harmony Tone ComboBox. 2. Translate into 6 languages if needed |

**New parameters required:** None. All presets use existing parameters (`harmony_tone` choice, `harmony_tone_color` continuous).

**Backward compatibility:** All existing presets (0-5) remain unchanged. Adding cases to the `switch` is non-destructive.

**Conclusion:** The implementation is **entirely feasible without any new external DSP module**. Everything happens inside the `switch` of `renderHarmonies()`.

---

## 4. Development and Testing Plan

### 4.1 Phase 1: Basic Implementation (4 new presets)

**Batch:** Glass + Warm Sub-Pad + Analog Pad + CinePad (No. 8, 7, 10, 11)

**Safety criterion:** 0 extra state per voice. Minimal CPU complexity.

Tasks:
1. Add 4 `case` entries in the `switch (toneMode)` of `renderHarmonies()` (HarmonyEngine.cpp)
2. Add 4 presets in the ComboBox of PluginEditor.cpp
3. Translate the 4 names into 6 languages (OVTLanguages.h)
4. Unit validation tests (RMS, clipping, CPU)

**Duration:** 1 development session

### 4.2 Phase 2: Sound Identity Tests (Phase 1)

**Activity:** Side-by-side comparative listening on high-quality monitoring (headphones/open-back) and in various environments.

**Methodology:**
1. Create a test project in a DAW: sing a series of diatonic chords (Am, C, F, G)
2. For each chord, cycle through the 4 new presets at equalized volume
3. Rate each preset on:
   - Clear distinction from the other new presets (score 1-5)
   - Clear distinction from the existing presets (Choir, Vocoder-like) (score 1-5)
   - Overall sound appeal (score 1-5)
   - Perceptible issues (clipping, resonances, "dead" sounds)

**Acceptance criterion:** All presets must achieve an identity score >=3/5 against all others.

### 4.3 Phase 3: Advanced Implementation (4 new presets)

**Batch:** Shimmer + Breath + Wobble + Reverse Reverb (No. 6, 9, 12, 13)

**Criterion:** 1 extra state per voice for each. CPU validation.

Tasks:
1. Add the required `std::vector<double>` in HarmonyEngine.h
2. Initialize and manage the new states in `prepare()` and `renderHarmonies()`
3. Add the `case` entries in the `switch` (No. 6, 9, 12, 13)
4. Add the 4 new presets in the ComboBox and translate them
5. CPU validation tests with profiling

**Duration:** 1-2 development sessions

### 4.4 Phase 4: Full Comparative Tests

**Activity:** Same procedure as Phase 2 but across all 12 presets (6 existing + 8 new).

**Listening scenarios:**
1. Deep male voice (Bass/Tenor) with minor chords
2. High female voice (Alto/Soprano) with major chords
3. Monophony (single note, no chord) to evaluate the pad character
4. Fast tempo (repetitive singing) to test the attacks
5. Slow tempo (long notes) to test the note developments
6. With/without reverb to check compatibility with effects

**Acceptance criterion:** Same metric as Phase 2 (identity score >=3/5 everywhere).

### 4.5 Phase 5: Optimization and Refinements

Tasks:
1. Adjust the weightings based on listening feedback
2. Match RMS volume across all presets (gain match)
3. Adjust envelopes if necessary (some presets may benefit from specific attack/release settings)
4. Check compatibility with the different scale detection modes

**Duration:** 1 session

### 4.6 Summary Schedule

| Phase | Content | Estimated Duration | Dependencies |
|-------|---------|-----------------|-------------|
| Phase 1 | Implementation (Glass, SubPad, Analog, CinePad) | 1 session | None |
| Phase 2 | Phase 1 identity tests | 1 session | Phase 1 complete |
| Phase 3 | Advanced implementation (Shimmer, Breath, Wobble, RevRvb) | 1-2 sessions | Phase 2 validated |
| Phase 4 | Full tests, 12 presets | 1-2 sessions | Phase 3 complete |
| Phase 5 | Optimization/tweaks | 1 session | Phase 4 validated |

**Estimated total:** 4-6 development sessions + 2-4 test sessions

---

## 5. Technical Constraints and Limitations to Anticipate

### 5.1 Inherent Limitations of the Sin/Cos Architecture

| Limitation | Impact | Mitigation |
|--------|--------|-------------|
| No true waveform | Impossible to generate a real saw, square, or triangle | Fourier series approximation (limited to 4 terms) |
| No lowpass/resonant filter | Impossible to create a true "filter sweep" | Interpolation between harmonics simulates a spectral sweep |
| No delay/feedback | No chorus, flanger, phaser, natural delay | Simulated via phase detunes (artificial chorus) |
| No true LFO < 1 Hz | No truly slow melodic vibrato | Simulated by a very-low-speed phase accumulator |

### 5.2 Sound Quality Risks

**Risk 1: Timbre confusion**
Presets using the same harmonics with different weights risk sounding too similar (e.g., Glass vs CinePad may sound close).
- *Mitigation:* Strongly differentiate the weights and use secondary effects (saturation for CinePad, no saturation for Glass)

**Risk 2: Clipping/Overload**
tanh saturation can create peaks if the amplitude is not properly controlled.
- *Mitigation:* Limit the tanh input amplitude (<=1.5) and keep the existing clamp (lines 417-419 of HarmonyEngine.cpp)

**Risk 3: Accumulator synchronization**
The slow accumulators (breath, shimmer, wobble, reverse) can synchronize across voices if initialized deterministically.
- *Mitigation:* Initialize randomly (`random.nextDouble() * twoPi`) or offset by voice index (`phase * (1.0 + 0.001 * voiceIndex)`)

**Risk 4: Frequent resonance**
Some harmonic combinations can create unwanted beats (beat frequencies) between voices close to each other within the same harmony.
- *Mitigation:* Keep detunes < 10 cents and make sure the detuned presets (Shimmer, Analog, CinePad) are not used with tight harmonies (octaves)

### 5.3 Hardware Audio Constraints

| Constraint | Impact |
|-----------|--------|
| Sampling frequency | At 44.1 kHz, harmonic 4 at 200 Hz = 800 Hz (OK). At 44.1 kHz, harmonic 4 at 700 Hz = 2800 Hz (still OK, but near Nyquist if f0 > 1000 Hz) |
| Stereo output | All presets use the existing fixed panning. None require dynamic panning |
| Latency | The new presets add virtually no latency (no buffering) |

### 5.4 Usage Scenarios to Avoid

**Avoid using:**
- **Wobble** on very tight intervals (thirds, unison): the beats add up instead of combining harmoniously
- **Glass** on very high notes (> 800 Hz): harmonics 3 and 4 approach the Nyquist limit
- **Shimmer** with octave harmonies: the upper sub-octave can create an unwanted "ring mod" effect with the other voices
- **Warm Sub-Pad** in mono output: the sub-oscillator one octave below can become inaudible or cause phase problems

---

## 6. Executive Summary

### Constraints Summary
- **Current architecture:** Pure sinusoidal additive synthesis, no filter and no external LFO
- **Main constraint:** Everything must run in an O(1) per-sample loop, with < 2 doubles of extra state per voice
- **Feasibility:** Yes - **100% feasible without any new external DSP module**

### Summary of the 8 Proposed Presets

| No. | Name | Type | Complexity | Extra States | Risk |
|-----|-----|------|-------------|-------------|--------|
| 6 | Shimmer | Octave-doubled ethereal pad | Low | 1 | Very low |
| 7 | Warm Sub-Pad | Warm deep pad | Very low | 0 | None |
| 8 | Glass | Bright crystalline pad | Very low | 0 | None |
| 9 | Breath | Oscillating breathy pad | Low | 1 | Low |
| 10 | Analog Pad | Warm triangle pad | Low | 0 | Very low |
| 11 | CinePad | Grand orchestral pad | Low | 0 | Very low |
| 12 | Wobble | Beat-modulated pad | Medium | 1 | Medium |
| 13 | Reverse Reverb | Progressive swell pad | Low | 1 | Low |

### Recommended Deployment Plan
1. **Immediate:** Implement the 4 presets without extra state (No. 8, 7, 10, 11)
2. **Post-validation:** Implement the 4 presets with an extra state (No. 6, 9, 12, 13)
3. **Project total:** 4-6 development sessions + 2-4 test sessions

### Estimated Cost (in development resources)
- Code: ~300-500 lines of changes (HarmonyEngine.h/cpp + PluginEditor.cpp + OVTLanguages.h)
- Tests: ~2-3 days of comparative listening
- Overall risk: **Low** (provided Phase 1 is validated before moving on to Phase 3)

---

*End of report.*
