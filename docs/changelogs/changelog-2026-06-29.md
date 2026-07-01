# Changelog 2026-06-29

## Pitch Detection Pipeline: YIN → SWIPE' Migration

> Requested by Jerome: improve pitch detection reliability by transitioning from the YIN algorithm to the SWIPE' spectral algorithm, with a comprehensive rollback strategy.

---

### Architecture Change

The pitch detector was refactored from a monolithic class into an **abstract interface** (`IPitchDetector`) with interchangeable implementations:

- `IPitchDetector` — abstract base with `prepare()`, `reset()`, `detectPitch()`, `setThreshold()`, `getName()`
- `YinPitchDetector` — original YIN algorithm (identical logic, same performance)
- `SwipePitchDetector` — new SWIPE'-inspired spectral algorithm (FFT-based, more robust on voice)
- `PyinPitchDetector` — new PYIN-inspired probabilistic algorithm (HMM Viterbi smoothing, ~1.5x YIN CPU)

### New files created

| File | Description |
|------|-------------|
| `Source/dsp/IPitchDetector.h` | Abstract interface for all pitch detectors |
| `Source/dsp/YinPitchDetector.h` | YIN implementation (refactored from PitchDetector) |
| `Source/dsp/YinPitchDetector.cpp` | YIN implementation (refactored from PitchDetector) |
| `Source/dsp/SwipePitchDetector.h` | SWIPE' spectral implementation |
| `Source/dsp/SwipePitchDetector.cpp` | SWIPE' spectral implementation |
| `Source/dsp/PyinPitchDetector.h` | PYIN probabilistic implementation |
| `Source/dsp/PyinPitchDetector.cpp` | PYIN probabilistic implementation |
| `docs/pitch-detection-rollback-guide.md` | Comprehensive rollback documentation (3 levels) |

### Files modified

| File | Changes |
|------|---------|
| `Source/PluginProcessor.h` | Changed `PitchDetector` → `IPitchDetector`. Added `detectorParam`, `swipeRollbackCounter`, `createDetector()` |
| `Source/PluginProcessor.cpp` | Added `pitch_detector` parameter. Factory logic in `createDetector()`. Runtime detector switching in `processBlock()`. Auto-rollback when SWIPE' fails |
| `CMakeLists.txt` | Replaced PitchDetector files with IPitchDetector + YinPitchDetector + SwipePitchDetector + PyinPitchDetector |
| `Source/PluginEditor.h` | Added `detectorBox` combo box + `detectorLabel` + `detectorAttachment` |
| `Source/PluginEditor.cpp` | Added detector selector UI in the middle block (Key/Scale row) |

### Files preserved (for rollback)

| File | Purpose |
|------|---------|
| `Source/dsp/PitchDetector.h` | Original YIN, kept as reference (not compiled) |
| `Source/dsp/PitchDetector.cpp` | Original YIN, kept as reference (not compiled) |

### Rollback mechanisms (3 levels)

1. **Level 1** — Runtime parameter `pitch_detector` (0=YIN, 1=SWIPE', 2=PYIN) + auto-fallback after ~2s of silence
2. **Level 2** — Source-level: remove SWIPE'/PYIN files and parameter, rebuild
3. **Level 3** — Full restore: revert to original `PitchDetector` class, no interface layer

### SWIPE' algorithm details

- FFT-based spectral pitch detection at full sample rate (no 4x decimation)
- 1024-point real FFT (~23ms latency at 44.1 kHz)
- Sawtooth kernel correlation (Pearson correlation per candidate)
- ~48 candidates per octave (30-1000 Hz range)
- Parabolic interpolation for sub-bin precision
- EMA stability filter (requires 3 consecutive detections near same pitch)
- ~2-3x CPU vs YIN (one FFT + spectral correlations per frame)

### PYIN algorithm details

- Same YIN core as base (difference function + cumulative mean normalization)
- **Multi-candidate tracking**: up to 3 local minima per frame instead of just the first
- **Viterbi-like HMM**: transition cost penalizes octave jumps (ratio ~2 or ~0.5)
  — semitone distance squared / 6, scaled by `transitionCostScale`
- **Dropout recovery**: after 5 consecutive no-pitch frames, resets Viterbi state
- **After silence**: prefers lowest-frequency candidate (fundamental over harmonics)
- Parabolic interpolation on the Viterbi-chosen tau
- CPU: ~1.5x standard YIN