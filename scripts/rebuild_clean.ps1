# rebuild_clean.ps1
# Nettoie completement le dossier build et regenere le projet
# Utile quand CMake est perdu ou pour repartir de zero

param(
    [ValidateSet("Debug", "Release", "Both")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host "=== Nettoyage et Rebuild Complet ===" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
Write-Host ""

# 1. Suppression complete du dossier build
if (Test-Path $BuildDir) {
    Write-Host "[1/3] Suppression du dossier build..." -ForegroundColor Cyan
    try {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Host "  -> Dossier build supprime" -ForegroundColor Green
    } catch {
        Write-Host "ERREUR: Impossible de supprimer le dossier build" -ForegroundColor Red
        Write-Host "Fermez Visual Studio et toutes les applications utilisant les fichiers du build" -ForegroundColor Yellow
        Write-Host "Erreur: $($_.Exception.Message)" -ForegroundColor Yellow
        Read-Host "Appuyez sur Entree pour quitter"
        exit 1
    }
} else {
    Write-Host "[1/3] Dossier build n'existe pas (deja propre)" -ForegroundColor Green
}

Write-Host ""

# 2. Re-generation via CMake
Write-Host "[2/3] Generation du projet via CMake..." -ForegroundColor Cyan
try {
    & .\build.ps1 -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Echec de la generation/compilation CMake"
    }
    Write-Host "  -> Projet compile avec succes" -ForegroundColor Green
} catch {
    Write-Host "ERREUR lors de la generation/compilation CMake !" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Yellow
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

Write-Host ""
Write-Host "=== Rebuild Complet Termine ! ===" -ForegroundColor Green
Write-Host ""
Write-Host "Prochaines etapes :" -ForegroundColor Cyan
Write-Host "  1. Ouvrir Visual Studio : build\OpenVoxTuner.sln" -ForegroundColor White
Write-Host "  2. Installer le VST3 : .\create_dev_symlink.ps1 (en mode Admin)" -ForegroundColor White
Write-Host "  3. Tester dans votre DAW" -ForegroundColor White
Write-Host ""
