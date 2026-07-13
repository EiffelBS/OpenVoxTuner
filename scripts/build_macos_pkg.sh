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
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE8
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE8 --formats VST3,STANDALONE
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE8 --sign-installer "Developer ID Installer: ACME (TEAMID)"
EOF
}

JUCE_PATH_ARG="${JUCE_PATH:-$HOME/dev/JUCE8}"
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

if [[ "$SKIP_BUILD" == false ]]; then
  if [[ -z "$JUCE_PATH_ARG" ]]; then
    echo "Erreur: JUCE introuvable (defaut: ~/dev/JUCE8, ou utilisez --juce-path / JUCE_PATH)." >&2
    exit 1
  fi

  if [[ ! -d "$JUCE_PATH_ARG" ]]; then
    echo "Erreur: JUCE_PATH introuvable: $JUCE_PATH_ARG" >&2
    exit 1
  fi
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

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
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

# Each format is packaged as its own component so the installer can let the user
# choose which ones to install (Standalone / VST3 / AU). Each component carries a
# unique package identifier, and (patched to a unique CFBundleIdentifier in the
# loop below) a unique bundle id, so relocation/upgrade stays unambiguous.
# Entry fields: artefactDir|installRelPath|bundleName|pkgId|choiceId|title|description
COMP_DIR="$BUILD_DIR/components"
rm -rf "$COMP_DIR"
mkdir -p "$COMP_DIR"

NL=$'\n'
OVT_COMP_ENTRIES=()
[[ "$WANT_STANDALONE" == true ]] && OVT_COMP_ENTRIES+=( "Standalone|Applications|OpenVoxTuner.app|com.eiffelbs.openvoxtuner.standalone|choice_standalone|Standalone (Application)|Application autonome OpenVoxTuner." )
[[ "$WANT_VST3" == true ]]      && OVT_COMP_ENTRIES+=( "VST3|Library/Audio/Plug-Ins/VST3|OpenVoxTuner.vst3|com.eiffelbs.openvoxtuner.vst3|choice_vst3|VST3|Plug-in VST3 pour les DAW." )
[[ "$WANT_AU" == true ]]        && OVT_COMP_ENTRIES+=( "AU|Library/Audio/Plug-Ins/Components|OpenVoxTuner.component|com.eiffelbs.openvoxtuner.au|choice_au|Audio Unit (AU)|Plug-in Audio Unit (macOS)." )

OUTLINE_XML=""
CHOICES_XML=""
PKG_REFS_XML=""

for entry in "${OVT_COMP_ENTRIES[@]}"; do
  IFS='|' read -r fmt dest_rel bundle_name pkg_id choice_id title desc <<< "$entry"

  SRC="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/$fmt/$bundle_name"
  [[ -d "$SRC" ]] || { echo "Bundle introuvable: $SRC" >&2; exit 1; }

  root="$COMP_DIR/$fmt"
  rm -rf "$root"
  mkdir -p "$root/$(dirname "$dest_rel")"
  rsync -a --delete "$SRC" "$root/$dest_rel/"

  # JUCE assigns EVERY plugin format the SAME CFBundleIdentifier
  # (com.EiffelBS.OpenVoxTuner, derived from the base target). The macOS Installer
  # builds its relocation rules from that id; identical ids make it collapse the
  # three bundles and abort with "Unable to move 'OpenVoxTuner.component' to
  # 'Applications'". Patch each format's Info.plist with a UNIQUE id so the
  # relocations stay unambiguous. (The host-facing plugin id comes from
  # PLUGIN_CODE/MANUFACTURER_CODE, not the bundle id, so this is safe.)
  PLIST="$root/$dest_rel/$bundle_name/Contents/Info.plist"
  if [[ -f "$PLIST" ]]; then
    /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $pkg_id" "$PLIST"
  else
    echo "Info.plist introuvable: $PLIST" >&2
    exit 1
  fi

  comp_pkg="$COMP_DIR/OpenVoxTuner-$fmt.pkg"
  pkgbuild --root "$root" --identifier "$pkg_id" --version "$VERSION" \
           --install-location "/" "$comp_pkg"

  kb=$(( $(stat -f%z "$comp_pkg") / 1024 ))
  PKG_REFS_XML+="    <pkg-ref id=\"$pkg_id\" version=\"$VERSION\" installKBytes=\"$kb\">#OpenVoxTuner-$fmt.pkg</pkg-ref>${NL}"
  OUTLINE_XML+="        <line choice=\"$choice_id\"/>${NL}"
  CHOICES_XML+="    <choice id=\"$choice_id\" visible=\"true\" title=\"$title\" description=\"$desc\">${NL}        <pkg-ref id=\"$pkg_id\"/>${NL}    </choice>${NL}"
done

PKG_DIR="$(dirname "$OUTPUT")"
mkdir -p "$PKG_DIR"

# Build a Distribution that presents one selectable choice per component.
DIST="$COMP_DIR/Distribution.xml"
cat > "$DIST" <<DISTEOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>OpenVoxTuner</title>
    <options customize="always" allow-external-scripts="no"/>
    <choices-outline>
${OUTLINE_XML}    </choices-outline>

${CHOICES_XML}
${PKG_REFS_XML}</installer-gui-script>
DISTEOF

PRODUCTBUILD_CMD=(productbuild
  --distribution "$DIST"
  --package-path "$COMP_DIR"
  "$OUTPUT")

if [[ -n "$SIGN_INSTALLER" ]]; then
  PRODUCTBUILD_CMD=(productbuild
    --sign "$SIGN_INSTALLER"
    --distribution "$DIST"
    --package-path "$COMP_DIR"
    "$OUTPUT")
fi

echo "[4/4] Génération du .pkg..."
"${PRODUCTBUILD_CMD[@]}"

echo "[OK] Installateur généré: $OUTPUT"
