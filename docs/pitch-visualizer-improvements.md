# Pitch Visualizer Improvements

> Date: 2026-07-07
> Files modified: `Source/ui/PitchVisualizer.h`, `Source/ui/PitchVisualizer.cpp`

## Overview

This document describes the comprehensive improvements made to the Pitch Visualizer
(the "Live" tab of the OpenVoxTuner plugin). The changes address octave reference
line display, keyboard shortcut readability, scroll/zoom controls, and legend clarity.

---

## 1. Octave Reference Lines (Dynamic Grid)

### Problem

The original implementation used 5 hardcoded frequencies for the horizontal grid
lines (C2=65.4 Hz, C3=130.8 Hz, C4=261.6 Hz, C5=523.3 Hz, C6=1046.5 Hz).
These lines did not adapt when the user zoomed in or scrolled to a different
frequency range. When zoomed into a narrow range (e.g. 200-400 Hz), no reference
lines might be visible at all. When zoomed out beyond C2-C6, no additional lines
appeared.

### Solution

The reference lines are now computed dynamically from the visible frequency range:

1. Convert `fMin` and `fMax` to MIDI note numbers.
2. Find the first C note (MIDI note divisible by 12) >= the lowest visible MIDI.
3. Draw a horizontal line at each C note up to the highest visible MIDI.
4. Each line is labelled with its octave (e.g. "C3", "C4").

This ensures that reference lines are always visible and correctly positioned
regardless of the zoom/scroll state.

### Implementation

```cpp
// In PitchVisualizer::paint(), within the plot area section:
const int lowestMidi  = (int) std::ceil(hzToMidiFloat(fMin));
const int highestMidi = (int) std::floor(hzToMidiFloat(fMax));
const int firstC = (lowestMidi % 12 == 0) ? lowestMidi
                   : lowestMidi + (12 - lowestMidi % 12);
for (int midi = firstC; midi <= highestMidi; midi += 12) {
    // Draw line + label "C{n}"
}
```

### Alignment with Piano Keyboard

Both the visualizer's `hzToY()` and the piano keyboard's `midiToY()` use the
same linear mapping from MIDI number to pixel position:

```
y = height * (1.0 - (midi - lowestMidi) / (highestMidi - lowestMidi))
```

Since octave reference lines are computed from exact MIDI C note numbers,
they align perfectly with the C key boundaries on the vertical piano.

---

## 2. Scale Note Lines (Dynamic)

### Problem

Scale note lines (the dimmer horizontal lines for non-C notes in the active
scale) were hardcoded to iterate octaves 2-5 only. When the user zoomed into
higher or lower ranges, no scale lines appeared.

### Solution

Scale note lines now iterate over every MIDI note number within the visible
range (`lowestMidi` to `highestMidi`), checking each against the current
`scaleIntervals` array. C notes are skipped since they are already drawn as
octave reference lines.

---

## 3. Keyboard Shortcut Text Fix

### Problem

The text "MouseWheel: Scroll | Ctrl+MouseWheel: Zoom" was rendered in a single
line with a fixed 200px width. On narrower windows or when the plot area was
small, the text was truncated (showing "MouseWheel: Scroll | Ctrl+MouseWhee"
with the end cut off).

### Solution

- Split into two separate lines: "MouseWheel: Scroll" and "Ctrl+Wheel: Zoom"
- Reduced font size from 11pt to 9pt
- Positioned with proper bounds that accommodate both lines

---

## 4. Scroll/Zoom Control Buttons

### Purpose

Not all users are familiar with mouse wheel + modifier key combinations. The
new buttons provide an accessible, visible alternative for navigating the
visualizer.

### Buttons

| Button | Symbol | Action |
|--------|--------|--------|
| Scroll Up | Up arrow | Pan toward higher pitches (15% of range) |
| Scroll Down | Down arrow | Pan toward lower pitches (15% of range) |
| Zoom In | + | Narrow frequency range by 30% |
| Zoom Out | - | Widen frequency range by 40% |
| Reset | Reset arrow | Restore default 50 Hz - 1500 Hz view |

### Styling

- Semi-transparent blue background (`0x331A9AF0`)
- Light grey text, white on hover/active
- 20x20px buttons with 3px gaps
- Positioned in the top-right corner of the header strip
- Tooltips explain each button's function

### Behavior

All button actions respect the same frequency limits (16.35 Hz - 8372 Hz) as
the mouse wheel handler. The piano keyboard range is updated after each
operation. The `resetView()` method restores the default range defined by
`kDefaultFMin` (50 Hz) and `kDefaultFMax` (1500 Hz).

---

## 5. Legend + Tuning Statistics Panel

### Before

- Two text labels: "Input" and "Output"
- Single-line keyboard shortcut hint (prone to truncation)

### After

Compact 160x52px panel with semi-transparent background (3 rows):
- **Row 1**: Input (pink), Output (green), Harm. (blue) - abbreviated curve labels
- **Row 2**: "Wheel: Scroll" / "Ctrl+Whl: Zoom" - compact keyboard hints
- **Row 3**: Tuning statistics (rolling average, in-tune percentage)

The panel replaces the previous 2-row legend with an expanded 3-row layout that includes real-time tuning accuracy metrics.

### Tuning Statistics

A rolling window of 300 samples (~10 seconds at 30fps) tracks cents offsets from the nearest quantized note:

- **In-tune percentage**: Percentage of samples within +/- 15 cents, color-coded:
  - Green (>= 80%): Excellent tuning
  - Yellow (>= 50%): Moderate tuning
  - Orange (< 50%): Needs improvement
- **Average cents offset**: Mean deviation from the target note

Statistics are recalculated each frame via `updateStatistics()` and reset when the cents history is cleared.

---

## 6. SVG Icon Buttons

### Problem

The scroll/zoom/reset buttons used text labels (arrows, +/-, reset symbol)
that were not visually distinctive and did not scale well across languages.

### Solution

Replaced `juce::TextButton` with `juce::DrawableButton` using SVG icons
(Lucide-style, 24x24 viewBox):
- **Zoom In**: magnifying glass with "+" sign
- **Zoom Out**: magnifying glass with "-" sign
- **Scroll Up**: upward chevron arrow
- **Scroll Down**: downward chevron arrow
- **Reset**: cross/X icon

Button order was changed to: Zoom In, Zoom Out, Scroll Up, Scroll Down, Reset
(zoom operations first, then scroll, then reset).

---

## 7. Hover Cursor with Hz/Note Readout

### Problem

Users had no way to know the exact frequency or note name at a given Y position
on the plot area without visually estimating from the piano keyboard.

### Solution

Moving the mouse over the plot area now shows:
- A horizontal crosshair line at the cursor Y position
- A readout box displaying the note name (e.g. "F#4") and frequency (e.g. "370 Hz")

The readout automatically repositions above or below the cursor to avoid
going outside the visible area.

---

## 8. Y-Axis Frequency Labels

### Problem

The plot area had no frequency reference on the Y-axis, making it difficult
to estimate the Hz value of pitch curves without tracing to the piano keyboard.

### Solution

Hz values are now displayed on the right edge of the plot area at each C note
octave boundary (e.g. "131 Hz", "262 Hz", "523 Hz"), providing an immediate
frequency reference.

---

## 9. Smooth Animated Transitions

### Problem

Zoom and scroll operations (via buttons or mouse wheel) caused instantaneous
jumps in the view, which was disorienting.

### Solution

All zoom and scroll operations now use linear interpolation (lerp factor 0.25)
to smoothly animate the transition from the current view to the target view.
The `timerCallback()` (running at 30 fps) interpolates `fMin`/`fMax` toward
their target values each frame.

---

## 10. ARA2 Waveform Overlay (Placeholder)

### Purpose

Provide a visual waveform overlay in the Pitch Visualizer to display audio
waveform data when connected via ARA2. This serves as a placeholder for
future ARA2 integration where the DAW will stream waveform data to the
plugin for display behind the pitch curves.

### Public API

```cpp
void setWaveformOverlay (const float* samples, int numSamples, double sampleRate);
```

- Stores audio data in an internal `juce::AudioBuffer<float>`.
- Sets `hasWaveform = true` when valid data is provided.
- Passing `nullptr` or `numSamples <= 0` clears the waveform.

### Rendering

`paintWaveformOverlay()` is called in `paint()` before the pitch curves,
drawing a semi-transparent waveform as a background layer:

1. For each pixel column (step of 2px), map the X range to sample indices.
2. Find the min/max sample values within that range.
3. Draw a rounded rectangle from `(x, yMin)` to `(x + 2, yMax)` where
   `yMin = midY - maxVal * halfH` and `yMax = midY - minVal * halfH`.
4. Color: `0x18ffffff` (very transparent white, 9.4% alpha).

The `halfH` is 35% of the plot area height, keeping the waveform compact
and centered vertically.

### Integration Point

The processor should call `setWaveformOverlay()` from its ARA2 audio source
accessor when waveform data is available. This is currently a placeholder
and will be fully wired when ARA2 waveform streaming is implemented.

---

## UX/UI Audit - Remaining Improvements

The following improvements are tracked in the implementation roadmap:

| Improvement | Complexity | User Impact | Priority |
|-------------|-----------|-------------|----------|
| Bookmark positions (save frequency range) | Medium | Medium | Medium |
| Dark/Light theme toggle | Medium | Medium | Medium |
| Responsive layout for small screens | High | Medium | Medium |
| Additional piano key labels (D-G-B) | Low | Low | Low |
| Touch gesture support | High | Low | Low |
