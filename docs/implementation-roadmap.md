# OpenVoxTuner - Implementation Roadmap

> Last updated: 2026-07-08 08:15 CEST

## Legend

- [x] Implemented
- [ ] Not yet implemented
- [~] In progress

---

## 1. Core DSP Engine

- [x] Pitch detection (YIN algorithm)
- [x] Pitch detection (SWIPE' algorithm)
- [x] Pitch shifting (Phase vocoder)
- [x] Formant preservation
- [x] Scale quantizer (14 scale types including Custom)
- [x] Harmony engine (20 harmony types)
- [x] Reverb effect (post-processing)
- [x] FlexTune / Humanize parameters
- [x] Correction mode (Modern / Transparent)
- [x] Retarget envelope (attack/release smoothing)
- [x] Pitch curve (programmatic control over time)

## 2. Audio I/O and Plugin Format

- [x] VST3 plugin format
- [x] Audio Unit (AU) plugin format (macOS)
- [x] Standalone application mode
- [x] ARA integration (DAW timeline sync)
- [x] MIDI output (quantized note events)
- [x] Bypass mode (audio pass-through)

## 3. UI / GUI - Main Editor

- [x] Dark theme with blue accent (Autotune-style)
- [x] Light theme support (toggle in hamburger menu)
- [x] Title bar with logo and version
- [x] Hamburger menu (gear icon) for options
- [x] Tabbed view: "Live" (visualizer) and "Curve Editor"
- [x] Preset system (Factory + Custom save/load/delete)
- [x] Key and Scale selection (ComboBox)
- [x] Custom scale editor (12-button keyboard)
- [x] Speed, Amount, Formant, Reverb knobs
- [x] FlexTune and Humanize knobs
- [x] Correction mode toggle (Modern / Transparent)
- [x] Bypass toggle (standalone only)
- [x] MIDI Out toggle
- [x] Harmony controls (enable, type, gain, blend, tone)
- [x] Use Voice / shifted voices selector
- [x] Harmony tone color knob
- [x] Latency mode selection (Low Latency / Quality / Safe)
- [x] Update checker (GitHub releases)
- [x] Internationalization (i18n) - English, French, German, Spanish, Japanese (menu items, labels, all tooltips)
- [x] Language selector in hamburger menu with persistence
- [x] Centralized font system (OVTFonts.h) - consistent typeface across all components
- [x] Centralized theme system (OVTTheme.h) - dark/light theme accessors
- [x] MIDI Learn for sliders (hamburger menu submenu with CC assignment)
- [x] CPU Usage Meter (header strip display with color-coded bar, positioned left of A/B button)
- [x] A/B Comparison (toggle between two plugin state slots)
- [x] Keyboard shortcuts help overlay (? key or hamburger menu)

## 4. UI / GUI - Pitch Visualizer (Live Tab)

- [x] Real-time pitch curve display (input + output)
- [x] Harmony voice lines display
- [x] Semi-logarithmic frequency scale (Hz)
- [x] LED-grid VU meter (tuning cents indicator)
- [x] Current note display with cents offset
- [x] Target note indicator
- [x] Vertical piano keyboard (notes highlighted by scale and active pitch)
- [x] Mouse wheel scroll (vertical pan)
- [x] Ctrl/Cmd + mouse wheel zoom
- [x] Dynamic octave reference lines (C notes) with labels - visible across ALL octaves
- [x] Dynamic scale note lines within visible range
- [x] Complete legend (Input, Output, Harmony) - compact, no truncation
- [x] Keyboard shortcut hints (non-truncated, compact format)
- [x] Scroll/zoom SVG icon buttons (magnifying glass, chevrons, cross)
- [x] Reset view button (restores default frequency range)
- [x] Hover cursor showing Hz/note value at mouse position
- [x] Y-axis frequency labels (Hz values along the right edge)
- [x] Animated smooth transitions for zoom/scroll (lerp interpolation)
- [x] Image export (PNG/JPEG at 2x resolution)
- [x] Piano key note labels (D, E, F, G, A, B - height-gated)
- [x] ARA2 waveform overlay (input audio captured in processBlock, displayed as background in Live visualizer with menu toggle)
- [x] Unified waveform display types (Bars, Filled, Line, Mirror) with user-selectable modes via hamburger menu, shared rendering function between Live and Curve Editor views, persisted across sessions
- [ ] Bookmark positions (save/restore frequently used frequency ranges)
- [ ] Responsive layout for small screens

## 5. UI / GUI - Curve Editor (Graphic Tab)

- [x] Graphical pitch curve editing (click to add/move points)
- [x] Time ruler with measures/beats
- [x] Snap to scale (quantize points to scale notes)
- [x] Snap to grid (quantize points to beat grid)
- [x] Step mode (staircase interpolation)
- [x] Clear all points
- [x] Transport playhead (follows DAW time in ARA mode)
- [x] Auto-scroll toggle (ARA mode only)
- [x] Time signature display
- [x] Measures count selector
- [x] Preset curves for common use cases
- [x] Harmony trace visualization
- [x] Right-click preset menu
- [x] Reset playhead button (standalone/VST3)
- [x] Undo/Redo buttons (visual UI complement to keyboard shortcuts)
- [x] Scale note lines (horizontal reference lines for current scale notes)

## 6. Scale Keyboard Component

- [x] 12-button chromatic keyboard display
- [x] Toggle individual scale notes
- [x] Blue highlight for active scale notes
- [x] Bidirectional sync with AudioParameterInt (custom0..custom11)
- [x] Auto-switch to Custom mode on user interaction

## 7. Build and Release

- [x] CMake build system
- [x] GitHub Actions CI (Windows, macOS)
- [x] Release workflow (tagged builds)
- [x] Version bump workflow
- [x] Installer for macOS (.pkg)

## 8. Documentation

- [x] Architecture overview
- [x] Default parameters documentation
- [x] Multi-engine architecture docs
- [x] Pitch detection rollback guide
- [x] Pitch shifting feasibility study
- [x] Deployment and packaging guide
- [x] macOS AU and installer guide
- [x] macOS VST3 build guide
- [x] ARA specifications
- [x] GitHub setup and release guide
- [x] Changelog (per-day format)
- [x] Implementation roadmap (this file)
- [x] Pitch Visualizer improvements documentation

## 9. Future Improvements (Backlog)

### Medium Priority
- [ ] Bookmark positions (save/restore frequency range presets)
- [x] Dark/Light theme toggle
- [ ] Responsive layout adaptation for small screens
- [ ] Note name labels on additional piano keys (D, E, F, G, A, B)

### Low Priority
- [ ] Touch gesture support (pinch-to-zoom, drag-to-scroll)
- [x] Export visualizer as image/screenshot
- [x] Multi-language UI support
- [ ] Accessibility improvements (keyboard navigation, screen reader)
