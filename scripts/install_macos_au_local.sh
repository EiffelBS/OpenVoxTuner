#!/usr/bin/env bash
# install_macos_au_local.sh
# Installs the OpenVoxTuner + OpenVoxKey Audio Unit (AU) plug-ins into the user
# ~/Library/Audio/Plug-Ins/Components directory, WITHOUT Developer ID signing.
#
# Context: an AU can be COMPILED without a certificate, but macOS (Gatekeeper /
# hardened runtime) refuses to load it as long as it is not signed. While waiting
# for the Developer ID, this script deploys/tests locally:
#   - copies the .component into the user directory (no sudo required),
#   - removes the quarantine attribute (com.apple.quarantine) that macOS puts on any
#     binary downloaded/compiled outside the App Store,
#   - forces AU re-scan (kill of AU processes + auval).
#
# The definitive signing/notarization will be done later via build_macos_pkg.sh
# (--sign-identity / --sign-installer / --notarize) once the certificate is obtained.
#
# Prerequisite: have run a build containing the AU (build_macos.sh, build_macos_au.sh
# or build_macos_pkg.sh with --formats ...AU...).
#
# Usage:
#   ./install_macos_au_local.sh                       # default build dir
#   ./install_macos_au_local.sh --build-dir build-mac # specific build directory
#   ./install_macos_au_local.sh --config Debug
#   ./install_macos_au_local.sh --no-rescan           # do not force AU re-scan

set -euo pipefail

# === Defaults ===
BUILD_DIR="build-mac-pkg"
CONFIG="Release"
RESCAN=true

# === Parse arguments ===
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir) BUILD_DIR="${2:-}"; shift 2 ;;
    --config)    CONFIG="${2:-}"; shift 2 ;;
    --no-rescan) RESCAN=false; shift ;;
    --help|-h)
      cat <<'EOF'
Usage: ./install_macos_au_local.sh [--build-dir <dir>] [--config Release|Debug] [--no-rescan]

Installs the OpenVoxTuner + OpenVoxKey AUs (unsigned) into
~/Library/Audio/Plug-Ins/Components for local testing.
EOF
      exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

DEST_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$DEST_DIR"

# The AU artifacts live in OpenVoxTuner_artefacts and OpenVoxKey_artefacts.
TUNER_AU="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/AU/OpenVoxTuner.component"
KEY_AU="$BUILD_DIR/OpenVoxKey_artefacts/$CONFIG/AU/OpenVoxKey.component"

echo "=== OpenVoxTuner - Local AU install (unsigned) ==="

if [[ ! -d "$TUNER_AU" ]]; then
  echo "Error: OpenVoxTuner AU not found: $TUNER_AU" >&2
  echo "Run a build with the AU first (e.g.: ./scripts/build_macos_au.sh)." >&2
  exit 1
fi
if [[ ! -d "$KEY_AU" ]]; then
  echo "Error: OpenVoxKey AU not found: $KEY_AU" >&2
  echo "Run a build with the AU first (e.g.: ./scripts/build_macos_pkg.sh --formats VST3,AU,STANDALONE)." >&2
  exit 1
fi

echo "[1/3] Copying the .component into $DEST_DIR ..."
rsync -a --delete "$TUNER_AU" "$DEST_DIR/"
rsync -a --delete "$KEY_AU"   "$DEST_DIR/"
echo "  OpenVoxTuner.component -> $DEST_DIR/"
echo "  OpenVoxKey.component   -> $DEST_DIR/"

echo "[2/3] Removing Gatekeeper quarantine (unsigned AUs)..."
# macOS marks every binary compiled/downloaded outside the App Store with the
# com.apple.quarantine attribute; without this, DAWs refuse to load the AU.
xattr -dr com.apple.quarantine "$DEST_DIR/OpenVoxTuner.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "$DEST_DIR/OpenVoxKey.component"   2>/dev/null || true
echo "  Quarantine removed (xattr -dr com.apple.quarantine)."

if [[ "$RESCAN" == true ]]; then
  echo "[3/3] Forcing Audio Units re-scan..."
  # Kill the processes caching the AU list to force a re-scan at next launch.
  killall -9 AudioUnitHost auvaltool com.apple.audio.AudioUnitHost 2>/dev/null || true
  # auval validates and registers the AU with the system (useful even without signing
  # to verify that the component loads). Non-blocking if it fails.
  if command -v auval >/dev/null 2>&1; then
    auval -v aufx OvtP Eiff 2>/dev/null || echo "  (auval OpenVoxTuner not valid without signing - normal in local dev)"
    auval -v aufx OvtK Eiff 2>/dev/null || echo "  (auval OpenVoxKey not valid without signing - normal in local dev)"
  fi
  echo "  Re-scan requested. Relaunch your DAW to see the AUs."
else
  echo "[3/3] Re-scan skipped (--no-rescan)."
fi

echo ""
echo "[OK] AUs installed locally (unsigned). For the signed/notarized version,"
echo "      use build_macos_pkg.sh with --sign-identity / --sign-installer / --notarize."
