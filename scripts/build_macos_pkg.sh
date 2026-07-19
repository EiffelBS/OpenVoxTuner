#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

show_help() {
  cat <<'EOF'
Usage: ./build_macos_pkg.sh --juce-path <path> [options]

Construit les formats demandés (VST3/AU/Standalone/CLAP), puis génère un installateur .pkg macOS.

Options:
  --juce-path <path>      Chemin vers le repo JUCE (ou variable JUCE_PATH)
  --build-dir <dir>       Dossier de build (defaut: build-mac-pkg)
  --config <cfg>          Configuration CMake (defaut: Release)
  --arch <archs>          Architectures macOS (defaut: arm64;x86_64)
  --generator <gen>       Generateur CMake (defaut: Ninja)
  --formats <list>        Formats a inclure: VST3,AU,STANDALONE,CLAP (defaut: VST3,AU,STANDALONE)
  --version <ver>         Version package (defaut: 0.1.1)
  --identifier <id>       Bundle identifier package (defaut: com.eiffelbs.openvoxtuner)
  --output <path>         Fichier pkg final (defaut: dist/OpenVoxTuner-macOS.pkg)
  --sign-identity <id>   codesign des bundles (Developer ID Application) — requis pour notariser
  --entitlements <path>  Fichier .entitlements (defaut: scripts/ovt.entitlements)
  --notarize              Notarise le pkg (xcrun notarytool) + staple
  --sign-installer <id>   Signature pkg (Developer ID Installer)
  --companion <on|off>    Inclure le plugin compagnon OpenVoxKey (memes formats que OpenVoxTuner) (defaut: on)
  --local                 Leve la quarantaine Gatekeeper du .pkg genere (test local, sans signature)
  --skip-build            Ne fait pas la compilation, package depuis les artefacts existants
  --help                  Affiche cette aide

Variables d'environnement pour --notarize:
  APPLE_API_KEY / APPLE_API_KEY_ID / APPLE_API_ISSUER   (cle API App Store Connect)
  ou APPLE_ID / APPLE_PASSWORD / APPLE_TEAM_ID          (compte Apple ID)

Exemples:
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE8
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE8 --formats VST3,STANDALONE
  ./build_macos_pkg.sh --juce-path ~/dev/JUCE8 --sign-installer "Developer ID Installer: ACME (TEAMID)" --sign-identity "Developer ID Application: ACME (TEAMID)" --notarize
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
SIGN_IDENTITY=""
ENTITLEMENTS="${SCRIPT_DIR}/ovt.entitlements"
NOTARIZE=false
SKIP_BUILD=false
WANT_COMPANION=true
LOCAL_UNSIGN=false

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
    --sign-identity)
      SIGN_IDENTITY="${2:-}"
      shift 2
      ;;
    --entitlements)
      ENTITLEMENTS="${2:-}"
      shift 2
      ;;
    --notarize)
      NOTARIZE=true
      shift
      ;;
    --sign-installer)
      SIGN_INSTALLER="${2:-}"
      shift 2
      ;;
    --companion)
      val="$(echo "${2:-on}" | tr '[:upper:]' '[:lower:]')"
      if [[ "$val" == "off" || "$val" == "false" || "$val" == "no" ]]; then
        WANT_COMPANION=false
      else
        WANT_COMPANION=true
      fi
      shift 2
      ;;
    --skip-build)
      SKIP_BUILD=true
      shift
      ;;
    --local)
      # Leve la quarantaine Gatekeeper du .pkg genere pour test local en
      # double-clic, sans certificat Developer ID. Ne remplace PAS la signature.
      LOCAL_UNSIGN=true
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

# Required commands. codesign/xcrun are only needed when signing/notarizing.
REQUIRED_CMDS=(cmake pkgbuild productbuild rsync)
if [[ -n "$SIGN_IDENTITY" || "$NOTARIZE" == true ]]; then
  REQUIRED_CMDS+=(codesign xcrun)
fi
for cmd in "${REQUIRED_CMDS[@]}"; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "Erreur: commande manquante: $cmd" >&2
    exit 1
  fi
done

WANT_VST3=false
WANT_AU=false
WANT_STANDALONE=false
WANT_CLAP=false
IFS=',' read -ra ITEMS <<< "$FORMATS"
for item in "${ITEMS[@]}"; do
  fmt="$(echo "$item" | tr '[:lower:]' '[:upper:]' | xargs)"
  case "$fmt" in
    VST3) WANT_VST3=true ;;
    AU) WANT_AU=true ;;
    STANDALONE) WANT_STANDALONE=true ;;
    CLAP) WANT_CLAP=true ;;
    "") ;;
    *)
      echo "Format non supporte: $fmt (utiliser VST3,AU,STANDALONE,CLAP)" >&2
      exit 1
      ;;
  esac
done

if [[ "$WANT_VST3" == false && "$WANT_AU" == false && "$WANT_STANDALONE" == false && "$WANT_CLAP" == false ]]; then
  echo "Erreur: aucun format valide demandé." >&2
  exit 1
fi

REPO_ROOT="$SCRIPT_DIR/.."
cd "$REPO_ROOT"

if [[ "$SKIP_BUILD" == false ]]; then
  echo "[1/4] Configuration CMake..."
  if [[ "$WANT_AU" == true ]]; then
    ENABLE_AU=ON
  else
    ENABLE_AU=OFF
  fi
  if [[ "$WANT_CLAP" == true ]]; then
    ENABLE_CLAP=ON
  else
    ENABLE_CLAP=OFF
  fi

  cmake -S . -B "$BUILD_DIR" -G "$GENERATOR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DJUCE_PATH="$JUCE_PATH_ARG" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCHS" \
    -DOVT_ENABLE_AU="$ENABLE_AU" \
    -DOVT_ENABLE_CLAP="$ENABLE_CLAP"

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

  if [[ "$WANT_CLAP" == true ]]; then
    echo "[2/4] Build target OpenVoxTuner_CLAP..."
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target OpenVoxTuner_CLAP
  fi

  # Companion key-detection plug-in (OpenVoxKey) is built for the SAME formats
  # as the main plug-in (VST3/AU/Standalone/CLAP), never VST3-only. JUCE target
  # names use the native format case (Standalone, not STANDALONE).
  if [[ "$WANT_COMPANION" == true ]]; then
    for fmt in "${ITEMS[@]}"; do
      case "$fmt" in
        STANDALONE) jt="Standalone" ;;
        *)          jt="$fmt" ;;
      esac
      echo "[2/4] Build target OpenVoxKey_${jt} (companion)..."
      cmake --build "$BUILD_DIR" --config "$CONFIG" --target "OpenVoxKey_${jt}"
    done
  fi
fi

echo "[3/4] Préparation du contenu package..."

# Each format is packaged as its own component so the installer can let the user
# choose which ones to install (Standalone / VST3 / AU / CLAP). Each component
# carries a unique package identifier, and (patched to a unique
# CFBundleIdentifier in the loop below) a unique bundle id, so relocation/upgrade
# stays unambiguous.
# Entry fields: artefactDir|artefactSubdir|destRel|bundleName|pkgId|choiceId|title|description
#   artefactDir    : dossier sous $BUILD_DIR (ex. OpenVoxTuner_artefacts ou OpenVoxKey_artefacts)
#   artefactSubdir : sous-dossier dans artefacts/Release/ (ex. Standalone, VST3, Components, CLAP)
#   destRel        : chemin d'installation relatif a la racine (ex. Library/Audio/Plug-Ins/VST3)
COMP_DIR="$BUILD_DIR/components"
# JUCE ships plugin bundles with read-only files; make them writable before
# removing so the clean rm -rf never fails on a stale staging dir.
chmod -R u+w "$COMP_DIR" 2>/dev/null || true
rm -rf "$COMP_DIR"
mkdir -p "$COMP_DIR"

NL=$'\n'
OVT_COMP_ENTRIES=()
[[ "$WANT_STANDALONE" == true ]] && OVT_COMP_ENTRIES+=( "OpenVoxTuner_artefacts|Standalone|Applications|OpenVoxTuner.app|com.eiffelbs.openvoxtuner.standalone|choice_standalone|Standalone (Application)|Application autonome OpenVoxTuner." )
[[ "$WANT_VST3" == true ]]      && OVT_COMP_ENTRIES+=( "OpenVoxTuner_artefacts|VST3|Library/Audio/Plug-Ins/VST3|OpenVoxTuner.vst3|com.eiffelbs.openvoxtuner.vst3|choice_vst3|VST3|Plug-in VST3 pour les DAW." )
[[ "$WANT_AU" == true ]]        && OVT_COMP_ENTRIES+=( "OpenVoxTuner_artefacts|AU|Library/Audio/Plug-Ins/Components|OpenVoxTuner.component|com.eiffelbs.openvoxtuner.au|choice_au|Audio Unit (AU)|Plug-in Audio Unit (macOS)." )
[[ "$WANT_CLAP" == true ]]      && OVT_COMP_ENTRIES+=( "OpenVoxTuner_artefacts|CLAP|Library/Audio/Plug-Ins/CLAP|OpenVoxTuner.clap|com.eiffelbs.openvoxtuner.clap|choice_clap|CLAP|Plug-in CLAP pour les DAW." )

# Companion (OpenVoxKey) follows the SAME formats as the main plug-in. Its format
# choices are grouped under a single parent choice so the user sees one "OpenVoxKey"
# toggle that installs every requested companion format.
OVT_COMPANION_PARENT="choice_companion"
if [[ "$WANT_COMPANION" == true ]]; then
  OVT_COMP_ENTRIES+=( "PARENT|${OVT_COMPANION_PARENT}|OpenVoxKey (companion)|Plug-in compagnon de detection de tonalite (partage la tonalite avec OpenVoxTuner)." )
  for fmt in "${ITEMS[@]}"; do
    case "$fmt" in
      STANDALONE) comp_subdir="Standalone";  comp_bundle="OpenVoxKey.app";  comp_dest="Applications";                          comp_pkgid="com.eiffelbs.openvoxkey.standalone" ;;
      VST3)       comp_subdir="VST3";        comp_bundle="OpenVoxKey.vst3"; comp_dest="Library/Audio/Plug-Ins/VST3";        comp_pkgid="com.eiffelbs.openvoxkey.vst3" ;;
      AU)         comp_subdir="AU";          comp_bundle="OpenVoxKey.component"; comp_dest="Library/Audio/Plug-Ins/Components"; comp_pkgid="com.eiffelbs.openvoxkey.au" ;;
      CLAP)       comp_subdir="CLAP";        comp_bundle="OpenVoxKey.clap"; comp_dest="Library/Audio/Plug-Ins/CLAP";        comp_pkgid="com.eiffelbs.openvoxkey.clap" ;;
    esac
    comp_choice="choice_companion_$(echo "$fmt" | tr '[:upper:]' '[:lower:]')"
    OVT_COMP_ENTRIES+=( "OpenVoxKey_artefacts|${comp_subdir}|${comp_dest}|${comp_bundle}|${comp_pkgid}|${comp_choice}|OpenVoxKey (${fmt})|Format ${fmt} du plug-in compagnon OpenVoxKey." )
  done
fi

OUTLINE_XML=""
CHOICES_XML=""
PKG_REFS_XML=""

# Accumulate companion child choice ids to nest them under the parent choice.
OVT_COMPANION_CHILDREN=()
for entry in "${OVT_COMP_ENTRIES[@]}"; do
  IFS='|' read -r artefact_dir artefact_subdir dest_rel bundle_name pkg_id choice_id title desc <<< "$entry"

  # PARENT entries declare a grouping choice (no package); their following
  # child entries are nested under them via <child> references. They use a
  # 4-field schema: PARENT|<choiceId>|<title>|<description>.
  if [[ "$artefact_dir" == "PARENT" ]]; then
    IFS='|' read -r _pfx OVT_COMPANION_PARENT_ID OVT_COMPANION_PARENT_TITLE OVT_COMPANION_PARENT_DESC <<< "$entry"
    OVT_COMPANION_CHILDREN=()
    continue
  fi

  SRC="$BUILD_DIR/$artefact_dir/$CONFIG/$artefact_subdir/$bundle_name"
  [[ -d "$SRC" ]] || { echo "Bundle introuvable: $SRC" >&2; exit 1; }

  root="$COMP_DIR/$choice_id"
  rm -rf "$root"
  mkdir -p "$root/$(dirname "$dest_rel")"
  rsync -a --delete "$SRC" "$root/$dest_rel/"

  # JUCE assigns EVERY plugin format the SAME CFBundleIdentifier
  # (com.EiffelBS.OpenVoxTuner, derived from the base target). The macOS Installer
  # builds its relocation rules from that id; identical ids make it collapse the
  # bundles and abort with "Unable to move 'OpenVoxTuner.component' to
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

  # Code-sign the bundle (required before notarization). Developer ID Application
  # + hardened runtime + entitlements. The AU must also pass `auval -v` for the
  # host to load it on a hardened macOS.
  if [[ -n "$SIGN_IDENTITY" ]]; then
    ENT_ARGS=()
    if [[ -n "$ENTITLEMENTS" && -f "$ENTITLEMENTS" ]]; then
      ENT_ARGS+=(--entitlements "$ENTITLEMENTS")
    fi
    echo "    codesign $bundle_name ..."
    codesign --force --options runtime --timestamp \
             "${ENT_ARGS[@]}" \
             --sign "$SIGN_IDENTITY" \
             "$root/$dest_rel/$bundle_name"
  fi

  comp_pkg="$COMP_DIR/OpenVoxTuner-$choice_id.pkg"
  pkgbuild --root "$root" --identifier "$pkg_id" --version "$VERSION" \
           --install-location "/" "$comp_pkg"

  kb=$(( $(stat -f%z "$comp_pkg") / 1024 ))
  PKG_REFS_XML+="    <pkg-ref id=\"$pkg_id\" version=\"$VERSION\" installKBytes=\"$kb\">#OpenVoxTuner-$choice_id.pkg</pkg-ref>${NL}"

  if [[ "$choice_id" == choice_companion_* ]]; then
    # Companion child: track for nesting under the parent choice, and emit its
    # own <choice> (with pkg-ref) — but NOT an outline line (it lives under the
    # parent in the outline).
    OVT_COMPANION_CHILDREN+=("$choice_id")
    CHOICES_XML+="    <choice id=\"$choice_id\" visible=\"true\" title=\"$title\" description=\"$desc\">${NL}        <pkg-ref id=\"$pkg_id\"/>${NL}    </choice>${NL}"
  else
    OUTLINE_XML+="        <line choice=\"$choice_id\"/>${NL}"
    CHOICES_XML+="    <choice id=\"$choice_id\" visible=\"true\" title=\"$title\" description=\"$desc\">${NL}        <pkg-ref id=\"$pkg_id\"/>${NL}    </choice>${NL}"
  fi
done

# Emit the companion parent choice (with its children nested) once all
# companion formats have been packaged.
if [[ -n "${OVT_COMPANION_PARENT_ID:-}" ]]; then
  child_xml=""
  outline_xml="        <line choice=\"${OVT_COMPANION_PARENT_ID}\">${NL}"
  for child in "${OVT_COMPANION_CHILDREN[@]}"; do
    child_xml+="        <child choice=\"${child}\"/>${NL}"
    outline_xml+="            <line choice=\"${child}\"/>${NL}"
  done
  outline_xml+="        </line>${NL}"
  OUTLINE_XML+="$outline_xml"
  CHOICES_XML+="    <choice id=\"${OVT_COMPANION_PARENT_ID}\" visible=\"true\" title=\"${OVT_COMPANION_PARENT_TITLE}\" description=\"${OVT_COMPANION_PARENT_DESC}\">${NL}${child_xml}    </choice>${NL}"
fi

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

# --- Local (un)signed test build ---
# Without a Developer ID, Gatekeeper flags the .pkg as quarantined and the
# Finder refuses to open it on double-click. --local clears the quarantine
# attribute so the pkg can be installed locally for testing (sudo installer
# works regardless). This is NOT a substitute for real signing/notarization.
if [[ "$LOCAL_UNSIGN" == true ]]; then
  if [[ -n "$SIGN_INSTALLER" || "$NOTARIZE" == true ]]; then
    echo "Erreur: --local est incompatible avec --sign-installer/--notarize (pkg deja signe)." >&2
    exit 1
  fi
  echo "[+] Levee de la quarantaine Gatekeeper (--local)..."
  xattr -dr com.apple.quarantine "$OUTPUT" 2>/dev/null || true
  echo "    Quarantaine levee sur: $OUTPUT"
  echo "    Installation locale: sudo installer -pkg $OUTPUT -target /"
fi

# --- Notarization (optional) ---
if [[ "$NOTARIZE" == true ]]; then
  if [[ -z "$SIGN_INSTALLER" || -z "$SIGN_IDENTITY" ]]; then
    echo "Erreur: --notarize requiert --sign-installer et --sign-identity (pkg + bundles signés)." >&2
    exit 1
  fi
  echo "[5/5] Notarisation du pkg..."
  NOTARY_ARGS=()
  if [[ -n "${APPLE_API_KEY:-}" && -n "${APPLE_API_KEY_ID:-}" && -n "${APPLE_API_ISSUER:-}" ]]; then
    NOTARY_ARGS+=(--key "$APPLE_API_KEY" --key-id "$APPLE_API_KEY_ID" --issuer "$APPLE_API_ISSUER")
  elif [[ -n "${APPLE_ID:-}" && -n "${APPLE_PASSWORD:-}" && -n "${APPLE_TEAM_ID:-}" ]]; then
    NOTARY_ARGS+=(--apple-id "$APPLE_ID" --password "$APPLE_PASSWORD" --team-id "$APPLE_TEAM_ID")
  else
    echo "Erreur: credentials notarytool manquants (APPLE_API_KEY* ou APPLE_ID*)." >&2
    exit 1
  fi
  xcrun notarytool submit "$OUTPUT" "${NOTARY_ARGS[@]}" --wait
  xcrun stapler staple "$OUTPUT"
  echo "[OK] pkg notarisé et staple."
fi
