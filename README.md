<p align="center">
  <a href="https://opensource.org/license/agpl-v3"><img src="https://img.shields.io/badge/License-AGPL_v3-blue.svg?color=3F51B5&style=for-the-badge&label=License&logoColor=000000&labelColor=ececec" alt="License: AGPLv3"></a>
  <img src="https://img.shields.io/badge/C++-17-blue.svg?style=for-the-badge&logo=cplusplus&logoColor=000000&labelColor=ececec" alt="C++17">
  <img src="https://img.shields.io/badge/JUCE-8-orange.svg?style=for-the-badge&labelColor=ececec" alt="JUCE 8">
  <img src="https://img.shields.io/badge/Platform-Win%20%7C%20Mac-lightgrey.svg?style=for-the-badge&labelColor=ececec" alt="Platform">
  <img src="https://img.shields.io/badge/Format-VST3%20%7C%20AU%20%7C%20Standalone-green.svg?style=for-the-badge&labelColor=ececec" alt="Formats">
</p>

<p align="center">
  <img src="assets/icon.png" width="120" alt="OpenVoxTuner Icon">
</p>

<h1 align="center">OpenVoxTuner</h1>

<h3 align="center">Real-time pitch correction & harmony generation for vocals</h3>

<p align="center">
  VST3 / AU / Standalone &mdash; built with JUCE 8 (C++17)
</p>

<p align="center">
  <a href="#features">Features</a> &bull;
  <a href="#screenshots">Screenshots</a> &bull;
  <a href="#licensing">License</a> &bull;
  <a href="#build">Build</a>
</p>

<p align="center">
  <a href="readme_i18n/README_fr_FR.md">Fran&ccedil;ais</a> &mdash;
  <a href="readme_i18n/README_de_DE.md">Deutsch</a> &mdash;
  <a href="readme_i18n/README_es_ES.md">Espa&ntilde;ol</a> &mdash;
  <a href="readme_i18n/README_ja_JP.md">&#26085;&#26412;&#35486;</a>
</p>

---

## Screenshots

<!-- Add screenshots here:
<p align="center">
  <img src="docs/screenshots/live-visualizer.png" width="45%" alt="Live Visualizer">
  <img src="docs/screenshots/curve-editor.png" width="45%" alt="Curve Editor">
</p>
-->

## Features

### Pitch Correction

- **Auto mode** — scale quantization with key and scale selection (Major, Minor, Pentatonic, Chromatic, Custom)
- **Graphic mode** — draw your own pitch curve (Melodyne-style editor with snap, grid, copy/paste, undo/redo)
- **Speed control** — retarget envelope for natural or robotic correction response
- **Amount control** — blend between corrected and dry signal
### Effects

- **Formant shift** — independent formant preservation/transposition (-12 to +12 semitones)
- **Reverb** — built-in reverb effect with adjustable mix level
- **Noise Gate** — input noise gate with threshold control (-80 to 0 dB), applied before pitch detection for cleaner results

### A/B Comparison & Morphing

- **A/B slots** — save and recall two complete plugin states (A and B)
- **Morph slider** — continuously interpolate between A and B states
- **Auto-save** — current slot is automatically saved when switching
- All parameters morph smoothly (continuous lerp for sliders, step at 50% for toggles)
- A/B state persisted across sessions

### Harmony Engine

- **Use Voice mode** — pitch-shifts your live vocal into harmony notes (1-4 shifted voices with stereo panning)
- **Synth mode** — synthesized harmony tones (Choir, Bright, Synth Lead, Strings, Guitar, Vocoder-like) with adjustable tone color
- **Harmony type presets** — 3rd/5th below/above, Vocal Stack, Power Chord, Parallel 3rd, Drone
- **Volume & Blend** knobs for independent harmony level and wet/dry mix
- Harmony traces overlay in the curve editor

### Pitch Detection

- **Modular pipeline** — abstract `IPitchDetector` interface with runtime switching
- **YIN** — time-domain autocorrelation (original, fast, low CPU)
- **SWIPE'** — spectral algorithm (FFT-based, robust on noisy vocals)
- **PYIN** — probabilistic YIN with HMM Viterbi smoothing *(removed — see changelog 2026-07-02)*

### ARA2 Integration

- Full ARA2 support (Audio Random Access) — seamless DAW timeline integration
- Automatic key/scale extraction from ARA musical context
- Time signature-aware measures ruler (3/4, 4/4, 6/8, 12/8)
- Auto-scroll following the DAW playhead during playback
- Multi-signature support (time signature changes mid-project)

### Curve Editor

- Graphical pitch curve editor with point drag, add, delete
- Snap to scale, snap to grid
- Measures selector (1, 2, 4, 8, 16, 32)
- Copy/paste and undo/redo
- Harmony trace overlay visualization
- Horizontal cursor line with note name and Hz readout
- Scale note reference lines (same as live visualizer)
- Waveform overlay (Line or Mirror display, synced with live visualizer)
- Auto-scroll toggle (works in all modes)

### Live Visualizer

- Real-time pitch visualization with input/output/harmony traces
- Piano keyboard with automatic note highlighting
- Horizontal cursor line with note name and Hz readout
- Waveform overlay (Line or Mirror display types)
- Legend block with statistics (in-tune %, average cents)
- Export as PNG image (2x resolution)

### Waveform Overlay

- Audio waveform captured from input in all modes (plugin, standalone, ARA)
- Two display types selectable from the menu:
  - **Line** — simple waveform outline (40% opacity)
  - **Mirror** — mirrored bars symmetric around center (default)
- Display type applies uniformly to both live visualizer and curve editor
- Preference persisted across sessions

### Theme System

- Dark and Light themes with unified color palette
- Automatic theme switching with full UI refresh
- Consistent popup menu colors (hamburger menu, presets, combos)
- Fixed tooltips with clean rectangular rendering

### Additional

- MIDI note output (generated from detected pitch)
- Stereo processing
- Low-latency PSOLA pitch shifting
- Bypass toggle (standalone mode)
- Standalone mode with internal 120 BPM transport

## Repository Structure

```text
.
├─ Source/
│  ├─ dsp/                        # DSP modules
│  │  ├─ IPitchDetector.h         # Abstract pitch detector interface
│  │  ├─ YinPitchDetector.*       # YIN algorithm
│  │  ├─ SwipePitchDetector.*     # SWIPE' spectral algorithm
│  │  ├─ ScaleQuantizer.*         # Scale quantization engine
│  │  ├─ PitchShifter.*           # PSOLA pitch shifter
│  │  ├─ RetargetEnvelope.*       # Speed envelope smoother
│  │  ├─ FormantPreserver.*       # Formant compensation filter
│  │  ├─ NoiseGate.h              # Input noise gate (RMS-based, hysteresis)
│  │  ├─ PitchCurve.*             # Curve data model
│  │  ├─ PresetMorpher.h          # A/B morphing engine
│  │  ├─ HarmonyEngine.*          # Harmony synthesis engine
│  │  ├─ PitchDetector.*          # Original YIN reference (not compiled)
│  │  └─ NoteUtils.h / IPitchShifter.h
│  ├─ ui/                         # UI components
│  │  ├─ PitchCurveEditor.*       # Curve editor component
│  │  ├─ PitchVisualizer.*        # Pitch visualisation
│  │  ├─ PianoKeyboard.*          # Piano keyboard widget
│  │  ├─ ScaleKeyboardComponent.* # Scale keyboard display
│  │  ├─ LookAndFeel.*            # Custom look and feel
│  │  ├─ OVTTheme.h               # Theme colours and shared waveform renderer
│  │  ├─ OVTFonts.h               # Font helpers
│  │  └─ OVTLanguages.h           # Multi-language translations
│  ├─ external/presonus/          # PreSonus plugin extensions (Studio One)
│  ├─ resources/                  # Binary resources (BuildInfo.h.in)
│  ├─ PluginProcessor.*           # Main audio processor
│  └─ PluginEditor.*              # Main editor UI
├─ scripts/                       # Build and development scripts
│  ├─ build.ps1                   # Windows build
│  ├─ build_installer.ps1         # Windows installer (Inno Setup)
│  ├─ build_macos_vst3.sh         # macOS VST3 build
│  ├─ build_macos_au.sh           # macOS AU build
│  ├─ build_macos_pkg.sh          # macOS .pkg installer
│  ├─ build_macos.sh              # macOS universal build
│  └─ ... (install, symlink, release helpers)
├─ test/                          # Unit tests (Catch2)
│  ├─ Main.cpp
│  └─ dsp/                        # Test suites per module
├─ docs/                          # Documentation and changelogs
│  ├─ changelogs/                 # Daily changelogs
│  ├─ releases/                   # Release notes (latest.json, v0.1.1.md)
│  ├─ architecture.md
│  ├─ default-parameters.md
│  └─ ...
├─ installer/                     # Windows installer assets
│  └─ OpenVoxTuner.iss            # Inno Setup script
├─ .github/                       # CI/CD and issue templates
│  ├─ workflows/                  # GitHub Actions (CI, release)
│  └─ ISSUE_TEMPLATE/             # Bug report / feature request
├─ assets/                        # Binary resources
│  └─ icon.png
├─ external/ARA_SDK/              # Celemony ARA SDK (v2.2, submodule)
├─ CMakeLists.txt
├─ README.md
├─ LICENSE
├─ roadmap.md
├─ .gitignore
├─ .gitattributes
└─ .gitmodules
```

## Licensing

OpenVoxTuner is available under a **dual license** model:

### Free — AGPLv3

- Free for anyone: personal, education, **and commercial use**
- You may modify and redistribute, but you **must share your source code** under AGPLv3
- Network use (SaaS) also requires source disclosure
- Full source code available on GitHub
- See [LICENSE](LICENSE) for the full AGPLv3 text

### Commercial — Paid License (Coming Soon)

For those who want to use OpenVoxTuner **without the AGPLv3 copyleft obligation** (closed-source productions, proprietary workflows):

| License | Price | Includes |
|---------|-------|----------|
| **User** | 99$ one-time *or* 5$/month (Patreon) | One-time: current version only. Patreon: unlimited updates |
| **Studio/Enterprise** | 20$/month (Patreon) | Unlimited updates, multi-seat, priority support |

Commercial licenses grant the right to use OpenVoxTuner in professional productions without the AGPLv3 copyleft obligation.

> **Note:** The commercial license offering is planned for a future release. For now, OpenVoxTuner is available under AGPLv3 only. If you are interested in a commercial license, please open an issue or contact the maintainer.

## Build

### Windows (Visual Studio)

Prerequisites:
- Visual Studio 2022
- CMake >= 3.22
- JUCE 8 (default path in CMake is `C:/JUCE`)
- Git LFS (for binary resources)

```powershell
# Debug build
.\scripts\build.ps1 -Configuration Debug

# Release build
.\scripts\build.ps1 -Configuration Release

# Build Windows installer (requires Inno Setup)
.\scripts\build_installer.ps1
```

### macOS (VST3 / AU / pkg)

Prerequisites:

```bash
xcode-select --install
brew install cmake ninja
```

Build VST3:

```bash
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

Build AU:

```bash
./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install
```

Build macOS `.pkg` installer (VST3 + AU + Standalone):

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE
```

Detailed build guides:
- [docs/MACOS_VST3_BUILD_GUIDE.md](docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## Formats

| Format   | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅    |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

## Documentation

- [docs/architecture.md](docs/architecture.md) — software architecture overview
- [docs/default-parameters.md](docs/default-parameters.md) — all plugin parameters reference
- [docs/deployment-and-packaging-guide.md](docs/deployment-and-packaging-guide.md) — release workflow
- [docs/GITHUB_SETUP_AND_RELEASE.md](docs/GITHUB_SETUP_AND_RELEASE.md) — GitHub repository setup
- [docs/pitch-detection-rollback-guide.md](docs/pitch-detection-rollback-guide.md) — pitch detector rollback procedures
- [docs/pitch-shifting-library-comparison.md](docs/pitch-shifting-library-comparison.md) — pitch shifting library evaluation

## License

See [LICENSE](LICENSE).

## Issue Reporting

Use GitHub issue templates:
- [Bug report](.github/ISSUE_TEMPLATE/bug_report.md)
- [Feature request](.github/ISSUE_TEMPLATE/feature_request.md)

## Star History

<a href="https://star-history.com/#EiffelBS/OpenVoxTuner&type=date">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date&theme=dark" />
    <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" />
    <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=EiffelBS/OpenVoxTuner&type=date" width="100%" />
  </picture>
</a>