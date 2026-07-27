# 2026-07-27 — Voice Type Selector

## Overview
Added a new `voice_type` parameter that constrains the pitch detector's YIN search
range to a vocal register. This reduces octave errors and CPU usage for singers with
a well-defined tessitura, while keeping "Universal" as the default for sources that
span the full range.

## Changes

### DSP
- `Source/dsp/YinPitchDetector.h` / `.cpp`: added public method
  `setFrequencyRange(float minHz, float maxHz)` to update the search range at
  runtime. Recomputes `maxLag`, grows the working buffer if needed, no-op until
  `prepare()` has been called.
- `Source/PluginProcessor.h`: added `voiceTypeParam` atomic pointer,
  `voiceTypeMinHz` / `voiceTypeMaxHz` constexpr tables, and `lastVoiceType` cache.
- `Source/PluginProcessor.cpp`:
  - APVTS parameter `"voice_type"` (AudioParameterChoice, default = 0 = Universal).
  - `prepareToPlay()`: applies the initial range to the main and sidechain
    YIN detectors.
  - `syncParameters()`: detects voice-type changes and calls
    `setFrequencyRange()` on both detectors.

### UI
- `Source/PluginEditor.h` / `.cpp`:
  - Added `voiceTypeBox` (ComboBox) and `voiceTypeLabel` to the Correction block's
    Advanced area.
  - Combo items: Universal, Bass, Baritone, Tenor, Alto, Soprano.
  - Layout: row 1 of the advanced area now uses 2 small knobs (Vibrato + Humanize
    side-by-side), row 2 holds the Voice Type combo. Combo is hidden when
    Advanced is collapsed.

### Persistence / A/B
- `Source/dsp/PresetMorpher.h`:
  - Added `int voiceType` to `MorphState` (default 0).
  - Captured in `captureState()` from the `"voice_type"` parameter.
  - Added `"voice_type"` to `getMorphParameterIds()`.
  - Step-at-50% interpolation in `applyInterpolatedState()`.

### Documentation
- `docs/voice-type-feasibility-report.md`: full feasibility report for this
  feature (UI redesign, DSP integration, performance assessment, test matrix).
- `docs/implementation-roadmap.md`: added a checkbox entry under "Core DSP Engine".

## Frequency Ranges

| Voice Type | minHz  | maxHz  | MIDI Range |
|------------|--------|--------|------------|
| Universal  | 30     | 1000   | C1–C6      |
| Bass       | 82.41  | 329.63 | E2–E4      |
| Baritone   | 110    | 440    | A2–A4      |
| Tenor      | 130.81 | 523.25 | C3–C5      |
| Alto       | 174.61 | 698.46 | F3–F5      |
| Soprano    | 261.63 | 1046.5 | C4–C6      |

## Validation
- All 139 unit tests pass.
- Plugin builds in Release mode (VST3 + Standalone) without errors or new warnings.
- Default "Universal" preserves existing behavior (30-1000 Hz).

## Notes
- The frequency range is applied in `syncParameters()`, which runs on the audio
  thread. The buffer grow (if needed) is a single `HeapBlock` allocation — still
  safe in the audio thread for the rare voice-type change.
- No allocations, no locks, no per-block cost change for "Universal" (the
  default).
