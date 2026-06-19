# open_vs.ps1
# Ouvre Visual Studio avec la solution OpenVoxTuner
# Lance depuis n'importe quel dossier du projet

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$SolutionPath = Join-Path $ProjectRoot "build\OpenVoxTuner.sln"

Write-Host "=== Ouverture de Visual Studio ===" -ForegroundColor Cyan

# Verification de l'existence de la solution
if (-not (Test-Path $SolutionPath)) {
    Write-Host "ERREUR: La solution Visual Studio n'existe pas encore !" -ForegroundColor Red
    Write-Host "Path attendu: $SolutionPath" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Generez d'abord le projet avec :" -ForegroundColor Yellow
    Write-Host "  .\build.ps1" -ForegroundColor White
    Write-Host "OU" -ForegroundColor Yellow
    Write-Host "  .\rebuild_clean.ps1" -ForegroundColor White
    Write-Host ""
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Recherche de Visual Studio
$vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
$vsPathPro = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe"
$vsPathEnterprise = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.exe"

$vsExe = $null
if (Test-Path $vsPath) {
    $vsExe = $vsPath
} elseif (Test-Path $vsPathPro) {
    $vsExe = $vsPathPro
} elseif (Test-Path $vsPathEnterprise) {
    $vsExe = $vsPathEnterprise
}

if (-not $vsExe) {
    Write-Host "ERREUR: Visual Studio 2022 introuvable !" -ForegroundColor Red
    Write-Host "Installez Visual Studio 2022 Community/Professional/Enterprise" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

# Ouverture de Visual Studio
Write-Host "Ouverture de : $SolutionPath" -ForegroundColor Yellow
Write-Host "Avec : $vsExe" -ForegroundColor Yellow
Write-Host ""

Start-Process -FilePath $vsExe -ArgumentList "`"$SolutionPath`""

Write-Host "Visual Studio lance !" -ForegroundColor Green
