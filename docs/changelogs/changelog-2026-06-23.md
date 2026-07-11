# Changelog 2026-06-23

## Pitch Detector — Octave-error prevention via octave continuity

### Problem
The YIN algorithm mistakenly detects an octave too high on low notes (e.g., sung F#1 -> detected as F#2) when the fundamental is weak and the first harmonic dominates. The existing octave-error prevention (step 3b) only corrects downward jumps (detects 2*f0 instead of f0), not upward ones.

### Solution
Added an octave-continuity check in `PitchDetector::getMedianFiltered()`:
- If the median differs from the last valid pitch by a factor of ~2 (octave above) or ~0.5 (octave below), it is adjusted toward the octave closest to the previous context.
- New `lastValidPitch` member to track the last valid pitch.

### Files modified
- `Source/dsp/PitchDetector.h`: added `lastValidPitch` member
- `Source/dsp/PitchDetector.cpp`: bidirectional octave correction in `getMedianFiltered()`, updated `lastValidPitch` in `detectPitch()`
