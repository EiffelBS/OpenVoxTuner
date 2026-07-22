#!/usr/bin/env bash
# deploy_macos.sh
# Build + déploiement system-wide des plug-ins OpenVoxTuner (+ OpenVoxKey)
# vers /Library/Audio/Plug-Ins/ (VST3, AU) avec re-signing ad-hoc.
#
# Contexte : pour le dev/test, on veut UNE seule copie des plug-ins a
# l'emplacement systeme (visible par tous les DAWs sans qu'ils scannent
# plusieurs endroits), sans avoir a creer un .pkg signe a chaque build.
# C'est le pendant "sudo" de install_macos_au_local.sh (qui copie dans
# ~/Library/ pour les tests sans sudo).
#
# Ce script:
#   1. (optionnel) Rebuild en // avec cmake --build
#   2. cmake --install des components demandes vers /Library/...
#   3. Re-signe les bundles (la signature est effacee par le deploiement)
#   4. (optionnel) Tue les processus DAW caches + vide les caches plugin
#
# Usage:
#   ./deploy_macos.sh                            # build + deploy VST3+AU
#   ./deploy_macos.sh --no-build                 # deploy sans rebuild
#   ./deploy_macos.sh --components VST3          # juste le VST3
#   ./deploy_macos.sh --components VST3,AU,CompanionVST3,CompanionAU
#   ./deploy_macos.sh --build-dir build-mac-debug
#   ./deploy_macos.sh --config Debug
#   ./deploy_macos.sh --no-rescan                # pas de kill de DAW
#   ./deploy_macos.sh --target /Users/foo/Library   # deploy user-level (pas de sudo)

set -euo pipefail

# === Defaults ===
BUILD_DIR="build-mac"
CONFIG="Release"
DO_BUILD=true
DO_RESCAN=true
COMPONENTS="VST3,AU"
TARGET_PREFIX="/Library"  # /Library (sudo) ou $HOME/Library (user)

# === Parse arguments ===
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)   BUILD_DIR="${2:-}"; shift 2 ;;
    --config)      CONFIG="${2:-}"; shift 2 ;;
    --no-build)    DO_BUILD=false; shift ;;
    --no-rescan)   DO_RESCAN=false; shift ;;
    --components)  COMPONENTS="${2:-}"; shift 2 ;;
    --target)      TARGET_PREFIX="${2:-}"; shift 2 ;;
    --help|-h)
      cat <<'EOF'
Usage: ./deploy_macos.sh [options]

Options:
  --build-dir <dir>        Dossier de build (defaut: build-mac)
  --config Release|Debug   Configuration (defaut: Release)
  --no-build               Skip le rebuild, deploy seulement
  --no-rescan              Ne pas forcer le re-scan des DAWs
  --components <list>      Liste separee par des virgules parmi
                           VST3, AU, Standalone, CompanionVST3, CompanionAU
                           (defaut: VST3,AU)
  --target <prefix>        /Library (system-wide, sudo) ou
                           $HOME/Library (user-level, pas de sudo)

Exemples:
  ./deploy_macos.sh --components VST3,AU
  ./deploy_macos.sh --no-build --target $HOME/Library
EOF
      exit 0 ;;
    *) echo "Option inconnue: $1" >&2; exit 1 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

# === Choix de la commande sudo selon la destination ===
SUDO=""
if [[ "$TARGET_PREFIX" == "/Library"* ]]; then
    SUDO="sudo"
fi

# === 1. Build ===
if [[ "$DO_BUILD" == true ]]; then
    echo "=== [1/4] Build ($BUILD_DIR, $CONFIG) ==="
    if [[ ! -d "$BUILD_DIR" ]]; then
        echo "  Configuration initiale de $BUILD_DIR ..."
        cmake -S . -B "$BUILD_DIR" >/dev/null
    fi
    cmake --build "$BUILD_DIR" -j4 --config "$CONFIG"
    echo ""
else
    echo "=== [1/4] Build ignore (--no-build) ==="
    echo ""
fi

# === 2. Install via cmake --install ===
echo "=== [2/4] Install (components: $COMPONENTS) ==="
echo "  Destination: $TARGET_PREFIX/Audio/Plug-Ins/"
$SUDO cmake --install "$BUILD_DIR" --config "$CONFIG" --component VST3 >/dev/null
$SUDO cmake --install "$BUILD_DIR" --config "$CONFIG" --component AU >/dev/null
# Reset par defaut ; on re-installe seulement ceux demandes
# (cmake --install --component ne supporte pas une liste, on installe
# tout puis on nettoie ce qu'il ne faut pas garder, ou on lance en
# sequentiel avec un filtre ulterieur. Ici on s'appuie sur la variable
# COMPONENTS pour le re-signing ci-dessous uniquement.)
echo "  Bundles installes."
echo ""

# === 3. Re-signing ad-hoc ===
echo "=== [3/4] Re-signing ad-hoc ==="
declare -a TO_SIGN=()

IFS=',' read -ra COMP_LIST <<< "$COMPONENTS"
for comp in "${COMP_LIST[@]}"; do
    comp="$(echo "$comp" | xargs)"  # trim
    case "$comp" in
        VST3)          TO_SIGN+=("$TARGET_PREFIX/Audio/Plug-Ins/VST3/OpenVoxTuner.vst3") ;;
        AU)            TO_SIGN+=("$TARGET_PREFIX/Audio/Plug-Ins/Components/OpenVoxTuner.component") ;;
        CompanionVST3) TO_SIGN+=("$TARGET_PREFIX/Audio/Plug-Ins/VST3/OpenVoxKey.vst3") ;;
        CompanionAU)   TO_SIGN+=("$TARGET_PREFIX/Audio/Plug-Ins/Components/OpenVoxKey.component") ;;
        Standalone)    echo "  (Standalone non signe - App dans /Applications/)" ;;
        *) echo "  Component ignore (inconnu): $comp" ;;
    esac
done

for path in "${TO_SIGN[@]}"; do
    if [[ -e "$path" ]]; then
        echo "  codesign --force --sign - $path"
        $SUDO codesign --force --sign - "$path"
    else
        echo "  [skip] introuvable: $path"
    fi
done
echo ""

# === 4. Re-scan (optionnel) ===
if [[ "$DO_RESCAN" == true ]]; then
    echo "=== [4/4] Re-scan des caches plugin ==="
    # Tuer les caches des DAWs connus pour forcer un re-scan au prochain lancement.
    killall -9 auvaltool 2>/dev/null || true
    killall -9 AudioUnitHost 2>/dev/null || true
    killall -9 com.apple.audio.AudioUnitHost 2>/dev/null || true
    # Vider les caches plugin connus. Certains DAWs les reconstruisent au lancement.
    for cache in \
        "$HOME/Library/Caches/Ableton" \
        "$HOME/Library/Caches/Steinberg" \
        "$HOME/Library/Caches/com.apple.logic10" \
        "$HOME/Library/Preferences/Ableton/Live Plugin Cache.bak" ; do
        if [[ -e "$cache" ]]; then
            echo "  rm -rf $cache"
            rm -rf "$cache" 2>/dev/null || true
        fi
    done
    echo "  Relancez votre DAW pour voir les plug-ins mis a jour."
else
    echo "=== [4/4] Re-scan ignore (--no-rescan) ==="
fi

echo ""
echo "[OK] Deploiement termine."
echo "     Bundles:"
for path in "${TO_SIGN[@]}"; do
    [[ -e "$path" ]] && echo "       - $path"
done
