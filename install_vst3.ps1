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

Write-Host "=== Installation rapide de OpenVoxTuner VST3 ===" -ForegroundColor Cyan
Write-Host "Configuration: $buildType" -ForegroundColor Yellow
Write-Host "Source: $vst3Source" -ForegroundColor Yellow
Write-Host "Destination: $vst3Dest" -ForegroundColor Yellow
Write-Host ""
Write-Host "NOTE: Pour une installation complete, utilisez plutot:" -ForegroundColor Cyan
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

# Verification de l'existence du fichier source
if (-not (Test-Path $vst3Source)) {
    Write-Host "ERREUR: Le VST3 source n'existe pas : $vst3Source" -ForegroundColor Red
    Write-Host "Compilez d'abord le projet dans Visual Studio (configuration $buildType)" -ForegroundColor Yellow
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Suppression de l'ancien VST3 si present
if (Test-Path $vst3Dest) {
    Write-Host "Suppression de l'ancienne version..." -ForegroundColor Yellow
    Remove-Item -Path $vst3Dest -Recurse -Force
    Write-Host "Ancienne version supprimee" -ForegroundColor Green
}

# Copie du nouveau VST3 (bundle complet avec tous les sous-dossiers)
Write-Host "Copie du nouveau VST3..." -ForegroundColor Yellow
Copy-Item -Path $vst3Source -Destination $vst3Dest -Recurse -Force

Write-Host ""
Write-Host "=== Installation terminee avec succes ! ===" -ForegroundColor Green
Write-Host "Le plugin OpenVoxTuner est maintenant installe dans :" -ForegroundColor Cyan
Write-Host $vst3Dest -ForegroundColor White
Write-Host ""
Write-Host "Relancez votre DAW pour charger la nouvelle version." -ForegroundColor Yellow
Write-Host ""

# Afficher le contenu du bundle pour verification
Write-Host "Contenu du bundle installe :" -ForegroundColor Cyan
Get-ChildItem -Path $vst3Dest -Recurse | Select-Object FullName | Format-Table -AutoSize

Read-Host "Appuyez sur Entree pour quitter"
