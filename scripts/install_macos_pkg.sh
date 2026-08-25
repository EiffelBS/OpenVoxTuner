#!/usr/bin/env bash
# install_macos_pkg.sh
# Generates a macOS .pkg installer for OpenVoxTuner.
#
# Prerequisites:
#   - Have run build_macos.sh beforehand (or --skip-build to
#     repackage from an existing build)
#   - pkgbuild (included with Xcode)
#
# Usage:
#   ./install_macos_pkg.sh                         # build + package
#   ./install_macos_pkg.sh --skip-build            # package only
#   ./install_macos_pkg.sh --output ~/Desktop/OpenVoxTuner.pkg

set -euo pipefail

# === Defaults ===
JUCE_PATH="${JUCE_PATH:-$HOME/dev/JUCE8}"
BUILD_DIR="build-mac"
CONFIG="Release"
ARCHS="arm64;x86_64"
GENERATOR="Ninja"
SKIP_BUILD=false
VERSION="0.1.1"
IDENTIFIER="com.eiffelbs.openvoxtuner"
OUTPUT="dist/OpenVoxTuner-macOS-${VERSION}.pkg"
SIGN_INSTALLER=""

# === Parse arguments ===
while [[ $# -gt 0 ]]; do
  case "$1" in
    --juce-path)      JUCE_PATH="${2:-}"; shift 2 ;;
    --build-dir)      BUILD_DIR="${2:-}"; shift 2 ;;
    --config)         CONFIG="${2:-}"; shift 2 ;;
    --arch)           ARCHS="${2:-}"; shift 2 ;;
    --generator)      GENERATOR="${2:-}"; shift 2 ;;
    --version)        VERSION="${2:-}"; shift 2 ;;
    --identifier)     IDENTIFIER="${2:-}"; shift 2 ;;
    --output)         OUTPUT="${2:-}"; shift 2 ;;
    --sign-installer) SIGN_INSTALLER="${2:-}"; shift 2 ;;
    --skip-build)     SKIP_BUILD=true; shift ;;
    --help|-h)
      echo "Usage: $0 [--juce-path <path>] [--output <file.pkg>] [--skip-build] [--sign-installer <id>]"
      exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "=== OpenVoxTuner macOS PKG Installer ==="
echo "  VERSION    = $VERSION"
echo "  IDENTIFIER = $IDENTIFIER"
echo "  OUTPUT     = $OUTPUT"
echo "  SKIP_BUILD = $SKIP_BUILD"
echo ""

# === Step 1: Build (unless --skip-build) ===
if [[ "$SKIP_BUILD" != true ]]; then
  if [[ ! -d "$JUCE_PATH" ]]; then
    echo "Error: JUCE not found in $JUCE_PATH" >&2
    echo "Pass --skip-build if the artifacts already exist." >&2
    exit 1
  fi

  echo "[1/4] Building..."
  cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DJUCE_PATH="$JUCE_PATH" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS"
  cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_VST3
  cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_AU
  cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_Standalone
  echo "[OK] Build completed."
else
  echo "[1/4] Build skipped (--skip-build)."
fi

ARTEFACTS="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG"

# === Artifact verification ===
echo "[2/4] Verifying artifacts..."
VST3_BUNDLE="$ARTEFACTS/VST3/OpenVoxTuner.vst3"
AU_BUNDLE="$ARTEFACTS/AU/OpenVoxTuner.component"
STANDALONE_APP="$ARTEFACTS/Standalone/OpenVoxTuner.app"

if [[ ! -d "$VST3_BUNDLE" ]]; then
  echo "Error: VST3 not found: $VST3_BUNDLE" >&2; exit 1
fi
if [[ ! -d "$AU_BUNDLE" ]]; then
  echo "Error: AU not found: $AU_BUNDLE" >&2; exit 1
fi
if [[ ! -d "$STANDALONE_APP" ]]; then
  echo "Error: Standalone not found: $STANDALONE_APP" >&2; exit 1
fi
echo "  VST3       : $VST3_BUNDLE"
echo "  AU         : $AU_BUNDLE"
echo "  Standalone : $STANDALONE_APP"
echo "[OK] All artifacts present."

# === Step 3: Creating the staging directory ===
echo "[3/4] Creating pkg staging..."
STAGING_DIR=$(mktemp -d)
trap 'rm -rf "$STAGING_DIR"' EXIT

PACKAGE_ROOT="$STAGING_DIR/root"
mkdir -p "$PACKAGE_ROOT"

# Copying the plugins into the standard macOS directories
DEST_VST3="$PACKAGE_ROOT/Library/Audio/Plug-Ins/VST3"
DEST_AU="$PACKAGE_ROOT/Library/Audio/Plug-Ins/Components"
DEST_APP="$PACKAGE_ROOT/Applications"

mkdir -p "$DEST_VST3" "$DEST_AU" "$DEST_APP"
cp -R "$VST3_BUNDLE" "$DEST_VST3/"
cp -R "$AU_BUNDLE" "$DEST_AU/"
cp -R "$STANDALONE_APP" "$DEST_APP/"

echo "  Staging: $PACKAGE_ROOT"
echo "    - Library/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3"
echo "    - Library/Audio/Plug-Ins/Components/OpenVoxTuner.component"
echo "    - Applications/OpenVoxTuner.app"

# === Step 4: Generating the .pkg ===
echo "[4/4] Generating package..."
mkdir -p "$(dirname "$OUTPUT")"

PKG_BUILD_ARGS=(
  --root "$PACKAGE_ROOT"
  --identifier "$IDENTIFIER"
  --version "$VERSION"
  --install-location "/"
)

if [[ -n "$SIGN_INSTALLER" ]]; then
  PKG_BUILD_ARGS+=(--sign "$SIGN_INSTALLER")
  echo "  Signing with: $SIGN_INSTALLER"
fi

pkgbuild "${PKG_BUILD_ARGS[@]}" "$OUTPUT"

echo "[OK] Package generated: $OUTPUT"
echo ""
echo "To install: sudo installer -pkg $OUTPUT -target /"
echo "Or double-click the .pkg in Finder."