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
- For public distribution, the official artifacts are signed & notarized (see "Signing &
  Notarization Status" below).

## Signing & Notarization Status

The official macOS artifacts (`.pkg`, AU, VST3, Standalone) are **signed with an Apple
Developer ID and notarized**. The drag-and-drop `.zip` is **not notarized**.

### If Gatekeeper blocks an artifact (zip contents or locally built plugins)

For the drag-and-drop `.zip` or for plugins built locally from source, macOS may attach a
quarantine attribute. Remove it after installing:

```bash
# Remove the quarantine attribute from the AU component
sudo xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component

# If installed via pkg, also clear it from the system location
sudo xattr -rd com.apple.quarantine /Library/Audio/Plug-Ins/Components/OpenVoxTuner.component
```

For the VST3:

```bash
sudo xattr -rd com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3
sudo xattr -rd com.apple.quarantine /Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3
```

### Allow in System Settings

1. Open **System Settings > Privacy & Security**.
2. If "OpenVoxTuner was blocked" is reported, click **Allow Anyway**.

### Signing & notarizing locally (reference)

To sign and notarize a build yourself (requires an Apple Developer ID):

```bash
# Sign the component
codesign --force --deep --sign "Developer ID Application: Your Name (TEAMID)" \
  ~/Library/Audio/Plug-Ins/Components/OpenVoxTuner.component

# Sign the pkg
./build_macos_pkg.sh \
  --juce-path ~/dev/JUCE \
  --sign-installer "Developer ID Installer: Your Name (TEAMID)"

# Notarize
xcrun notarytool submit OpenVoxTuner-macOS.pkg \
  --apple-id "your@email.com" \
  --team-id TEAMID \
  --password "app-specific-password" \
  --wait

# Staple the notarization ticket
xcrun stapler staple OpenVoxTuner-macOS.pkg
```
