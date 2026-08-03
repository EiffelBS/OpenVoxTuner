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
  <a href="#build">Build</a> &bull;
  <a href="https://openvoxtuner.eiffelbs.ovh" target="_blank">Website</a> &bull;
  <a href="https://ovtdocs.eiffelbs.ovh" target="_blank">Docs</a>
</p>

<p align="center">
  <a href="readme_i18n/README_fr_FR.md">Fran&ccedil;ais</a> &mdash;
  <a href="readme_i18n/README_de_DE.md">Deutsch</a> &mdash;
  <a href="readme_i18n/README_es_ES.md">Espa&ntilde;ol</a> &mdash;
  <a href="readme_i18n/README_ja_JP.md">&#26085;&#26412;&#35486;</a> &mdash;
  <a href="readme_i18n/README_zh_CN.md">&#20013;&#25991;</a>
</p>

## Screenshots

<p align="center">
  <img src="assets/screenshots/main_screen.png" width="80%" alt="OpenVoxTuner main window">
</p>

<details>
<summary><strong>More screenshots...</strong></summary>

<p align="center">
  <img src="assets/screenshots/curve_editor.png" width="45%" alt="Curve Editor">
  <img src="assets/screenshots/curve_pianoroll.png" width="45%" alt="Piano Roll View">
</p>
<p align="center">
  <img src="assets/screenshots/harmony_types.png" width="80%" alt="Harmony types">
</p>

</details>

## Download

OpenVoxTuner is distributed as GitHub Releases for each version:

| Platform | Artifact | Notes |
|----------|----------|-------|
| Windows  | `OpenVoxTuner_Windows_Installer.exe` | Installer (Inno Setup) |
| macOS    | `OpenVoxTuner-macOS.zip` | Drag-and-drop: VST3 + Standalone (universal arm64/x86_64) — not notarized |
| macOS    | `OpenVoxTuner-macOS.pkg` | Signed & notarized installer: VST3 &rarr; `/Library/Audio/Plug-Ins/VST3`, AU &rarr; `/Library/Audio/Plug-Ins/Components`, Standalone &rarr; `/Applications` |

> The macOS `.pkg` installer is **signed & notarized** and includes the **VST3, AU and
> Standalone** formats. The drag-and-drop `.zip` is **not notarized** — if Gatekeeper
> blocks its contents, right-click &rarr; *Open*, or use the `.pkg` installer instead.

## Table of Contents

[Download](#download) &bull;
[Screenshots](#screenshots) &bull;
[Features](#features) &bull;
[Why OpenVoxTuner?](#why-openvoxtuner) &bull;
[Repository Structure](#repository-structure) &bull;
[Licensing](#licensing) &bull;
[Support the Project](#support-the-project) &bull;
[Developer License](#developer-license) &bull;
[Contributing](#contributing) &bull;
[Build](#build) &bull;
[Documentation](#documentation)

---

## Features

### Pitch Correction

- **Auto mode** — scale quantization with 14 scale types (Major, Minor, Pentatonic, Blues, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Chromatic, Custom...)
- **Graphic mode** — draw your own pitch curve (Melodyne-style editor with snap, grid, copy/paste, undo/redo)
- **Correction Mode** — Modern (tight) or Transparent (gentle) character
- **Speed & Amount** — retarget envelope + dry/wet blend for natural or robotic response
- **Humanize** — add subtle random variation for a more natural sound (0-50 cents)
- **Vibrato Preserve** — retain the singer's natural vibrato through correction (0-100%)
- **Voice Type** — constrains pitch detection range (Universal, Bass, Baritone, Tenor, Alto, Soprano)
- **Latency Mode** — Direct Monitoring, Low Latency, Quality, Safe

### Key Detection

- **Auto** — real-time key detection from audio input (Krumhansl-Schmuckler profiles)
- **OpenVoxKey** — companion bridge via shared memory IPC
- **Sidechain** — analyze accompaniment through dedicated sidechain bus

### Effects

- **Formant Processing** — 3 modes (Legacy, MultiFormant, Allpass) with multiple preservation strategies (LPC cross-synthesis available)
- **Reverb** — built-in reverb with adjustable mix
- **Noise Gate** — input gate with threshold control (-80 to 0 dB)
- **Upward Compressor** — lifts quiet passages before pitch detection

### Harmony Engine

- **Use Voice mode** — pitch-shifts your live vocal into 1-4 harmony voices with stereo panning
- **Synth mode** — synthesized harmony tones (Choir, Organ) with adjustable tone color
- **22 harmony types** — intervals (3rd/4th/5th/octave below/above), Vocal Stack, Power Chord, Drone, Unison...
- **Harmony controls** — Gain Match (auto RMS balancing), Follow Lead, per-voice Attack, Harmony Formant Shift (-5 to +5 st)
- Harmony traces overlay in the curve editor

### Curve Editor & Visualizer

- Graphical pitch curve editor with point drag, snap to scale/grid, copy/paste, undo/redo
- Real-time pitch visualization with input/output/harmony traces
- Piano keyboard with automatic note highlighting
- Waveform overlay (Line, Mirror, or Spectral display)
- Export as PNG (2x resolution)

### ARA2 Integration

- Full ARA2 support — seamless DAW timeline integration
- Automatic key/scale extraction from ARA musical context
- Time signature-aware measures ruler with auto-scroll
- Multi-signature support (time signature changes mid-project)

### A/B Comparison & Morphing

- **A/B slots** — save and recall two complete plugin states
- **Morph slider** — continuously interpolate between A and B (all parameters smooth)
- State persisted across sessions

### Additional

- MIDI note output (generated from detected pitch)
- MIDI target (incoming MIDI controls correction pitch)
- Curve presets with card-based gallery UI
- Global undo/redo (all automatable parameters)
- Dark/Light theme
- Stereo processing, low-latency PSOLA pitch shifting
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
├─ docs/implementation-roadmap.md
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
| **Buy me a coffee** | [Ko-fi](https://ko-fi.com/eiffelbs) | One-time | A quick thank-you ❤️ |
| **Sponsor** | [GitHub Sponsors](https://github.com/sponsors/) | Monthly | Support ongoing development |
| **Supporter** | Patreon | Coming soon | Private Discord + vote on upcoming features |
| **Gold** | Patreon | Coming soon | All Supporter perks + early access / beta builds + name in credits |

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

**Contact:** open an issue on GitHub.

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

Build macOS `.pkg` installer. The official releases ship a **signed & notarized** installer
with **VST3 + AU + Standalone**:

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,AU,STANDALONE
```

To build a smaller installer without the AU, override `--formats` (e.g. `VST3,STANDALONE`).

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

- [Website](https://openvoxtuner.eiffelbs.ovh) — landing page with features overview and download links
- [Online Docs](https://ovtdocs.eiffelbs.ovh) — full documentation (MkDocs Material)
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