#!/usr/bin/env bash
set -euo pipefail

show_help() {
  cat <<'EOF'
Usage: ./build_macos_pkg.sh --juce-path <path> [options]

Construit les formats demandés (VST3/AU/Standalone), puis génère un installateur .pkg macOS.

Options:
  --juce-path <path>      Chemin vers le repo JUCE (ou variable JUCE_PATH)
  --build-dir <dir>       Dossier de build (defaut: build-mac-pkg)
  --config <cfg>          Configuration CMake (defaut: Release)
  --arch <archs>          Architectures macOS (defaut: arm64;x86_64)
  --generator <gen>       Generateur CMake (defaut: Ninja)
  --formats <list>        Formats a inclure: VST3,AU,STANDALONE (defaut: VST3,AU,STANDALONE)
  --version <ver>         Version package (defaut: 0.1.1)
  --identifier <id>       Bundle identifier package (defaut: com.eiffelbs.openvoxtuner)
  --output <path>         Fichier pkg final (defaut: dist/OpenVoxTuner-macOS.pkg)
  --sign-installer <id>   Signature pkg (Developer ID Installer)
  --skip-build            Ne fait pas la compilation, package depuis les artefacts existants
  --help                  Affiche cette aide

Exemples:
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE --formats VST3,STANDALONE
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE --sign-installer "Developer ID Installer: ACME (TEAMID)"
EOF
}

JUCE_PATH_ARG="${JUCE_PATH:-}"
BUILD_DIR="build-mac-pkg"
CONFIG="Release"
ARCHS="arm64;x86_64"
GENERATOR="Ninja"
FORMATS="VST3,AU,STANDALONE"
VERSION="0.1.1"
IDENTIFIER="com.eiffelbs.openvoxtuner"
OUTPUT="dist/OpenVoxTuner-macOS.pkg"
SIGN_INSTALLER=""
SKIP_BUILD=false

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
    --formats)
      FORMATS="${2:-}"
      shift 2
      ;;
    --version)
      VERSION="${2:-}"
      shift 2
      ;;
    --identifier)
      IDENTIFIER="${2:-}"
      shift 2
      ;;
    --output)
      OUTPUT="${2:-}"
      shift 2
      ;;
    --sign-installer)
      SIGN_INSTALLER="${2:-}"
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=true
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
  echo "Erreur: --juce-path est requis (ou variable JUCE_PATH)." >&2
  exit 1
fi

if [[ ! -d "$JUCE_PATH_ARG" ]]; then
  echo "Erreur: JUCE_PATH introuvable: $JUCE_PATH_ARG" >&2
  exit 1
fi

for cmd in cmake pkgbuild productbuild rsync; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Erreur: commande manquante: $cmd" >&2
    exit 1
  fi
done

WANT_VST3=false
WANT_AU=false
WANT_STANDALONE=false
IFS=',' read -ra ITEMS <<< "$FORMATS"
for item in "${ITEMS[@]}"; do
  fmt="$(echo "$item" | tr '[:lower:]' '[:upper:]' | xargs)"
  case "$fmt" in
    VST3) WANT_VST3=true ;;
    AU) WANT_AU=true ;;
    STANDALONE) WANT_STANDALONE=true ;;
    "") ;;
    *)
      echo "Format non supporte: $fmt (utiliser VST3,AU,STANDALONE)" >&2
      exit 1
      ;;
  esac
done

if [[ "$WANT_VST3" == false && "$WANT_AU" == false && "$WANT_STANDALONE" == false ]]; then
  echo "Erreur: aucun format valide demandé." >&2
  exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

if [[ "$SKIP_BUILD" == false ]]; then
  echo "[1/4] Configuration CMake..."
  if [[ "$WANT_AU" == true ]]; then
    ENABLE_AU=ON
  else
    ENABLE_AU=OFF
  fi

  cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DJUCE_PATH="$JUCE_PATH_ARG" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DOVT_ENABLE_AU="$ENABLE_AU"

  if [[ "$WANT_VST3" == true ]]; then
    echo "[2/4] Build target OpenVoxTuner_VST3..."
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_VST3
  fi

  if [[ "$WANT_AU" == true ]]; then
    echo "[2/4] Build target OpenVoxTuner_AU..."
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_AU
  fi

  if [[ "$WANT_STANDALONE" == true ]]; then
    echo "[2/4] Build target OpenVoxTuner_Standalone..."
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_Standalone
  fi
fi

echo "[3/4] Préparation du contenu package..."
PKGROOT="$BUILD_DIR/pkgroot"
rm -rf "$PKGROOT"
mkdir -p "$PKGROOT/Library/Audio/Plug-Ins"

if [[ "$WANT_VST3" == true ]]; then
  SRC_VST3="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/VST3/OpenVoxTuner.vst3"
  [[ -d "$SRC_VST3" ]] || { echo "VST3 introuvable: $SRC_VST3" >&2; exit 1; }
  mkdir -p "$PKGROOT/Library/Audio/Plug-Ins/VST3"
  rsync -a --delete "$SRC_VST3" "$PKGROOT/Library/Audio/Plug-Ins/VST3/"
fi

if [[ "$WANT_AU" == true ]]; then
  SRC_AU="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/AU/OpenVoxTuner.component"
  [[ -d "$SRC_AU" ]] || { echo "AU introuvable: $SRC_AU" >&2; exit 1; }
  mkdir -p "$PKGROOT/Library/Audio/Plug-Ins/Components"
  rsync -a --delete "$SRC_AU" "$PKGROOT/Library/Audio/Plug-Ins/Components/"
fi

if [[ "$WANT_STANDALONE" == true ]]; then
  SRC_APP="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/Standalone/OpenVoxTuner.app"
  [[ -d "$SRC_APP" ]] || { echo "Standalone introuvable: $SRC_APP" >&2; exit 1; }
  mkdir -p "$PKGROOT/Applications"
  rsync -a --delete "$SRC_APP" "$PKGROOT/Applications/"
fi

PKG_DIR="$(dirname "$OUTPUT")"
mkdir -p "$PKG_DIR"
COMP_PKG="$BUILD_DIR/OpenVoxTuner-component.pkg"

PKGBUILD_CMD=(pkgbuild
  --root "$PKGROOT"
  --identifier "$IDENTIFIER.plugins"
  --version "$VERSION"
  --install-location "/"
  "$COMP_PKG")

if [[ -n "$SIGN_INSTALLER" ]]; then
  PKGBUILD_CMD=(pkgbuild
    --root "$PKGROOT"
    --identifier "$IDENTIFIER.plugins"
    --version "$VERSION"
    --install-location "/"
    --sign "$SIGN_INSTALLER"
    "$COMP_PKG")
fi

"${PKGBUILD_CMD[@]}"

PRODUCTBUILD_CMD=(productbuild
  --package "$COMP_PKG"
  "$OUTPUT")

if [[ -n "$SIGN_INSTALLER" ]]; then
  PRODUCTBUILD_CMD=(productbuild
    --sign "$SIGN_INSTALLER"
    --package "$COMP_PKG"
    "$OUTPUT")
fi

echo "[4/4] Génération du .pkg..."
"${PRODUCTBUILD_CMD[@]}"

echo "[OK] Installateur généré: $OUTPUT"
