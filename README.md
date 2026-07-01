# OpenVoxTuner

**OpenVoxTuner** is a real-time pitch correction and harmony generation VST3/AU audio plugin built with JUCE 8 (C++17). It features a graphic pitch curve editor (Melodyne-style), ARA2 integration, a modular pitch detection pipeline (YIN / SWIPE' / PYIN), and PSOLA-based pitch shifting with formant preservation.

## Features

### Pitch Correction

- **Auto mode** — scale quantization with key and scale selection (Major, Minor, Pentatonic, Chromatic, Custom)
- **Graphic mode** — draw your own pitch curve (Melodyne-style editor with snap, grid, copy/paste, undo/redo)
- **Speed control** — retarget envelope for natural or robotic correction response
- **Amount control** — blend between corrected and dry signal
- **Formant shift** — independent formant preservation/transposition (-12 to +12 semitones)

### Harmony Engine

- **Use Voice mode** — pitch-shifts your live vocal into harmony notes (1-4 shifted voices with stereo panning)
- **Synth mode** — synthesized harmony tones (Choir, Bright, Synth Lead, Strings, Guitar, Vocoder-like) with adjustable tone color
- **Harmony type presets** — 3rd/5th below/above, Vocal Stack, Power Chord, Parallel 3rd, Drone
- **Volume & Blend** knobs for independent harmony level and wet/dry mix
- Harmony traces overlay in the curve editor

### Pitch Detection

- **Modular pipeline** — abstract `IPitchDetector` interface with runtime switching
- **YIN** — time-domain autocorrelation (original, fast, low CPU)
- **SWIPE'** — spectral algorithm (FFT-based, robust on noisy vocals) *(source-ready)*
- **PYIN** — probabilistic YIN with HMM Viterbi smoothing *(source-ready)*

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
- Auto-scroll in ARA mode

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
│  │  ├─ PyinPitchDetector.*      # PYIN probabilistic algorithm
│  │  ├─ ScaleQuantizer.*         # Scale quantization engine
│  │  ├─ PitchShifter.*           # PSOLA pitch shifter
│  │  ├─ RetargetEnvelope.*       # Speed envelope smoother
│  │  ├─ FormantPreserver.*       # Formant compensation filter
│  │  ├─ PitchCurve.*             # Curve data model
│  │  ├─ HarmonyEngine.*          # Harmony synthesis engine
│  │  ├─ PitchDetector.*          # Original YIN reference (not compiled)
│  │  └─ NoteUtils.h / IPitchShifter.h
│  ├─ ui/                         # UI components
│  │  ├─ PitchCurveEditor.*       # Curve editor component
│  │  ├─ PitchVisualizer.*        # Pitch visualisation
│  │  ├─ PianoKeyboard.*          # Piano keyboard widget
│  │  ├─ ScaleKeyboardComponent.* # Scale keyboard display
│  │  └─ LookAndFeel.*            # Custom look and feel
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