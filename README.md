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
  <a href="readme_i18n/README_ja_JP.md">&#26085;&#26412;&#35486;</a> &mdash;
  <a href="readme_i18n/README_zh_CN.md">&#20013;&#25991;</a>
</p>

## Download

OpenVoxTuner is distributed as GitHub Releases for each version:

| Platform | Artifact | Notes |
|----------|----------|-------|
| Windows  | `OpenVoxTuner_Windows_Installer.exe` | Installer (Inno Setup) |
| macOS    | `OpenVoxTuner-macOS.zip` | Drag-and-drop: VST3 + Standalone (universal arm64/x86_64) |
| macOS    | `OpenVoxTuner-macOS.pkg` | Installer (unsigned): VST3 &rarr; `/Library/Audio/Plug-Ins/VST3`, Standalone &rarr; `/Applications` |

> The macOS `.pkg` is **unsigned**. To install it, right-click &rarr; *Open*, or run
> `sudo installer -pkg OpenVoxTuner-macOS.pkg -target /`.
> The **AU** plugin is not included in the unsigned releases (an unsigned AU cannot be
> loaded by a DAW); build it from source if you need it.

## Table of Contents

- [Download](#download)

- [Screenshots](#screenshots)
- [Features](#features)
- [Why OpenVoxTuner?](#why-openvoxtuner)
- [Repository Structure](#repository-structure)
- [Licensing](#licensing)
- [Support the Project](#support-the-project)
- [Developer License](#developer-license)
- [Contributing](#contributing)
- [Build](#build)
- [Documentation](#documentation)

---

## Screenshots

<!-- TODO: replace these with the real plugin screenshots.
     Save your PNGs in docs/screenshots/ (e.g. main-view.png, live-visualizer.png,
     curve-editor.png). Until the files exist, the images below render as broken —
     that is expected; this is just the markdown template to fill in later. -->

<p align="center">
  <img src="docs/screenshots/main-view.png" width="80%" alt="OpenVoxTuner main window — waveform with pitch-curve overlay">
</p>
<p align="center">
  <img src="docs/screenshots/live-visualizer.png" width="45%" alt="Live Visualizer">
  <img src="docs/screenshots/curve-editor.png" width="45%" alt="Curve Editor">
</p>

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

- **YIN** — time-domain autocorrelation (fast, low CPU, the only detector used)
- SWIPE' and PYIN were evaluated and removed (kept YIN only for speed and stability)

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

## Why OpenVoxTuner?

- **Open source by design** — every line of DSP, UI, and preset logic is public. No black boxes, no telemetry, no feature paywalls. You can read exactly how your voice is processed.
- **AGPLv3 for freedom** — the license guarantees the project stays free and open. Anyone can use it (even commercially), and any improvements must be shared back with the community.
- **ARA2 native** — deep DAW integration means key, scale, and tempo are read directly from your project. No manual setup, no guesswork — OpenVoxTuner follows your arrangement automatically.
- **Built for real vocals** — YIN pitch detection, formant-preserving PSOLA, and a Melodyne-style curve editor are tuned for the nuances of sung performances, not just proof-of-concept demos.

### AI-Assisted Development

OpenVoxTuner uses AI coding assistants to accelerate development, always under strict human supervision. Every line of code is reviewed, tested, and fully open for community audit. No unverified AI-generated code is merged.

## Repository Structure

```text
.
├─ Source/
│  ├─ dsp/                        # DSP modules
│  │  ├─ IPitchDetector.h         # Abstract pitch detector interface
│  │  ├─ YinPitchDetector.*       # YIN algorithm (active)
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
├─ docs/                        # Documentation
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

OpenVoxTuner is **free for everyone** under the [AGPLv3](LICENSE) license — musicians, producers, studios, educators. No restrictions on commercial use.

### Third-party licenses

| Library | License | Compatibility |
|---------|---------|--------------|
| JUCE 8 | AGPLv3 | Same license |
| ARA SDK | Apache 2.0 | Fully compatible |
| PreSonus Extensions | Public Domain | Fully compatible |
| Catch2 (tests) | Boost (BSL-1.0) | Fully compatible |

All third-party licenses are compatible with AGPLv3.

### What AGPLv3 means for you

| You are... | Can you use it for free? | Any obligation? |
|---|---|---|
| Musician / Producer | Yes | None — just make music |
| Studio (mixing, mastering, production) | Yes | None — you're using the plugin as a tool |
| Educator / Student | Yes | None |
| Developer (modifying & redistributing) | Yes | You must share your modified source under AGPLv3 |
| Company (forking into a closed-source product) | No | You need a commercial license |

> In practice, if you use OpenVoxTuner to make music — even professionally — the AGPLv3 license is completely free with no obligations.

### Support the project

OpenVoxTuner is free for everyone. If OpenVoxTuner saves you time or helps your music, consider supporting the project — even a small donation makes a huge difference.

| Tier | Platform | Price | Perks |
|------|----------|-------|-------|
| **Free** | — | 0$ | Full plugin, all features |
| **Buy me a coffee** | [Ko-fi](https://ko-fi.com/) | One-time | A quick thank-you ❤️ |
| **Sponsor** | [GitHub Sponsors](https://github.com/sponsors/) | Monthly | Support ongoing development |
| **Supporter** | [Patreon](https://patreon.com/) | 5$/month | Private Discord + vote on upcoming features |
| **Gold** | [Patreon](https://patreon.com/) | 20$/month | All Supporter perks + early access / beta builds + name in credits |

Every contribution helps keep the project alive and free for everyone.

### Developer license

A commercial license is available for developers or companies who want to integrate OpenVoxTuner into a **closed-source product** without the AGPLv3 copyleft obligation.

**What it grants:**
- Permission to use OpenVoxTuner's DSP, UI components, and algorithms in proprietary software
- No copyleft obligations — you are **not** required to publish your source code
- No requirement to release derivative works under AGPLv3

**What it includes:**
- Priority email support
- Optional custom features and DSP consulting
- Perpetual license for the version purchased (updates subject to tier)

**Contact:** open an issue on GitHub or email [license@openvoxtuner.com](mailto:license@openvoxtuner.com).

## Contributing

Contributions are welcome! You can help by:

- Fixing bugs
- Improving DSP algorithms
- Adding language translations
- Enhancing the UI
- Writing documentation
- Testing DAW compatibility

See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

### AI-Generated Code Review

Some parts of OpenVoxTuner are written with the assistance of AI coding agents, always under human supervision. All code is reviewed before merging, and community pull requests are highly encouraged to audit, improve, or replace AI-assisted sections.

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

Build macOS `.pkg` installer. The official releases ship **VST3 + Standalone** (the AU is omitted because an unsigned AU cannot be loaded by a DAW):

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

To also include the AU component in a local build, add it to `--formats` (e.g. `VST3,AU,STANDALONE`).

Detailed build guides:
- [docs/MACOS_VST3_BUILD_GUIDE.md](docs/MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](docs/MACOS_AU_AND_INSTALLER_GUIDE.md)

## Formats

| Format   | Windows | macOS |
|----------|---------|-------|
| VST3     | ✅      | ✅    |
| AU       | —       | ✅*   |
| Standalone | ✅    | ✅    |
| ARA2     | ✅      | ✅    |

> \* The AU component is buildable from source, but is **not shipped** in the unsigned
> releases — an unsigned AU cannot be loaded by a DAW on macOS. The distributed releases
> include **VST3 + Standalone** on both platforms.

## Documentation

- [docs/architecture.md](docs/architecture.md) — software architecture overview
- [docs/default-parameters.md](docs/default-parameters.md) — all plugin parameters reference
- [docs/implementation-roadmap.md](docs/implementation-roadmap.md) — feature roadmap
- [docs/ARA_Specifications.md](docs/ARA_Specifications.md) — ARA2 support technical specifications
- [docs/deployment-and-packaging-guide.md](docs/deployment-and-packaging-guide.md) — release workflow
- [docs/GITHUB_SETUP_AND_RELEASE.md](docs/GITHUB_SETUP_AND_RELEASE.md) — GitHub repository setup
- [docs/MACOS_VST3_BUILD_GUIDE.md](docs/MACOS_VST3_BUILD_GUIDE.md) — macOS VST3 build guide
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](docs/MACOS_AU_AND_INSTALLER_GUIDE.md) — macOS AU + installer guide
- [docs/preset-morphing-technical-strategy.md](docs/preset-morphing-technical-strategy.md) — A/B preset morphing strategy
- [docs/releases/v0.1.1.md](docs/releases/v0.1.1.md) — release notes

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