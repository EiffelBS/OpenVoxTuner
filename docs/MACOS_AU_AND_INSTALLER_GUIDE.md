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

## Signing & Notarization Status

!!! warning "AU and PKG are not signed"
    The current builds do **not** include a Apple Developer ID signature. macOS Gatekeeper will block the unsigned `.component` and `.pkg` by default.

    To use the unsigned plugin, you need to bypass Gatekeeper for the installed files.

### Bypass Gatekeeper (one-time per install)

After installing via the `.pkg` or manually copying the `.component`:

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

### Alternative: Allow in System Settings

1. Try to open the plugin in your DAW (it will be blocked)
2. Open **System Settings > Privacy & Security**
3. Scroll down — you should see a message about "OpenVoxTuner was blocked"
4. Click **Allow Anyway**

### For developers with a Developer ID

If you have an Apple Developer ID, you can sign and notarize:

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
