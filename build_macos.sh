#!/usr/bin/env bash
# build_macos.sh
# Script de compilation macOS pour OpenVoxTuner.
# Utilise ~/dev/JUCE par defaut et integre les flags de compatibilite
# pour SDK macOS 26.x "insider" (deployment target 11.0,
# -Wno-unguarded-availability).
#
# Usage:
#   ./build_macos.sh                         # build Release (VST3 + AU + Standalone)
#   ./build_macos.sh --config Debug          # build Debug
#   ./build_macos.sh --formats VST3          # VST3 uniquement
#   ./build_macos.sh --install               # build + copie dans ~/Library/Audio/Plug-Ins
#   ./build_macos.sh --juce-path /autre/chemin

set -euo pipefail

# === Defaults ===
JUCE_PATH="${JUCE_PATH:-$HOME/dev/JUCE}"
BUILD_DIR="build-mac"
CONFIG="Release"
ARCHS="arm64;x86_64"
GENERATOR="Ninja"
FORMATS="VST3;AU;Standalone"
INSTALL=false

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
    --help|-h)
      echo "Usage: $0 [--juce-path <path>] [--config Release|Debug] [--formats VST3;AU;Standalone] [--install]"
      exit 0 ;;
    *) echo "Option inconnue: $1"; exit 1 ;;
  esac
done

# === Verifications ===
if [[ ! -d "$JUCE_PATH" ]]; then
  echo "Erreur: JUCE introuvable dans $JUCE_PATH" >&2
  echo "Indiquez le chemin avec --juce-path ou la variable JUCE_PATH" >&2
  exit 1
fi

if ! command -v cmake &>/dev/null; then
  echo "Erreur: cmake introuvable. Installer CMake." >&2
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

echo "=== OpenVoxTuner Build macOS ==="
echo "  JUCE_PATH    = $JUCE_PATH"
echo "  BUILD_DIR    = $BUILD_DIR"
echo "  CONFIG       = $CONFIG"
echo "  ARCHS        = $ARCHS"
echo "  FORMATS      = $FORMATS"
echo "  INSTALL      = $INSTALL"
echo ""

# === Etape 1 : Configuration CMake ===
echo "[1/3] Configuration CMake..."
cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DJUCE_PATH="$JUCE_PATH" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHS"

# === Etape 2 : Compilation ===
echo "[2/3] Compilation..."
IFS=';' read -ra TARGET_LIST <<< "$FORMATS"
for fmt in "${TARGET_LIST[@]}"; do
  TARGET_NAME="OpenVoxTuner_${fmt}"
  echo "  -> Building $TARGET_NAME ..."
  cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$TARGET_NAME"
done

echo "[OK] Compilation terminee."

# === Etape 3 : Installation (optionnelle) ===
if [[ "$INSTALL" == true ]]; then
  echo "[3/3] Installation locale..."

  IFS=';' read -ra FMT_LIST <<< "$FORMATS"
  for fmt in "${FMT_LIST[@]}"; do
    case "$fmt" in
      VST3)
        SRC="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/VST3/OpenVoxTuner.vst3"
        DST="$HOME/Library/Audio/Plug-Ins/VST3/"
        mkdir -p "$DST"
        rsync -a --delete "$SRC" "$DST"
        echo "  VST3 -> $DST"
        ;;
      AU)
        SRC="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/AU/OpenVoxTuner.component"
        DST="$HOME/Library/Audio/Plug-Ins/Components/"
        mkdir -p "$DST"
        rsync -a --delete "$SRC" "$DST"
        echo "  AU   -> $DST"
        ;;
      Standalone)
        SRC="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/Standalone/OpenVoxTuner.app"
        DST="/Applications/"
        echo "  Standalone -> $DST (sudo requis)"
        sudo rsync -a --delete "$SRC" "$DST"
        echo "  Standalone installe dans /Applications/"
        ;;
    esac
  done

  echo "[OK] Installation terminee."
fi

echo ""
echo "=== Build termine avec succes ==="
echo "Build dir : $REPO_ROOT/$BUILD_DIR"
echo "Artefacts : $REPO_ROOT/$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/"