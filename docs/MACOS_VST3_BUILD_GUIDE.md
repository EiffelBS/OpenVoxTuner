# Build VST3 macOS (OpenVoxTuner)

This guide corresponds to the current project (`OpenVoxTuner`) and its `CMakeLists.txt`.

## macOS Prerequisites

Install on Mac:

```bash
xcode-select --install
brew install cmake ninja
```

Clone JUCE locally (example):

```bash
git clone https://github.com/juce-framework/JUCE.git ~/dev/JUCE
```

## Quick Build with the Script

From the project root:

```bash
chmod +x ./build_macos_vst3.sh
./build_macos_vst3.sh --juce-path ~/dev/JUCE --install
```

## Build on macOS with "insider" SDK (26.x)

If you are using an "insider" version of the macOS SDK (e.g., Xcode 17+ / macOS 26.5) which is not officially supported by JUCE 8, the CMakeLists.txt already contains the necessary adaptations:

1. **Forced deployment target**: `CMAKE_OSX_DEPLOYMENT_TARGET = "11.0"` (configurable via `-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0` on the command line)
2. **Disabled warnings**: `-Wno-unguarded-availability` to ignore deprecated API warnings in the 26.x SDK.

**If errors persist**, you can force the use of a specific SDK installed on your machine (e.g., macOS 14.5 SDK):

```bash
# List available SDKs
ls /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/
# or for Xcode beta
ls /Applications/Xcode-beta.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/

# Force a specific SDK in the CMake command
cmake -B build_mac \
  -DCMAKE_OSX_SYSROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX14.5.sdk \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DJUCE_PATH=~/dev/JUCE \
  -G Ninja

cmake --build build_mac
```

> **Note**: `CMAKE_OSX_SYSROOT` points to an INSTALLED SDK folder.
> If you have only a single Xcode, the default SDK is the right one.
> Using `CMAKE_OSX_DEPLOYMENT_TARGET` alone is sufficient in most cases.

This script:
1. Configures CMake in `Release`
2. Builds the `OpenVoxTuner_VST3` target
3. Copies the bundle to `~/Library/Audio/Plug-Ins/VST3` if `--install` is provided

## Useful Script Options

```bash
./build_macos_vst3.sh --help
```

Examples:

```bash
# Build universal (Apple Silicon + Intel), without installation
./build_macos_vst3.sh --juce-path ~/dev/JUCE --arch "arm64;x86_64"

# Build arm64 only
./build_macos_vst3.sh --juce-path ~/dev/JUCE --arch arm64

# Build in a custom folder
./build_macos_vst3.sh --juce-path ~/dev/JUCE --build-dir build-mac-release
```

## Manual Build (without script)

```bash
cmake -S . -B build-mac -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DJUCE_PATH=~/dev/JUCE \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

cmake --build build-mac --config Release --target OpenVoxTuner_VST3
```

Generated bundle (expected path):

```text
build-mac/OpenVoxTuner_artefacts/Release/VST3/OpenVoxTuner.vst3
```

Manual copy to the user VST3 folder:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3
rsync -a --delete "build-mac/OpenVoxTuner_artefacts/Release/VST3/OpenVoxTuner.vst3" \
  ~/Library/Audio/Plug-Ins/VST3/
```

## Available macOS Scripts

- `build_macos_vst3.sh`: build VST3 (and optional local installation)
- `build_macos_au.sh`: build AU (and optional local installation)
- `build_macos_pkg.sh`: generate a `.pkg` installer (default: `VST3 + AU + STANDALONE`)

See also: `docs/MACOS_AU_AND_INSTALLER_GUIDE.md`.

## Distribution (optional)

For public distribution on macOS, plan for:
- `codesign`
- Apple notarization (`notarytool`)
- `stapler`

The script above only covers the developer build + local installation.
