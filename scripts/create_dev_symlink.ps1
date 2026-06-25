# create_dev_symlink.ps1
# Cree un lien symbolique vers le VST3 en developpement
# Avantage : chaque rebuild Visual Studio met automatiquement a jour le plugin dans Program Files
# DOIT etre execute en mode Administrateur

$ErrorActionPreference = "Stop"

# Configuration : changez "Debug" en "Release" selon votre besoin
$buildConfig = "Debug"

$repoRoot = Split-Path $PSScriptRoot -Parent

$vst3Source = "$repoRoot\build\OpenVoxTuner_artefacts\$buildConfig\VST3\OpenVoxTuner.vst3"
$vst3Dest = "$env:CommonProgramFiles\VST3\OpenVoxTuner.vst3"

Write-Host "=== Creation d'un lien symbolique pour le developpement ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration : $buildConfig" -ForegroundColor Yellow
Write-Host "Source        : $vst3Source" -ForegroundColor Yellow
Write-Host "Destination   : $vst3Dest" -ForegroundColor Yellow
Write-Host ""

# Verification des droits admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERREUR: Ce script doit etre execute en tant qu'administrateur !" -ForegroundColor Red
    Write-Host "Clic droit sur le fichier > Executer en tant qu'administrateur" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Verification de l'existence du source
if (-not (Test-Path $vst3Source)) {
    Write-Host "ERREUR: Le VST3 source n'existe pas !" -ForegroundColor Red
    Write-Host "Path: $vst3Source" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Compilez d'abord le projet en configuration '$buildConfig' dans Visual Studio." -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Suppression de l'ancien (copie ou lien)
if (Test-Path $vst3Dest) {
    Write-Host "Suppression de l'ancien plugin..." -ForegroundColor Yellow

    # Verification si c'est deja un lien symbolique
    $item = Get-Item $vst3Dest
    if ($item.LinkType -eq "SymbolicLink") {
        Write-Host "  -> Ancien lien symbolique detecte (source: $($item.Target))" -ForegroundColor Cyan
    } else {
        Write-Host "  -> Copie physique detectee (pas un lien)" -ForegroundColor Cyan
    }

    Remove-Item -Path $vst3Dest -Recurse -Force
    Write-Host "  -> Suppression terminee" -ForegroundColor Green
}

# Creation du lien symbolique
Write-Host ""
Write-Host "Creation du lien symbolique..." -ForegroundColor Cyan
try {
    New-Item -ItemType SymbolicLink -Path $vst3Dest -Target $vst3Source -Force | Out-Null
    Write-Host "  -> Lien symbolique cree avec succes !" -ForegroundColor Green
} catch {
    Write-Host "ERREUR lors de la creation du lien !" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Yellow
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Verification
Write-Host ""
Write-Host "=== Verification ===" -ForegroundColor Cyan
$link = Get-Item $vst3Dest
if ($link.LinkType -eq "SymbolicLink") {
    Write-Host "Type          : Lien symbolique" -ForegroundColor Green
    Write-Host "Cible         : $($link.Target)" -ForegroundColor White
    Write-Host ""
    Write-Host "SUCCESS!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Le plugin OpenVoxTuner est maintenant lie a votre build $buildConfig." -ForegroundColor Cyan
    Write-Host "Chaque fois que vous recompilez dans Visual Studio, le plugin dans" -ForegroundColor Cyan
    Write-Host "Program Files sera automatiquement mis a jour !" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Important:" -ForegroundColor Yellow
    Write-Host "  1. Fermez completement votre DAW" -ForegroundColor White
    Write-Host "  2. Relancez-le pour rescanner les plugins" -ForegroundColor White
    Write-Host "  3. Chargez OpenVoxTuner" -ForegroundColor White
    Write-Host ""
    Write-Host "Logs disponibles dans: $env:UserProfile\Documents\OpenVoxTuner.log" -ForegroundColor Cyan
} else {
    Write-Host "AVERTISSEMENT: Le lien n'est pas de type SymbolicLink !" -ForegroundColor Yellow
    Write-Host "Type detecte: $($link.LinkType)" -ForegroundColor Yellow
}

Write-Host ""
Read-Host "Appuyez sur Entree pour quitter"
