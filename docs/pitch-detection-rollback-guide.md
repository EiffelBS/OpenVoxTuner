# Pitch Detection Rollback Guide — YIN / SWIPE' / PYIN

> **Status update (2026-07-02):** PYIN has been permanently removed due to unresolved memory crashes. SWIPE' has been re-enabled and optimized (energy calculation extracted from the candidate loop, dead code removed in interpolation). See `docs/changelogs/changelog-2026-07-02.md` for details.

## Overview

This document describes the pitch detection migration from **YIN** to **SWIPE' and PYIN** and provides the complete procedure to roll back to YIN if the new algorithms exhibit critical issues in production.

## Architecture Change

### Before (original)

```
Source/dsp/PitchDetector.h     — class atdsp::PitchDetector (YIN algorithm)
Source/dsp/PitchDetector.cpp
Source/PluginProcessor.h       — std::unique_ptr<atdsp::PitchDetector> pitchDetector;
```

### After (refactored)

```
Source/dsp/IPitchDetector.h      — abstract interface (base class)
Source/dsp/YinPitchDetector.h    — class atdsp::YinPitchDetector : IPitchDetector (YIN, identical logic)
Source/dsp/YinPitchDetector.cpp
Source/dsp/SwipePitchDetector.h  — class atdsp::SwipePitchDetector : IPitchDetector (SWIPE' spectral)
Source/dsp/SwipePitchDetector.cpp
Source/dsp/PyinPitchDetector.h   — class atdsp::PyinPitchDetector : IPitchDetector (PYIN probabilistic)
Source/dsp/PyinPitchDetector.cpp
Source/PluginProcessor.h         — std::unique_ptr<atdsp::IPitchDetector> pitchDetector;
```

### Files preserved (for rollback reference)

The original `Source/dsp/PitchDetector.h` and `Source/dsp/PitchDetector.cpp` files are **preserved unchanged** in the repository. They are no longer compiled into the plugin binary but serve as the canonical YIN codebase for rollback.

## Rollback Mechanisms

There are **three levels** of rollback, from least to most invasive:

### Level 1 — Runtime Parameter Rollback (no rebuild needed)

The `pitch_detector` parameter in the plugin UI allows switching between YIN, SWIPE' and PYIN at runtime:

- **Value 0** = YIN (default, safe)
- **Value 1** = SWIPE' (spectral, FFT-based)
- **Value 2** = PYIN (probabilistic, HMM-smoothed)

To switch: change the parameter in the plugin UI or automate it from the DAW.

**Additionally**, an automatic rollback is built in:
- If SWIPE' fails to detect pitch for more than ~2 seconds of non-silent input, the system **automatically falls back to YIN** and resets the parameter to 0.
- This is controlled by `swipeRollbackCounter` in `PluginProcessor.h` (threshold: 46 blocks at 2048 samples / 44.1 kHz).

### Level 2 — Source-level Rollback (rebuild required, fast)

If SWIPE' or PYIN have compilation or runtime issues, remove them from the build and force YIN everywhere:

**Step 1 — Remove SWIPE' and PYIN from CMakeLists.txt**
```
File: CMakeLists.txt
-   Source/dsp/SwipePitchDetector.cpp
-   Source/dsp/SwipePitchDetector.h
-   Source/dsp/PyinPitchDetector.cpp
-   Source/dsp/PyinPitchDetector.h
```

**Step 2 — Remove detector parameter**
```
File: Source/PluginProcessor.cpp
-   , std::make_unique<juce::AudioParameterChoice> (
-         "pitch_detector", "Pitch Detector",
-         juce::StringArray { "YIN", "SWIPE'", "PYIN" }, 0)
```

**Step 3 — Change includes in PluginProcessor.h**
```
File: Source/PluginProcessor.h
-   #include "dsp/SwipePitchDetector.h"
-   #include "dsp/PyinPitchDetector.h"
```

**Step 4 — Simplify detector creation**
```
File: Source/PluginProcessor.cpp
-   // In createDetector(), remove the SWIPE' and PYIN branches:
-   if (mode == 1) { ... }
-   else if (mode == 2) { ... }
+   det = std::make_unique<atdsp::YinPitchDetector>();
```

**Step 5 — Remove rollback logic**
```
File: Source/PluginProcessor.cpp
-   Remove the "PITCH DETECTOR SWITCHING" block in processBlock()
File: Source/PluginProcessor.h
-   Remove: std::atomic<float>* detectorParam = nullptr;
-   Remove: int swipeRollbackCounter = 0;
```

**Step 6 — Rebuild**
```bash
cmake --build build --config Release
```

### Level 3 — Full Restore to Original (rebuild required, safest)

Revert to the original monolithic `PitchDetector` class with no interface layer.

**Step 1 — Restore CMakeLists.txt**
```
File: CMakeLists.txt
+   Source/dsp/PitchDetector.cpp
+   Source/dsp/PitchDetector.h
-   Source/dsp/IPitchDetector.h
-   Source/dsp/YinPitchDetector.cpp
-   Source/dsp/YinPitchDetector.h
-   Source/dsp/SwipePitchDetector.cpp
-   Source/dsp/SwipePitchDetector.h
-   Source/dsp/PyinPitchDetector.cpp
-   Source/dsp/PyinPitchDetector.h
```

**Step 2 — Restore PluginProcessor.h include**
```
File: Source/PluginProcessor.h
-   #include "dsp/IPitchDetector.h"
-   #include "dsp/YinPitchDetector.h"
-   #include "dsp/SwipePitchDetector.h"
-   #include "dsp/PyinPitchDetector.h"
+   #include "dsp/PitchDetector.h"
```

**Step 3 — Restore member type**
```
File: Source/PluginProcessor.h
-   std::unique_ptr<atdsp::IPitchDetector> pitchDetector;
+   std::unique_ptr<atdsp::PitchDetector> pitchDetector;
```

**Step 4 — Remove helper method**
```
File: Source/PluginProcessor.h
-   std::unique_ptr<atdsp::IPitchDetector> createDetector(int mode);
File: Source/PluginProcessor.cpp
-   Remove the createDetector() implementation
```

**Step 5 — Restore factory call**
```
File: Source/PluginProcessor.cpp
-   pitchDetector = createDetector(0);
+   pitchDetector = std::make_unique<atdsp::PitchDetector>();
```

**Step 6 — Remove parameter and rollback code**
```
File: Source/PluginProcessor.cpp
-   Remove the "pitch_detector" parameter from constructor initializer
-   Remove: detectorParam = parameters.getRawParameterValue("pitch_detector");
-   Remove the entire "PITCH DETECTOR SWITCHING" block in processBlock()
File: Source/PluginProcessor.h
-   Remove: std::atomic<float>* detectorParam = nullptr;
-   Remove: int swipeRollbackCounter = 0;
```

**Step 7 — Restore prepareToPlay**
```
File: Source/PluginProcessor.cpp
-   Restore:
+   pitchDetector->prepare(sampleRate / 4.0, samplesPerBlock);
```

**Step 8 — Remove unused source files**
```
Delete: Source/dsp/IPitchDetector.h
Delete: Source/dsp/YinPitchDetector.cpp
Delete: Source/dsp/YinPitchDetector.h
Delete: Source/dsp/SwipePitchDetector.cpp
Delete: Source/dsp/SwipePitchDetector.h
Delete: Source/dsp/PyinPitchDetector.cpp
Delete: Source/dsp/PyinPitchDetector.h
```

**Step 9 — Clean build**
```bash
cmake -B build -S . -G "Visual Studio 17 2022" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Verification Points

After each rollback level, verify:

| Check | Method | Expected Result |
|-------|--------|-----------------|
| **Build succeeds** | `cmake --build build --config Release` | Exit code 0 |
| **Plugin loads** | Launch DAW or standalone | No crash on instantiation |
| **Pitch detection works** | Sing into the plugin | Visualizer shows pitch tracking |
| **No glitches** | Check "processBlock" log | No "SWIPE' rollback" messages |
| **CPU level** | Check DAW CPU meter | Similar to original YIN-only build |

## Incident Management During Rollback

### Rollback is in progress but the plugin won't load
1. Check the log file at `Documents/OpenVoxTuner.log` for error messages.
2. If the error is in `SwipePitchDetector`, perform Level 2 rollback (remove SWIPE' only).
3. If the error is in the interface layer, perform Level 3 rollback (full restore).

### The build fails after rollback
1. Ensure all files from the "Remove" sections have been removed.
2. Ensure all files from the "Add" sections have been added.
3. Run a clean cmake configuration first: `cmake -B build -S .`
4. If using Visual Studio, delete the `build` directory and reconfigure.

### After Level 1 auto-rollback, the parameter stays at 0
1. The auto-rollback resets the parameter to 0 in the DAW. The user can switch back to SWIPE' at any time by changing the parameter back to 1.
2. If the auto-rollback triggers immediately on every block, SWIPE' has a fundamental issue — perform Level 2 or Level 3 rollback.

## File Change Log

| File | Status | Description |
|------|--------|-------------|
| `Source/dsp/IPitchDetector.h` | **New** | Abstract interface for pitch detectors |
| `Source/dsp/YinPitchDetector.h` | **New** | YIN implementation (refactored from PitchDetector) |
| `Source/dsp/YinPitchDetector.cpp` | **New** | YIN implementation (refactored from PitchDetector) |
| `Source/dsp/SwipePitchDetector.h` | **New** | SWIPE' spectral implementation |
| `Source/dsp/SwipePitchDetector.cpp` | **New** | SWIPE' spectral implementation |
| `Source/dsp/PyinPitchDetector.h` | **New** | PYIN probabilistic implementation |
| `Source/dsp/PyinPitchDetector.cpp` | **New** | PYIN probabilistic implementation |
| `Source/dsp/PitchDetector.h` | **Preserved** | Original YIN (rollback reference, not compiled) |
| `Source/dsp/PitchDetector.cpp` | **Preserved** | Original YIN (rollback reference, not compiled) |
| `Source/PluginProcessor.h` | **Modified** | Changed member type + added detectorParam + swipeRollbackCounter + createDetector() |
| `Source/PluginProcessor.cpp` | **Modified** | Added parameter + factory + switching logic + auto-rollback |
| `Source/PluginEditor.h` | **Modified** | Added detectorBox ComboBox and detectorAttachment |
| `Source/PluginEditor.cpp` | **Modified** | Added detector selector UI in middle block layout |
| `CMakeLists.txt` | **Modified** | Replaced PitchDetector with YinPitchDetector + SwipePitchDetector + PyinPitchDetector |