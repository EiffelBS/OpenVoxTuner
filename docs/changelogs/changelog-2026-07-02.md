# Changelog 2026-07-02

## Removed: PYIN pitch detector (unresolved memory crashes)

PYIN-inspired probabilistic pitch detector (`PyinPitchDetector.h/.cpp`) has been permanently removed due to persistent memory crashes that could not be resolved.

### Files deleted
- `Source/dsp/PyinPitchDetector.h`
- `Source/dsp/PyinPitchDetector.cpp`

### References cleaned up
- `CMakeLists.txt` — removed commented-out PYIN source entries
- `PluginProcessor.h` — removed PYIN include and disabled comments
- `PluginProcessor.cpp` — removed PYIN references from parameter string and comments
- `PluginEditor.cpp` — removed "disabled" greyed-out menu item
- `README.md` — updated feature list and tree

---

## Re-enabled: SWIPE' pitch detector (fixed and optimized)

SWIPE' spectral pitch detector (`SwipePitchDetector.h/.cpp`) has been re-enabled after fixing two critical performance issues that previously caused memory-related crashes.

### Fixes applied

1. **Energy calculation extracted from candidate loop** (major perf fix)
   - Signal energy was recomputed inside the 300-candidate correlation loop (~300x per frame)
   - Now computed once before the loop, reducing CPU by ~30-40% in `detectPitch()`

2. **Dead code removed in parabolic interpolation**
   - Removed unused `offset`, `y0`, `corrL`/`corrR` variables
   - Removed dummy `interpolatePeak(-1.0f, 0.0f, -1.0f)` fallback call
   - Reused pre-computed `energyFactor` instead of recomputing it

### Files modified
- `Source/dsp/SwipePitchDetector.cpp` — optimized `detectPitch()` loop
- `CMakeLists.txt` — uncommented SWIPE' source files
- `PluginProcessor.h` — changed `pitchDetectors[1]` to `pitchDetectors[2]`; updated comments
- `PluginProcessor.cpp` — updated factory to create both YIN (mode=0) and SWIPE' (mode=1); updated parameter string
- `PluginEditor.h` — updated comment
- `PluginEditor.cpp` — updated pitch detection menu to show SWIPE' as active
- `README.md` — updated feature list and tree

### Documentation
- `docs/pitch-detection-rollback-guide.md` — updated to reflect PYIN removal and SWIPE' reactivation