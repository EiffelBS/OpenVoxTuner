# set_startup_project.ps1
# Configure le projet de demarrage pour Visual Studio
# Doit etre execute AVANT d'ouvrir la solution dans Visual Studio

param(
    [ValidateSet("Standalone", "Tests")]
    [string]$ProjectType = "Standalone"
)

$ErrorActionPreference = "Stop"

$BuildDir = $PSScriptRoot
$SolutionPath = Join-Path $BuildDir "OpenVoxTuner.sln"
$SuoPath = Join-Path $BuildDir ".vs\OpenVoxTuner\v17\.suo"

if (-not (Test-Path $SolutionPath)) {
    Write-Host "ERREUR: Solution introuvable : $SolutionPath" -ForegroundColor Red
    exit 1
}

$projectName = if ($ProjectType -eq "Standalone") { "OpenVoxTuner_Standalone" } else { "OpenVoxTunerTests" }

Write-Host "=== Configuration du projet de demarrage ===" -ForegroundColor Cyan
Write-Host "Projet de demarrage : $projectName" -ForegroundColor Yellow
Write-Host ""

# Visual Studio stocke le projet de demarrage dans le fichier .suo (binaire)
# On ne peut pas le modifier facilement par script.
# La meilleure solution est de le faire manuellement dans Visual Studio.

Write-Host "Instructions pour configurer le projet de demarrage dans Visual Studio :" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Ouvrez la solution : OpenVoxTuner.sln" -ForegroundColor White
Write-Host ""
Write-Host "2. Dans l'Explorateur de solutions :" -ForegroundColor White
Write-Host "   - Trouvez le projet : $projectName" -ForegroundColor Yellow
Write-Host "   - Clic droit sur le projet" -ForegroundColor Yellow
Write-Host "   - Selectionnez : 'Definir comme projet de demarrage'" -ForegroundColor Yellow
Write-Host ""
Write-Host "3. Le projet $projectName devrait apparaitre en GRAS" -ForegroundColor White
Write-Host ""
Write-Host "4. Lancez le debogueur : F5 ou Debug > Start Debugging" -ForegroundColor White
Write-Host ""
Write-Host "Alternative rapide :" -ForegroundColor Cyan
Write-Host "  - Clic droit sur $projectName > Debug > Start New Instance" -ForegroundColor Yellow
Write-Host ""

# Ouverture automatique de Visual Studio
$openVS = Read-Host "Voulez-vous ouvrir Visual Studio maintenant ? (O/N)"
if ($openVS -eq "O" -or $openVS -eq "o") {
    $vsPath = "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe"
    if (Test-Path $vsPath) {
        Write-Host "Ouverture de Visual Studio..." -ForegroundColor Green
        Start-Process -FilePath $vsPath -ArgumentList "`"$SolutionPath`""
    } else {
        Write-Host "Visual Studio Community 2022 introuvable" -ForegroundColor Yellow
        Write-Host "Ouvrez manuellement : $SolutionPath" -ForegroundColor Yellow
    }
}
