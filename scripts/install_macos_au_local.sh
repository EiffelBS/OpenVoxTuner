#!/usr/bin/env bash
# install_macos_au_local.sh
# Installe les plug-ins Audio Unit (AU) OpenVoxTuner + OpenVoxKey dans le dossier
# utilisateur ~/Library/Audio/Plug-Ins/Components, SANS signature Developer ID.
#
# Contexte : une AU peut etre COMPILEE sans certificat, mais macOS (Gatekeeper /
# hardened runtime) refuse de la charger tant qu'elle n'est pas signee. En attendant
# le Developer ID, ce script devel/tes en local :
#   - copie les .component dans le dossier utilisateur (pas de sudo requis),
#   - leve l'attribut de quarantaine (com.apple.quarantine) que macOS pose sur tout
#     binaire telecharge/compile hors App Store,
#   - force le re-scan des AU (kill des processus AU + auval).
#
# La signature/notarization definitive se fera plus tard via build_macos_pkg.sh
# (--sign-identity / --sign-installer / --notarize) une fois le certificat obtenu.
#
# Pre-requis : avoir lance un build contenant l'AU (build_macos.sh, build_macos_au.sh
# ou build_macos_pkg.sh avec --formats ...AU...).
#
# Usage:
#   ./install_macos_au_local.sh                       # build dir par defaut
#   ./install_macos_au_local.sh --build-dir build-mac # dossier de build specifique
#   ./install_macos_au_local.sh --config Debug
#   ./install_macos_au_local.sh --no-rescan           # ne pas forcer le re-scan AU

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

Installe les AU OpenVoxTuner + OpenVoxKey (non signees) dans
~/Library/Audio/Plug-Ins/Components pour test local.
EOF
      exit 0 ;;
    *) echo "Option inconnue: $1" >&2; exit 1 ;;
  esac
done

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

DEST_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$DEST_DIR"

# Les artefacts AU vivent dans OpenVoxTuner_artefacts et OpenVoxKey_artefacts.
TUNER_AU="$BUILD_DIR/OpenVoxTuner_artefacts/$CONFIG/AU/OpenVoxTuner.component"
KEY_AU="$BUILD_DIR/OpenVoxKey_artefacts/$CONFIG/AU/OpenVoxKey.component"

echo "=== OpenVoxTuner — Install AU local (non signe) ==="

if [[ ! -d "$TUNER_AU" ]]; then
  echo "Erreur: AU OpenVoxTuner introuvable: $TUNER_AU" >&2
  echo "Lancez d'abord un build avec l'AU (ex: ./scripts/build_macos_au.sh)." >&2
  exit 1
fi
if [[ ! -d "$KEY_AU" ]]; then
  echo "Erreur: AU OpenVoxKey introuvable: $KEY_AU" >&2
  echo "Lancez d'abord un build avec l'AU (ex: ./scripts/build_macos_pkg.sh --formats VST3,AU,STANDALONE)." >&2
  exit 1
fi

echo "[1/3] Copie des .component vers $DEST_DIR ..."
rsync -a --delete "$TUNER_AU" "$DEST_DIR/"
rsync -a --delete "$KEY_AU"   "$DEST_DIR/"
echo "  OpenVoxTuner.component -> $DEST_DIR/"
echo "  OpenVoxKey.component   -> $DEST_DIR/"

echo "[2/3] Lever la quarantaine Gatekeeper (AU non signees)..."
# macOS marque tout binaire compile/telecharge hors App Store avec l'attribut
# com.apple.quarantine ; sans cela, les DAW refusent de charger l'AU.
xattr -dr com.apple.quarantine "$DEST_DIR/OpenVoxTuner.component" 2>/dev/null || true
xattr -dr com.apple.quarantine "$DEST_DIR/OpenVoxKey.component"   2>/dev/null || true
echo "  Quarantaine levee (xattr -dr com.apple.quarantine)."

if [[ "$RESCAN" == true ]]; then
  echo "[3/3] Forcer le re-scan des Audio Units..."
  # Tuer les processus qui cachent la liste des AU pour forcer un re-scan au prochain lancement.
  killall -9 AudioUnitHost auvaltool com.apple.audio.AudioUnitHost 2>/dev/null || true
  # auval valide et enregistre l'AU aupres du systeme (utile meme sans signature
  # pour verifier que le composant se charge). Non bloquant si il echoue.
  if command -v auval >/dev/null 2>&1; then
    auval -v aufx OvtP Eiff 2>/dev/null || echo "  (auval OpenVoxTuner non valide sans signature — normal en dev local)"
    auval -v aufx OvtK Eiff 2>/dev/null || echo "  (auval OpenVoxKey non valide sans signature — normal en dev local)"
  fi
  echo "  Re-scan demande. Relancez votre DAW pour voir les AU."
else
  echo "[3/3] Re-scan ignore (--no-rescan)."
fi

echo ""
echo "[OK] AU installees localement (non signees). Pour la version signee/notarisee,"
echo "      utilisez build_macos_pkg.sh avec --sign-identity / --sign-installer / --notarize."
