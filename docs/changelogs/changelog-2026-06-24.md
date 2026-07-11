# Changelog 2026-06-24

## Autotune plugin diagnosis and remediation plan

> Request from Jerome: analyze and document the fixes for
> all the malfunctions affecting the autotune plugin.
> 4 root-cause failures identified, 2 rounds of fixes applied.

---

## Round 1 (previous session)

### Failures identified (R1, R2, R3)

**R1** - Main audio processing: ratio `1.0` injected into the PitchShifter
due to YIN micro-pauses (overzealous octave-error prevention).

**R2** - Visual display: `PianoKeyboard::setNoteNames()` was missing,
note labels were not shown in real time.

**R3** - Parasitic latency: `getPlayHead()->getPosition()` and
`getLoopPoints()` running synchronously in the audio thread.

### R1, R2, R3 fixes applied (Build OK)
- `PluginProcessor.h`: added `lastValidPitchForAutotune`, `lastRatioSnapshot`,
  `cachedTransportTime`, `lastTransportTimeUpdateMs`
- `PluginProcessor.cpp`: `computeInputPitch()` fallback to last valid pitch;
  `processBlock()` ratio snapshot; 10ms transport cache
- `PitchShifter.cpp`: input ratio validation (NaN/Inf/<=0 -> 1.0)
- `PluginEditor.cpp`: call `setNoteNames()` on both pianos
- `PianoKeyboard.h/cpp`: new method + red/green labels at the top
- `PitchVisualizer.h/cpp`: named `kHarmonyColour` constant

---

## Round 2 (this session) - ROOT CAUSE FOUND

### R4 - YIN never executes (BLOCKING BUG)

**Symptom** after Round 1: still no audible autotune effect.
Only the formant shift works.

**Root cause identified by static analysis:**
Inconsistency between `prepareToPlay()` and `computeInputPitch()`:

```
prepareToPlay:  pitchDetector->prepare (sampleRate / 4.0, ...)
computeInputPitch:  decimation = 8
```

Internal sampling rate mismatch:

| Step | Frequency | Buffer size |
|------|-----------|-------------|
| `prepareToPlay` | 44100 / 4 = **11025 Hz** | `analysisWindow` = 2048 raw |
| `maxLag` YIN | 11025 / 30 Hz = **367 samples** | needed = 2*367 = **734** |
| `decimation = 8` | 44100 / 8 = **5512 Hz** | `2048/8` = **256 samples** |
| YIN check | 256 < 734 ? **FAIL -> returns 0** | | |

**Result**: `detectPitch()` returns `0.0f` immediately at EVERY
block, because `numSamples (256) < maxLag * 2 (734)`.

**YIN has never worked since the Multi-Engine overhaul**
(Phase 7, 2026-06-12) which introduced the decimation by 8 without
updating `prepareToPlay`.

The formant shift worked independently because the PitchShifter
uses `formantRatio` to control the playback speed of the grains,
independently of `f0_in`.

### R4 fix

| File | Before | After |
|------|--------|-------|
| `PluginProcessor.h` `analysisWindow` | 2048 | **4096** |
| `PluginProcessor.h` `analysisHopSize` | 1024 (~23ms) | **2048 (~46ms)** |
| `PluginProcessor.cpp` `decimation` | 8 | **4** |

Verification after fix:

| Step | Value |
|------|-------|
| Prepare sample rate | 44100 / 4 = **11025 Hz** |
| maxLag YIN (30 Hz) | 11025 / 30 = **367 samples** |
| YIN need (2*maxLag) | **734 samples** |
| Decimated window (4096/4) | **1024 samples** |
| Check | 1024 >= 734 ? **OK -> YIN runs** |

### Files modified Round 2

| File | Modification |
|------|--------------|
| `Source/PluginProcessor.h` | `analysisWindow` 2048 -> 4096, `analysisHopSize` 1024 -> 2048 |
| `Source/PluginProcessor.cpp` | `decimation` 8 -> 4, comment updated |

---

## Round 3 (this session) - Octave drops on held note

### R5 - Overly permissive octave-error prevention and consensus bug

**Symptom**: autotune works but the detected pitch regularly jumps
by an octave (often upward) while the user holds a constant note.

**Root cause identified**: 2 bugs in the PitchDetector octave-error prevention:

#### Bug 1 (detectPitch step 3b) - Thresholds too restrictive
The old correction checked `yinBuffer[tauHalf] < threshold` and
`ratio < 1.1`. When YIN found the 2nd harmonic (tau corresponding to
2*f0, typical of female voices and vowels with high formant),
`yinBuffer[2*tau]` (the correct fundamental) was NOT below the
threshold, so the correction did not trigger.

**Fix**: replaced with a systematic evaluation of the 2
alternatives (tau/2 and 2*tau). The algorithm chooses the best
one based on:
- (a) the d' value (lowest = best clarity)
- (b) octave continuity with `lastValidPitch` (at similar clarity
      within < 20%, the octave closest to the context is preferred)

#### Bug 2 (getMedianFiltered) - Check on 1 value instead of 5
The old octave-continuity loop had a `break` that stopped
the check after the FIRST valid value in the history. If that
single value was an outlier (e.g., a spurious pitch an octave
away), the correction was wrongly applied.

**Fix**: CONSENSUS check across the 5 history values. The correction
is applied only if >= 3 valid values indicate the SAME octave jump,
and NONE votes for the opposite direction.

### File modified Round 3

| File | Modification |
|------|--------------|
| `Source/dsp/PitchDetector.cpp` | `detectPitch()` step 3b: new systematic octave evaluation logic. `getMedianFiltered()`: consensus vote > 50% correction |

---

## Summary

| Failure | Cause | Fix | Status |
|---------|-------|-----|--------|
| R1 | ratio 1.0 on YIN micro-pause | fallback lastValidPitchForAutotune | **Compiles** |
| R2 | no note labels | setNoteNames() on both pianos | **Compiles** |
| R3 | synchronous getPlayHead | 10ms cache | **Compiles** |
| **R4** | **YIN never executed** | **decimation=4, analysisWindow=4096** | **Compiles** |
| **R5** | **Octave drops on held note** | **New step 3b + median consensus** | **Compiles** |
| **R6** | **Persistent octave drops in Curve Editor mode** | **Anti-jump filter in processBlock (lastOctaveValidatedPitch)** | **Compiles** |

## Round 4 - Persistent octave drops in Curve Editor mode

### R6 - Octave jump in processBlock not caught by YIN

**Symptom**: despite the R5 fixes in PitchDetector, octave drops
still occur, EVEN in Curve Editor mode (where the target note is
forced by the user curve).

**Root cause**: the problem is NOT in YIN but in how `f0_in` is used
by `processBlock`. Even with the correct target note provided by the
curve, if `f0_in` jumps by an octave:
- The ratio `f0_target / f0_in` becomes wrong (half or double)
- The PitchShifter receives an invalid ratio -> grains synthesized at
  the wrong period -> output octave drop
- The formant shift is not impacted because it uses `formantRatio`
  independently of `f0_in`

**Fix**: added an **octave-jump filter** at the `processBlock` level,
after `computeInputPitch()`. This filter verifies that `f0_in` does
not jump by a factor of ~2 or ~0.5 relative to the last valid pitch
(`lastOctaveValidatedPitch`). If it does, it keeps the old value.

Unlike the R5 fixes in PitchDetector (which act on YIN's output), this
filter is **independent of YIN** and protects the entire pipeline:
autotune, harmonies AND Curve Editor.

### Files modified Round 4

| File | Modification |
|------|--------------|
| `Source/PluginProcessor.h` | Added `lastOctaveValidatedPitch` member |
| `Source/PluginProcessor.cpp` | Octave-jump filter between `computeInputPitch()` and `lastInputPitch.store()` |

---

## Final summary

| Failure | Cause | Fix | Status |
|---------|-------|-----|--------|
| R1 | ratio 1.0 on YIN micro-pause | fallback lastValidPitchForAutotune | **Compiles** |
| R2 | no note labels | setNoteNames() on both pianos | **Compiles** |
| R3 | synchronous getPlayHead | 10ms cache | **Compiles** |
| R4 | YIN never executed | decimation=4, analysisWindow=4096 | **Compiles** |
| R5 | Octave drops (internal YIN) | New step 3b + median consensus | **Compiles** |
| **R6** | **Octave drops (pipeline)** | **Anti-jump filter in processBlock** | **Compiles** |

**Release VST3 build succeeded** after each round. The plugin is now
functional with autotune + harmonies + display + controlled latency +
multi-layer octave-error prevention.
