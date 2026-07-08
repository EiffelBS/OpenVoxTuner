# OpenVoxTuner - Additional Feature Proposals

> Date: 2026-07-07
> Status: Proposals for user validation

---

## Overview

This document presents additional feature proposals for the OpenVoxTuner plugin,
extending beyond the visualizer improvements. Each proposal includes a use case,
estimated complexity, and user experience impact assessment.

---

## 1. ARA2 Waveform Overlay in Visualizer

**Description**: When running inside a DAW that supports ARA2 (Cubase, Studio One,
REAPER), overlay the DAW's waveform/clip data behind the pitch curves in the
visualizer. This provides visual context for the pitch correction relative to
the audio waveform.

**Use Case**: The singer sees exactly which part of the waveform corresponds to
each pitch deviation, making it easier to understand when and where correction
is applied.

**Complexity**: High (requires ARA2 content reader integration, waveform
rendering pipeline, and DAW-specific testing).

**UX Impact**: High - transforms the visualizer from an abstract pitch display
into a contextual audio editing tool.

**Feasibility**: Requires cooperation with the ARA2 host. Some DAWs provide
waveform data through ARA2 extensions, others don't. Fallback to pitch-only
display for unsupported hosts.

---

## 2. Per-Voice Harmony Tuning Controls

**Description**: Add individual tuning controls (cents offset, scale snap, gain)
for each harmony voice independently. Currently all voices share global tuning
parameters.

**Use Case**: The user wants the low octave harmony to be slightly flat for
warmth, while the high harmony is tightly tuned for clarity.

**Complexity**: Medium (UI expansion for per-voice parameter panels, DSP
modifications for independent tuning paths).

**UX Impact**: Medium - provides fine-grained control for advanced users while
keeping the simple interface for beginners.

---

## 3. Formant Shift Visualization

**Description**: Display the formant frequencies as an overlay on the pitch
curves. Show the formant shift applied by the formant knob as a separate
curve or as a delta indicator.

**Use Case**: The user can visually confirm that formant preservation is working
and see how the formant shift affects the vocal character.

**Complexity**: Medium (formant frequency extraction from DSP, additional
curve rendering).

**UX Impact**: Medium - helps users understand the formant parameter's effect
without relying solely on auditory feedback.

---

## 4. Preset Morphing / Crossfade

**Description**: Allow morphing between two presets by interpolating all
parameters. Add a "morph slider" that smoothly transitions between the
current settings and a target preset.

**Use Case**: The user has two vocal presets (one for verses, one for choruses)
and wants to smoothly transition between them during a song.

**Complexity**: High (parameter interpolation engine, UI for morph control,
DAW automation support).

**UX Impact**: High - unique feature that differentiates OpenVoxTuner from
competitors.

---

## 5. Undo/Redo for Curve Editor

**Description**: Implement a full undo/redo stack for the pitch curve editor.
Support multi-level undo with visual history display.

**Use Case**: The user makes a mistake while drawing pitch curves and wants to
step back through previous states.

**Complexity**: Medium (command pattern implementation, history stack management,
UI for undo/redo buttons).

**UX Impact**: High - essential for any graphical editing workflow. Currently
the only way to recover from mistakes is "Clear All".

---

## 6. A/B Comparison

**Description**: Add an A/B toggle that allows the user to save two complete
parameter states and instantly switch between them for comparison.

**Use Case**: The user wants to compare two different correction settings to
decide which sounds better for a particular vocal track.

**Complexity**: Low (parameter snapshot/restore mechanism, two-state toggle UI).

**UX Impact**: High - extremely common workflow in audio production. Simple to
implement with high value.

---

## 7. Tuning Statistics Dashboard

**Description**: Display real-time statistics about the current performance:
average cents deviation, percentage of notes in tune (within threshold),
pitch stability index, and historical trend.

**Use Case**: The singer gets immediate feedback on their pitch accuracy over
time, helping them improve their performance.

**Complexity**: Medium (statistical computation, dashboard UI component).

**UX Impact**: Medium - educational value for singers, useful for vocal training.

---

## 8. MIDI Learn for All Parameters

**Description**: Enable MIDI CC mapping for all plugin parameters. The user
assigns a MIDI controller knob/slider to any parameter via right-click
context menu.

**Use Case**: The user controls the correction amount, speed, and formant
in real-time using a MIDI keyboard or control surface during live performance.

**Complexity**: Low (JUCE provides MIDI learn infrastructure; mostly UI for
mapping interface).

**UX Impact**: High - essential for live performance use cases.

---

## 9. Keyboard Shortcuts Help Overlay

**Description**: Add a help overlay (triggered by "?" key or menu item) that
shows all available keyboard shortcuts and mouse interactions in a visual
diagram.

**Status**: IMPLEMENTED - Press "?" or use hamburger menu "Keyboard Shortcuts (?)" to display the overlay.

**Use Case**: New users can quickly discover all available interactions without
reading documentation.

**Complexity**: Low (static overlay component, triggered by key press).

**UX Impact**: Medium - improves discoverability and reduces learning curve.

---

## 10. CPU Usage Meter

**Description**: Display the real-time CPU usage of the plugin as a small
indicator in the header strip. Show both instant and average CPU load.

**Use Case**: The user monitors plugin performance to ensure it doesn't exceed
the DAW's real-time processing budget.

**Complexity**: Low (JUCE provides getCpuUsage() on AudioProcessor).

**UX Impact**: Medium - important for power users running multiple plugin
instances.

---

## Priority Ranking

| # | Feature | Complexity | UX Impact | Recommended Priority |
|---|---------|-----------|-----------|---------------------|
| 6 | A/B Comparison | Low | High | **Immediate** |
| 8 | MIDI Learn | Low | High | **Immediate** |
| 5 | Undo/Redo Curve Editor | Medium | High | **High** |
| 9 | Keyboard Shortcuts Overlay | Low | Medium | **Implemented** |
| 10 | CPU Usage Meter | Low | Medium | **High** |
| 1 | ARA2 Waveform Overlay | High | High | Medium |
| 2 | Per-Voice Harmony Tuning | Medium | Medium | Medium |
| 3 | Formant Shift Visualization | Medium | Medium | Medium |
| 7 | Tuning Statistics | Medium | Medium | Medium |
| 4 | Preset Morphing | High | High | Low (future) |
