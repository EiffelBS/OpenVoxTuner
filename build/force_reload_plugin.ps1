# force_reload_plugin.ps1
# Force le rechargement du plugin en supprimant tous les caches

$ErrorActionPreference = "Stop"

Write-Host "=== Forçage du Rechargement du Plugin ===" -ForegroundColor Cyan
Write-Host ""

# Verification admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERREUR: Ce script doit etre execute en mode Administrateur !" -ForegroundColor Red
    Write-Host ""
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

Write-Host "Ce script va :" -ForegroundColor Yellow
Write-Host "  1. Supprimer OpenVoxTuner.vst3 de Program Files" -ForegroundColor White
Write-Host "  2. Copier la version Debug la plus recente" -ForegroundColor White
Write-Host "  3. Suggerer de supprimer les caches DAW" -ForegroundColor White
Write-Host ""

$continue = Read-Host "Continuer ? (O/N)"
if ($continue -ne "O" -and $continue -ne "o") {
    Write-Host "Annule."
    exit 0
}

# 1. Supprimer l'ancien plugin
$dest = "$env:CommonProgramFiles\VST3\OpenVoxTuner.vst3"
Write-Host ""
Write-Host "[1/2] Suppression de l'ancien plugin..." -ForegroundColor Cyan
if (Test-Path $dest) {
    Remove-Item -Path $dest -Recurse -Force
    Write-Host "  -> Supprime" -ForegroundColor Green
} else {
    Write-Host "  -> Deja absent" -ForegroundColor Yellow
}

# Attendre un peu pour etre sur que Windows libere le fichier
Start-Sleep -Milliseconds 500

# 2. Copier la nouvelle version
$source = "C:\Users\User\Documents\trae_projects\VST3\build\OpenVoxTuner_artefacts\Debug\VST3\OpenVoxTuner.vst3"
Write-Host ""
Write-Host "[2/2] Copie de la nouvelle version..." -ForegroundColor Cyan

if (-not (Test-Path $source)) {
    Write-Host "ERREUR: Source introuvable !" -ForegroundColor Red
    Write-Host "Path: $source" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Compilez d'abord en Debug dans Visual Studio" -ForegroundColor Yellow
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

Copy-Item -Path $source -Destination $dest -Recurse -Force
Write-Host "  -> Copie" -ForegroundColor Green

# Verification
$installedDll = "$dest\Contents\x86_64-win\OpenVoxTuner.vst3"
$dllInfo = Get-Item $installedDll
Write-Host ""
Write-Host "Plugin installe :" -ForegroundColor Green
Write-Host "  Path  : $installedDll" -ForegroundColor White
Write-Host "  Date  : $($dllInfo.LastWriteTime)" -ForegroundColor White
Write-Host "  Taille: $($dllInfo.Length) bytes" -ForegroundColor White
Write-Host ""

# Instructions pour les caches DAW
Write-Host "=== IMPORTANT : Supprimer les caches DAW ===" -ForegroundColor Yellow
Write-Host ""
Write-Host "Selon votre DAW, supprimez les caches VST3 :" -ForegroundColor Cyan
Write-Host ""
Write-Host "Reaper :" -ForegroundColor White
Write-Host "  - Menu: Options > Preferences > Plug-ins > VST" -ForegroundColor White
Write-Host "  - Cliquez: 'Re-scan' puis 'Clear cache/re-scan'" -ForegroundColor White
Write-Host ""
Write-Host "Studio One :" -ForegroundColor White
Write-Host "  - Fermez Studio One" -ForegroundColor White
Write-Host "  - Supprimez: %AppData%\PreSonus\Studio One\x64\Settings\PluginBlacklist.txt" -ForegroundColor White
Write-Host "  - Relancez et rescannez" -ForegroundColor White
Write-Host ""
Write-Host "Ableton Live :" -ForegroundColor White
Write-Host "  - Preferences > Plug-Ins > Rescan" -ForegroundColor White
Write-Host ""
Write-Host "FL Studio :" -ForegroundColor White
Write-Host "  - Options > Manage Plugins > Plugin Manager" -ForegroundColor White
Write-Host "  - Clic droit sur le dossier VST3 > Fast Scan" -ForegroundColor White
Write-Host ""
Write-Host "Cubase/Nuendo :" -ForegroundColor White
Write-Host "  - Menu: Studio > VST Plug-in Manager" -ForegroundColor White
Write-Host "  - Bouton: Update Plug-in Information" -ForegroundColor White
Write-Host ""

Write-Host "Ensuite :" -ForegroundColor Cyan
Write-Host "  1. Fermez COMPLETEMENT le DAW (pas juste le projet)" -ForegroundColor White
Write-Host "  2. Relancez-le" -ForegroundColor White
Write-Host "  3. Chargez OpenVoxTuner" -ForegroundColor White
Write-Host "  4. Testez avec audio vocal" -ForegroundColor White
Write-Host ""
Write-Host "Logs disponibles dans: E:\Documents\OpenVoxTuner.log" -ForegroundColor Cyan
Write-Host ""

Read-Host "Appuyez sur Entree pour quitter"
