# macOS: Build AU + `.pkg` Installer

This document covers the two scripts:
- `build_macos_au.sh`
- `build_macos_pkg.sh`

## Prerequisites

```bash
xcode-select --install
brew install cmake ninja
```

JUCE must be available locally (e.g. `~/dev/JUCE`).

## 1) Build AU (Audio Unit)

Script:

```bash
chmod +x ./build_macos_au.sh
./build_macos_au.sh --juce-path ~/dev/JUCE --install
```

Expected output:
- artifact: `build-mac-au/OpenVoxTuner_artefacts/Release/AU/OpenVoxTuner.component`
- local install (with `--install` option): `~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component`

Useful options:

```bash
./build_macos_au.sh --help
```

## 2) Generate a macOS `.pkg` installer

Script:

```bash
chmod +x ./build_macos_pkg.sh
./build_macos_pkg.sh --juce-path ~/dev/JUCE
```

By default, the script:
- builds `VST3` + `AU` + `STANDALONE`
- generates `dist/OpenVoxTuner-macOS.pkg`

### Locations installed by the `.pkg`

- `/Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3`
- `/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component`
- `/Applications/OpenVoxTuner.app` (Standalone)

### Examples

VST3 only:

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3
```

AU only:

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats AU
```

Standalone only:

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats STANDALONE
```

VST3 + Standalone:

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
```

Without rebuild (package from existing artifacts):

```bash
./build_macos_pkg.sh --juce-path ~/dev/JUCE --skip-build
```

Sign the package (Developer ID Installer):

```bash
./build_macos_pkg.sh \
  --juce-path ~/dev/JUCE \
  --sign-installer "Developer ID Installer: Your Name (TEAMID)"
```

All options:

```bash
./build_macos_pkg.sh --help
```

## Important notes

- The CMake project has been prepared to enable `AU` on macOS via the `OVT_ENABLE_AU` option.
- The generated `.pkg` installs to:
  - `/Library/Audio/Plug-Ins/VST3` (if VST3 is included)
  - `/Library/Audio/Plug-Ins/Components` (if AU is included)
- For public distribution, add notarization (`notarytool`) and `stapler` afterward.
