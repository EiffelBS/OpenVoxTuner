# Script d'installation rapide du VST3 OpenVoxTuner (pour developpement)
# Execute ce script en mode administrateur (clic droit > Executer en tant qu'administrateur)
#
# OPTION 1 (RECOMMANDEE): Utiliser l'installeur Inno Setup
#   .\build_installer.ps1
#   Puis executer: .\build\installer\OpenVoxTuner_Windows_Installer.exe
#
# OPTION 2: Utiliser ce script pour une installation rapide pendant le developpement

$ErrorActionPreference = "Stop"

# Configuration
$buildType = "Debug"  # Changez en "Release" pour la version Release
$vst3Source = "$PSScriptRoot\build\OpenVoxTuner_artefacts\$buildType\VST3\OpenVoxTuner.vst3"
$vst3Dest = "$env:CommonProgramFiles\VST3\OpenVoxTuner.vst3"
$clapSource = "$PSScriptRoot\build\OpenVoxTuner_artefacts\$buildType\CLAP\OpenVoxTuner.clap"
$clapDest = "$env:CommonProgramFiles\CLAP\OpenVoxTuner.clap"

Write-Host "=== Installation rapide de OpenVoxTuner ===" -ForegroundColor Cyan
Write-Host "Configuration: $buildType" -ForegroundColor Yellow
Write-Host "Source VST3: $vst3Source" -ForegroundColor Yellow
Write-Host "Dest VST3  : $vst3Dest" -ForegroundColor Yellow
Write-Host "Source CLAP: $clapSource" -ForegroundColor Yellow
Write-Host "Dest CLAP  : $clapDest" -ForegroundColor Yellow
Write-Host ""
Write-Host "NOTE: Pour une installation complete (avec installeur), utilisez plutot:" -ForegroundColor Cyan
Write-Host "  .\build_installer.ps1" -ForegroundColor White
Write-Host "  .\build\installer\OpenVoxTuner_Windows_Installer.exe" -ForegroundColor White
Write-Host ""

# Verification des droits admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERREUR: Ce script doit etre execute en tant qu'administrateur !" -ForegroundColor Red
    Write-Host "Clic droit sur le fichier > Executer en tant qu'administrateur" -ForegroundColor Yellow
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Verification de l'existence du VST3 source
if (-not (Test-Path $vst3Source)) {
    Write-Host "ERREUR: Le VST3 source n'existe pas : $vst3Source" -ForegroundColor Red
    Write-Host "Compilez d'abord le projet dans Visual Studio (configuration $buildType)" -ForegroundColor Yellow
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Suppression de l'ancien VST3 si present
if (Test-Path $vst3Dest) {
    Write-Host "Suppression de l'ancienne version VST3..." -ForegroundColor Yellow
    Remove-Item -Path $vst3Dest -Recurse -Force
    Write-Host "Ancienne version VST3 supprimee" -ForegroundColor Green
}

# Copie du nouveau VST3 (bundle complet avec tous les sous-dossiers)
Write-Host "Copie du nouveau VST3..." -ForegroundColor Yellow
Copy-Item -Path $vst3Source -Destination $vst3Dest -Recurse -Force

# Traitement du format CLAP si disponible
if (Test-Path $clapSource) {
    if (Test-Path $clapDest) {
        Write-Host "Suppression de l'ancienne version CLAP..." -ForegroundColor Yellow
        Remove-Item -Path $clapDest -Force
        Write-Host "Ancienne version CLAP supprimee" -ForegroundColor Green
    }
    
    Write-Host "Copie du nouveau CLAP..." -ForegroundColor Yellow
    $clapDestDir = Split-Path -Path $clapDest -Parent
    if (-not (Test-Path $clapDestDir)) {
        New-Item -ItemType Directory -Path $clapDestDir -Force | Out-Null
    }
    Copy-Item -Path $clapSource -Destination $clapDest -Force
    Write-Host "CLAP installe dans : $clapDest" -ForegroundColor Green
} else {
    Write-Host "CLAP non trouve (cible non compilee pour $buildType), saut de l'installation CLAP" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Installation terminee avec succes ! ===" -ForegroundColor Green
Write-Host "Le plugin OpenVoxTuner VST3 est installe dans : $vst3Dest" -ForegroundColor Cyan
if (Test-Path $clapDest) {
    Write-Host "Le plugin OpenVoxTuner CLAP est installe dans : $clapDest" -ForegroundColor Cyan
}
Write-Host ""
Write-Host "Relancez votre DAW pour charger la nouvelle version." -ForegroundColor Yellow
Write-Host ""

Read-Host "Appuyez sur Entree pour quitter"
