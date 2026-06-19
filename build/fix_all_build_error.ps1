# fix_all_build_error.ps1
# Affiche la solution rapide pour l'erreur "Impossible de demarrer ALL_BUILD"

$ErrorActionPreference = "Stop"

Clear-Host

Write-Host ""
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  SOLUTION : Erreur 'Impossible de demarrer ALL_BUILD'" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

Write-Host "PROBLEME :" -ForegroundColor Red
Write-Host "  Visual Studio essaie de lancer 'ALL_BUILD' qui n'est pas un executable." -ForegroundColor White
Write-Host ""

Write-Host "SOLUTION (2 etapes) :" -ForegroundColor Green
Write-Host ""
Write-Host "  1. Dans l'Explorateur de Solutions de Visual Studio :" -ForegroundColor Cyan
Write-Host "     - Trouvez le projet : " -NoNewline -ForegroundColor White
Write-Host "OpenVoxTuner_Standalone" -ForegroundColor Yellow
Write-Host "     - Clic droit > 'Definir comme projet de demarrage'" -ForegroundColor White
Write-Host "     - Le projet devient " -NoNewline -ForegroundColor White
Write-Host "GRAS" -ForegroundColor Yellow
Write-Host ""
Write-Host "  2. Lancez le debogueur :" -ForegroundColor Cyan
Write-Host "     - Appuyez sur " -NoNewline -ForegroundColor White
Write-Host "F5" -ForegroundColor Yellow -NoNewline
Write-Host " OU" -ForegroundColor White
Write-Host "     - Menu : Debug > Start Debugging" -ForegroundColor White
Write-Host ""

Write-Host "ALTERNATIVE (sans changer le projet de demarrage) :" -ForegroundColor Green
Write-Host "  - Clic droit sur OpenVoxTuner_Standalone" -ForegroundColor White
Write-Host "  - Debug > Start New Instance" -ForegroundColor White
Write-Host ""

Write-Host "════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

Write-Host "Projets disponibles dans la solution :" -ForegroundColor Cyan
Write-Host ""
Write-Host "  [EXECUTABLE]  OpenVoxTuner_Standalone   <- Utilisez celui-ci !" -ForegroundColor Green
Write-Host "  [EXECUTABLE]  OpenVoxTunerTests" -ForegroundColor White
Write-Host "  [DLL]         OpenVoxTuner_VST3         (ne peut pas etre lance seul)" -ForegroundColor DarkGray
Write-Host "  [UTILITY]     ALL_BUILD                 (compile tout, pas un executable)" -ForegroundColor DarkGray
Write-Host "  [UTILITY]     ZERO_CHECK                (verification CMake)" -ForegroundColor DarkGray
Write-Host ""

Write-Host "Pour plus de details, consultez : " -ForegroundColor Cyan -NoNewline
Write-Host "QUICKFIX_ALL_BUILD_ERROR.md" -ForegroundColor Yellow
Write-Host "ou" -ForegroundColor Cyan -NoNewline
Write-Host " DEBUG_GUIDE.md" -ForegroundColor Yellow
Write-Host ""

$openGuide = Read-Host "Voulez-vous ouvrir le guide complet de debogage ? (O/N)"
if ($openGuide -eq "O" -or $openGuide -eq "o") {
    $guidePath = Join-Path $PSScriptRoot "..\DEBUG_GUIDE.md"
    if (Test-Path $guidePath) {
        Start-Process $guidePath
    } else {
        Write-Host "Guide introuvable : $guidePath" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Appuyez sur une touche pour continuer..." -ForegroundColor DarkGray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
