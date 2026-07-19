#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage: ./build_macos_au.sh --juce-path <path> [options]

Options:
  --juce-path <path>   Chemin vers le repo JUCE (ou variable JUCE_PATH)
  --build-dir <dir>    Dossier de build (defaut: build-mac-au)
  --config <cfg>       Configuration CMake (defaut: Release)
  --arch <archs>       Architectures macOS (defaut: arm64;x86_64)
  --generator <gen>    Generateur CMake (defaut: Ninja)
  --install            Copie le .component vers ~/Library/Audio/Plug-Ins/Components
  --help               Affiche cette aide

Exemple:
  ./build_macos_au.sh --juce-path ~/dev/JUCE8 --install
EOF
}

JUCE_PATH_ARG="${JUCE_PATH:-$HOME/dev/JUCE8}"
BUILD_DIR="build-mac-au"
CONFIG="Release"
ARCHS="arm64;x86_64"
GENERATOR="Ninja"
INSTALL=false

while [[ $# -gt 0 ]]; do
  case "$1" in
    --juce-path)
      JUCE_PATH_ARG="${2:-}"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="${2:-}"
      shift 2
      ;;
    --config)
      CONFIG="${2:-}"
      shift 2
      ;;
    --arch)
      ARCHS="${2:-}"
      shift 2
      ;;
    --generator)
      GENERATOR="${2:-}"
      shift 2
      ;;
    --install)
      INSTALL=true
      shift
      ;;
    --help|-h)
      show_help
      exit 0
      ;;
    *)
      echo "Option inconnue: $1" >&2
      show_help
      exit 1
      ;;
  esac
done

if [[ -z "$JUCE_PATH_ARG" ]]; then
  echo "Erreur: JUCE introuvable (defaut: ~/dev/JUCE8, ou utilisez --juce-path / JUCE_PATH)." >&2
  exit 1
fi

if [[ ! -d "$JUCE_PATH_ARG" ]]; then
  echo "Erreur: JUCE_PATH introuvable: $JUCE_PATH_ARG" >&2
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "Erreur: cmake introuvable. Installer CMake." >&2
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "[1/3] Configuration CMake (AU activé)..."
cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DJUCE_PATH="$JUCE_PATH_ARG" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
  -DOVT_ENABLE_AU=ON

echo "[2/3] Build target OpenVoxTuner_AU..."
cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_AU

echo "[2/3] Build target OpenVoxKey_AU (companion)..."
cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxKey_AU

PLUGIN_PATH="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/AU/OpenVoxTuner.component"
if [[ ! -d "$PLUGIN_PATH" ]]; then
  echo "Erreur: composant AU introuvable: $PLUGIN_PATH" >&2
  exit 1
fi

COMPANION_PATH="$BUILD_DIR/OpenVoxKey_artefacts/$CONFIG/AU/OpenVoxKey.component"
if [[ ! -d "$COMPANION_PATH" ]]; then
  echo "Erreur: composant AU compagnon introuvable: $COMPANION_PATH" >&2
  exit 1
fi

echo "[OK] AU généré: $PLUGIN_PATH"
echo "[OK] AU compagnon généré: $COMPANION_PATH"

if [[ "$INSTALL" == true ]]; then
  DEST_DIR="$HOME/Library/Audio/Plug-Ins/Components"
  mkdir -p "$DEST_DIR"
  echo "[3/3] Installation locale..."
  rsync -a --delete "$PLUGIN_PATH" "$DEST_DIR/"
  rsync -a --delete "$COMPANION_PATH" "$DEST_DIR/"
  echo "[OK] Installé dans: $DEST_DIR/OpenVoxTuner.component"
  echo "[OK] Installé dans: $DEST_DIR/OpenVoxKey.component"
fi
