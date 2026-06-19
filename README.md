# OpenVoxTuner

OpenVoxTuner is a real-time pitch correction plugin built with JUCE 8 (C++17), with harmony generation, MIDI output, and ARA support.

## Features

- Real-time pitch correction (PSOLA-based)
- Scale quantization (Auto mode)
- Graphic pitch mode (curve editor)
- Harmony engine (`Use Voice` and synth modes)
- Formant control
- MIDI note output
- VST3 / AU / Standalone targets (platform dependent)

## Repository Structure

```text
.
├─ Source/                         # Plugin + DSP source code
├─ test/                           # Unit test sources
├─ docs/                           # Project and platform documentation
├─ installer/                      # Windows installer assets/scripts
├─ CMakeLists.txt
├─ build.ps1
├─ build_installer.ps1             # Windows installer flow (Inno Setup)
├─ build_macos_vst3.sh             # macOS VST3 build helper
├─ build_macos_au.sh               # macOS AU build helper
└─ build_macos_pkg.sh              # macOS .pkg installer helper
```

## Build

### Windows (Visual Studio)

Prerequisites:
- Visual Studio 2022
- CMake >= 3.22
- JUCE 8 (default path in CMake is `C:/JUCE`)

Typical workflow:

```powershell
# from repository root
.\build.ps1 -Configuration Debug
# or
.\build.ps1 -Configuration Release
```

Windows installer:

```powershell
.\build_installer.ps1
```

### macOS (VST3 / AU / pkg)

Prerequisites:

```bash
xcode-select --install
brew install cmake ninja
```

Build VST3:

```bash
./build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

Build AU:

```bash
./build_macos_au.sh --juce-path ~/dev/JUCE --install
```

Build macOS `.pkg` installer (default: VST3 + AU + Standalone):

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE
```

More details:
- `docs/MACOS_VST3_BUILD_GUIDE.md`
- `docs/MACOS_AU_AND_INSTALLER_GUIDE.md`

## Documentation

- `BUILD_GUIDE.md`
- `DEBUG_GUIDE.md`
- `docs/MACOS_VST3_BUILD_GUIDE.md`
- `docs/MACOS_AU_AND_INSTALLER_GUIDE.md`
- `docs/GITHUB_SETUP_AND_RELEASE.md`
- `docs/releases/v0.1.1.md`

## Issue Reporting

Use GitHub issue templates:
- Bug report
- Feature request

## Releases

Recommended release process and templates are in:
- `.github/RELEASE_TEMPLATE.md`
- `docs/releases/v0.1.1.md`

## License

See `LICENSE`.
