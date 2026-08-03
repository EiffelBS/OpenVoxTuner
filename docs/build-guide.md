# Build Guide

This guide explains how to compile OpenVoxTuner from source on **Windows** and
**macOS**. OpenVoxTuner uses the native JUCE 8 CMake support (no Projucer required),
with **C++17** as the standard.

## Prerequisites

### Common

- [JUCE 8](https://juce.com/) (Starter Free / AGPLv3) — default path on Windows is `C:/JUCE`
- CMake **>= 3.22**
- A C++17 compiler (MSVC on Windows, Clang on macOS)
- Git LFS (for binary resources)

### Windows

- Visual Studio 2022
- CMake >= 3.22
- JUCE 8 (default path in CMake is `C:/JUCE`)
- Git LFS

### macOS

```bash
xcode-select --install
brew install cmake ninja
```

Clone JUCE locally (example):

```bash
git clone https://github.com/juce-framework/JUCE.git ~/dev/JUCE
```

## Plugin formats

| Format | Windows | macOS |
|--------|---------|-------|
| VST3 | ✅ | ✅ |
| AU | — | ✅ |
| Standalone | ✅ | ✅ |
| ARA2 | ✅ | ✅ |

!!! note "About CLAP and AAX"
    **CLAP** is not available in JUCE 8.x (verified up to 8.0.14) and is therefore not
    built. **AAX** (Pro Tools) is not built in the default configuration — it requires
    an Avid Developer account and the PACE/iLok SDK. See the
    [Deployment and Packaging Guide](deployment-and-packaging-guide.md) for details.

## Building on Windows

### Using the build script

```powershell
# Debug build
.\scripts\build.ps1 -Configuration Debug

# Release build
.\scripts\build.ps1 -Configuration Release
```

### Windows installer (Inno Setup)

Requires [Inno Setup](https://jrsoftware.org/isinfo.php) installed:

```powershell
.\scripts\build_installer.ps1
```

This produces `OpenVoxTuner_Windows_Installer.exe`, which copies the VST3 to
`C:\Program Files\Common Files\VST3\` and the Standalone to `C:\Program Files\OpenVoxTuner\`.

### Manual CMake (alternative)

```bash
cmake -B build -G "Visual Studio 17 2023"
cmake --build build --config Release
```

## Building on macOS

Three scripts are provided (each supports `--help`):

| Script | Purpose |
|--------|---------|
| `scripts/build_macos_vst3.sh` | Build the VST3 (and optionally install it locally) |
| `scripts/build_macos_au.sh` | Build the AU (and optionally install it locally) |
| `scripts/build_macos_pkg.sh` | Generate a macOS `.pkg` installer |

### VST3

```bash
chmod +x ./scripts/build_macos_vst3.sh
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

Expected artifact: `build-mac/OpenVoxTuner_artefacts/Release/VST3/OpenVoxTuner.vst3`
Installed to `~/Library/Audio/Plug-Ins/VST3` when `--install` is used.

Useful options:

```bash
# Universal (Apple Silicon + Intel), without installation
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --arch "arm64;x86_64"

# Build arm64 only
./scripts/build_macos_vst3.sh --juce-path ~/dev/JUCE --arch arm64
```

### AU

```bash
chmod +x ./scripts/build_macos_au.sh
./scripts/build_macos_au.sh --juce-path ~/dev/JUCE --install
```

Expected artifact: `build-mac-au/OpenVoxTuner_artefacts/Release/AU/OpenVoxTuner.component`
Installed to `~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component`.

### `.pkg` installer

The official releases ship a **signed & notarized** `.pkg` with **VST3 + AU + Standalone**.
By default the script builds **VST3 + AU + STANDALONE**:

```bash
chmod +x ./scripts/build_macos_pkg.sh
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE
```

To build a smaller installer without the AU, override `--formats`:

```bash
./scripts/build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

The generated `.pkg` installs:

- `/Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3`
- `/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component` (if AU is included)
- `/Applications/OpenVoxTuner.app` (Standalone)

Signing the package (Developer ID Installer):

```bash
./scripts/build_macos_pkg.sh \
  --juce-path ~/dev/JUCE \
  --sign-installer "Developer ID Installer: Your Name (TEAMID)"
```

### Manual CMake on macOS (alternative)

```bash
cmake -S . -B build-mac -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DJUCE_PATH=~/dev/JUCE \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cmake --build build-mac --config Release --target OpenVoxTuner_VST3
```

## CMake configuration notes

- **Standard C++17** is enforced (`CMAKE_CXX_STANDARD 17`).
- **JUCE path** defaults to `C:/JUCE` on Windows; pass `-DJUCE_PATH=...` if JUCE lives elsewhere.
- **ARA2** is enabled by default (`OVT_ENABLE_ARA=ON`) and can be disabled for hosts
  that do not support it (e.g. Ableton Live, FL Studio).
- On **macOS**, the AU format is enabled via the `OVT_ENABLE_AU` option (default `ON`).
  CLAP is disabled (`OVT_ENABLE_CLAP=OFF`).
- On **macOS**, the deployment target is forced to `11.0` for compatibility with
  JUCE 8, and unguarded-availability warnings are disabled (needed for "insider"
  SDKs such as Xcode 17+ / macOS 26.x).

### Installing to system plugin folders (macOS)

After a build, you can deploy all plugin formats system-wide:

```bash
sudo cmake --install build-mac
```

## Dedicated macOS guides

For step-by-step details specific to macOS, refer to:

- [docs/MACOS_VST3_BUILD_GUIDE.md](MACOS_VST3_BUILD_GUIDE.md)
- [docs/MACOS_AU_AND_INSTALLER_GUIDE.md](MACOS_AU_AND_INSTALLER_GUIDE.md)

## Related pages

- [Installation](installation.md) — install the plugin once it is built.
- [Testing](testing.md) — build and run the unit test suite.
- [Deployment and Packaging Guide](deployment-and-packaging-guide.md) — signing,
  notarization, and release workflow.
