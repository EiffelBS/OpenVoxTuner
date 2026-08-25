#!/usr/bin/env bash
# build_macos.sh
# macOS build script for OpenVoxTuner.
# Uses ~/dev/JUCE8 by default and integrates the compatibility flags
# for macOS SDK 26.x "insider" (deployment target 11.0,
# -Wno-unguarded-availability).
#
# Usage:
#   ./build_macos.sh                         # build Release (VST3 + AU + Standalone)
#   ./build_macos.sh --config Debug          # build Debug
#   ./build_macos.sh --formats VST3          # VST3 only
#   ./build_macos.sh --install               # build + copy into ~/Library/Audio/Plug-Ins
#   ./build_macos.sh --juce-path /some/other/path

set -euo pipefail

# === Defaults ===
JUCE_PATH="${JUCE_PATH:-$HOME/dev/JUCE8}"
BUILD_DIR="build-mac"
CONFIG="Release"
ARCHS="arm64;x86_64"
GENERATOR="Ninja"
FORMATS="VST3;AU;Standalone"
INSTALL=false
ENABLE_ARA=ON

# === Parse arguments ===
while [[ $# -gt 0 ]]; do
  case "$1" in
    --juce-path)   JUCE_PATH="${2:-}"; shift 2 ;;
    --build-dir)   BUILD_DIR="${2:-}"; shift 2 ;;
    --config)      CONFIG="${2:-}"; shift 2 ;;
    --arch)        ARCHS="${2:-}"; shift 2 ;;
    --generator)   GENERATOR="${2:-}"; shift 2 ;;
    --formats)     FORMATS="${2:-}"; shift 2 ;;
    --install)     INSTALL=true; shift ;;
    --no-ara)      ENABLE_ARA=OFF; shift ;;
    --help|-h)
      echo "Usage: $0 [--juce-path <path>] [--config Release|Debug] [--formats VST3;AU;Standalone] [--install] [--no-ara]"
      exit 0 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

# === Checks ===
if [[ ! -d "$JUCE_PATH" ]]; then
  echo "Error: JUCE not found in $JUCE_PATH" >&2
  echo "Specify the path with --juce-path or the JUCE_PATH variable" >&2
  exit 1
fi

if ! command -v cmake &>/dev/null; then
  echo "Error: cmake not found. Install CMake." >&2
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "=== OpenVoxTuner Build macOS ==="
echo "  JUCE_PATH    = $JUCE_PATH"
echo "  BUILD_DIR    = $BUILD_DIR"
echo "  CONFIG       = $CONFIG"
echo "  ARCHS        = $ARCHS"
echo "  FORMATS      = $FORMATS"
echo "  INSTALL      = $INSTALL"
echo "  ARA          = $ENABLE_ARA"
echo ""

# === Step 1: CMake configuration ===
echo "[1/3] Configuring CMake..."
cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DJUCE_PATH="$JUCE_PATH" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
  -DOVT_ENABLE_ARA="$ENABLE_ARA"

# === Step 2: Build ===
echo "[2/3] Building..."
IFS=';' read -ra TARGET_LIST <<< "$FORMATS"
for fmt in "${TARGET_LIST[@]}"; do
  # JUCE target names use the native format case (Standalone, not STANDALONE).
  case "$fmt" in
    STANDALONE) jt="Standalone" ;;
    *)          jt="$fmt" ;;
  esac
  TARGET_NAME="OpenVoxTuner_${jt}"
  echo "  -> Building $TARGET_NAME ..."
  cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$TARGET_NAME"
  # Companion key-detection plug-in (OpenVoxKey) is built for the same formats,
  # except Standalone (it needs a host track + an OpenVoxTuner to consume the key).
  if [[ "$fmt" != "Standalone" ]]; then
    COMPANION_NAME="OpenVoxKey_${jt}"
    echo "  -> Building $COMPANION_NAME ..."
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$COMPANION_NAME"
  fi
done

echo "[OK] Build completed."

# === Step 3: Installation (optional) ===
if [[ "$INSTALL" == true ]]; then
  echo "[3/3] Local installation..."

  IFS=';' read -ra FMT_LIST <<< "$FORMATS"
  for fmt in "${FMT_LIST[@]}"; do
    case "$fmt" in
      VST3)
        SRC="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/VST3/OpenVoxTuner.vst3"
        DST="/Library/Audio/Plug-Ins/VST3/"
        echo "  VST3 -> $DST (sudo required)"
        sudo mkdir -p "$DST"
        sudo rsync -a --delete "$SRC" "$DST"
        ;;
      AU)
        SRC="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/AU/OpenVoxTuner.component"
        DST="/Library/Audio/Plug-Ins/Components/"
        echo "  AU   -> $DST (sudo required)"
        sudo mkdir -p "$DST"
        sudo rsync -a --delete "$SRC" "$DST"
        ;;
      Standalone)
        SRC="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/Standalone/OpenVoxTuner.app"
        DST="/Applications/"
        echo "  Standalone -> $DST (sudo required)"
        sudo rsync -a --delete "$SRC" "$DST"
        echo "  Standalone installed in /Applications/"
        ;;
    esac
  done

  echo "[OK] Installation completed."
fi

echo ""
echo "=== Build completed successfully ==="
echo "Build dir: $REPO_ROOT/$BUILD_DIR"
echo "Artifacts: $REPO_ROOT/$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/"