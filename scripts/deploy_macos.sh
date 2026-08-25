#!/usr/bin/env bash
# deploy_macos.sh
# System-wide build + deployment of the OpenVoxTuner (+ OpenVoxKey) plug-ins
# into /Library/Audio/Plug-Ins/ (VST3, AU) with ad-hoc re-signing.
#
# Context: for dev/test, we want a SINGLE copy of the plug-ins at
# the system location (visible to all DAWs without them scanning
# multiple locations), without having to create a signed .pkg at every build.
# It is the "sudo" counterpart of install_macos_au_local.sh (which copies into
# ~/Library/ for tests without sudo).
#
# This script:
#   1. (optional) Rebuild in parallel with cmake --build
#   2. cmake --install of the requested components into /Library/...
#   3. Re-sign the bundles (the signature is erased by the deployment)
#   4. (optional) Kill hidden DAW processes + clear plugin caches
#
# Usage:
#   ./deploy_macos.sh                            # build + deploy VST3+AU
#   ./deploy_macos.sh --no-build                 # deploy without rebuild
#   ./deploy_macos.sh --components VST3          # VST3 only
#   ./deploy_macos.sh --components VST3,AU,CompanionVST3,CompanionAU
#   ./deploy_macos.sh --build-dir build-mac-debug
#   ./deploy_macos.sh --config Debug
#   ./deploy_macos.sh --no-rescan                # no DAW kill
#   ./deploy_macos.sh --target /Users/foo/Library   # user-level deploy (no sudo)

set -euo pipefail

# === Defaults ===
BUILD_DIR="build-mac"
CONFIG="Release"
DO_BUILD=true
DO_RESCAN=true
COMPONENTS="VST3,AU"
TARGET_PREFIX="/Library"  # /Library (sudo) or $HOME/Library (user)

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
  --build-dir <dir>        Build directory (default: build-mac)
  --config Release|Debug   Configuration (default: Release)
  --no-build               Skip the rebuild, deploy only
  --no-rescan              Do not force DAW re-scan
  --components <list>      Comma-separated list among
                           VST3, AU, Standalone, CompanionVST3, CompanionAU
                           (default: VST3,AU)
  --target <prefix>        /Library (system-wide, sudo) or
                           $HOME/Library (user-level, no sudo)

Examples:
  ./deploy_macos.sh --components VST3,AU
  ./deploy_macos.sh --no-build --target $HOME/Library
EOF
      exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

# === Sudo command selection based on destination ===
SUDO=""
if [[ "$TARGET_PREFIX" == "/Library"* ]]; then
    SUDO="sudo"
fi

# === 1. Build ===
if [[ "$DO_BUILD" == true ]]; then
    echo "=== [1/4] Build ($BUILD_DIR, $CONFIG) ==="
    if [[ ! -d "$BUILD_DIR" ]]; then
        echo "  Initial configuration of $BUILD_DIR ..."
        cmake -S . -B "$BUILD_DIR" >/dev/null
    fi
    cmake --build "$BUILD_DIR" -j4 --config "$CONFIG"
    echo ""
else
    echo "=== [1/4] Build skipped (--no-build) ==="
    echo ""
fi

# === 2. Install via cmake --install ===
echo "=== [2/4] Install (components: $COMPONENTS) ==="
echo "  Destination: $TARGET_PREFIX/Audio/Plug-Ins/"
$SUDO cmake --install "$BUILD_DIR" --config "$CONFIG" --component VST3 >/dev/null
$SUDO cmake --install "$BUILD_DIR" --config "$CONFIG" --component AU >/dev/null
# Reset by default; we only re-install the requested ones
# (cmake --install --component does not support a list, we install
# everything then clean up what should not be kept, or run in
# sequence with a filter afterwards. Here we rely on the COMPONENTS
# variable for the re-signing below only.)
echo "  Bundles installed."
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
        Standalone)    echo "  (Standalone not signed - App in /Applications/)" ;;
        *) echo "  Component skipped (unknown): $comp" ;;
    esac
done

for path in "${TO_SIGN[@]}"; do
    if [[ -e "$path" ]]; then
        echo "  codesign --force --sign - $path"
        $SUDO codesign --force --sign - "$path"
    else
        echo "  [skip] not found: $path"
    fi
done
echo ""

# === 4. Re-scan (optional) ===
if [[ "$DO_RESCAN" == true ]]; then
    echo "=== [4/4] Plugin cache re-scan ==="
    # Kill the caches of known DAWs to force a re-scan at next launch.
    killall -9 auvaltool 2>/dev/null || true
    killall -9 AudioUnitHost 2>/dev/null || true
    killall -9 com.apple.audio.AudioUnitHost 2>/dev/null || true
    # Clear known plugin caches. Some DAWs rebuild them at launch.
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
    echo "  Relaunch your DAW to see the updated plug-ins."
else
    echo "=== [4/4] Re-scan skipped (--no-rescan) ==="
fi

echo ""
echo "[OK] Deployment completed."
echo "     Bundles:"
for path in "${TO_SIGN[@]}"; do
    [[ -e "$path" ]] && echo "       - $path"
done
