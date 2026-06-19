# copy_vst3_to_program_files.ps1
# Copie le VST3 Debug vers Program Files (necessite admin)

$ErrorActionPreference = "Stop"

Write-Host "=== Installation VST3 Debug ===" -ForegroundColor Cyan
Write-Host ""

# Verification admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERREUR: Ce script doit etre execute en mode Administrateur !" -ForegroundColor Red
    Write-Host ""
    Write-Host "Relancez PowerShell en mode Admin, puis executez:" -ForegroundColor Yellow
    Write-Host "  cd C:\Users\User\Documents\trae_projects\VST3\build" -ForegroundColor White
    Write-Host "  .\copy_vst3_to_program_files.ps1" -ForegroundColor White
    Write-Host ""
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

$source = "C:\Users\User\Documents\trae_projects\VST3\build\OpenVoxTuner_artefacts\Debug\VST3\OpenVoxTuner.vst3"
$dest = "$env:CommonProgramFiles\VST3\OpenVoxTuner.vst3"

Write-Host "Source: $source" -ForegroundColor Yellow
Write-Host "Dest  : $dest" -ForegroundColor Yellow
Write-Host ""

if (-not (Test-Path $source)) {
    Write-Host "ERREUR: VST3 source introuvable !" -ForegroundColor Red
    Write-Host "Compilez d'abord en Debug dans Visual Studio" -ForegroundColor Yellow
    Read-Host "Appuyez sur Entree pour quitter"
    exit 1
}

Write-Host "Suppression de l'ancienne version..." -ForegroundColor Cyan
if (Test-Path $dest) {
    Remove-Item -Path $dest -Recurse -Force
    Write-Host "  -> Supprimee" -ForegroundColor Green
}

Write-Host "Copie de la nouvelle version..." -ForegroundColor Cyan
Copy-Item -Path $source -Destination $dest -Recurse -Force
Write-Host "  -> Copiee" -ForegroundColor Green

$installedDll = "$dest\Contents\x86_64-win\OpenVoxTuner.vst3"
$info = Get-Item $installedDll
Write-Host ""
Write-Host "DLL installee :" -ForegroundColor Green
Write-Host "  Date  : $($info.LastWriteTime)" -ForegroundColor White
Write-Host "  Taille: $($info.Length) bytes" -ForegroundColor White
Write-Host ""
Write-Host "SUCCESS! Plugin installe." -ForegroundColor Green
Write-Host ""
Write-Host "Prochaines etapes :" -ForegroundColor Cyan
Write-Host "  1. Fermez COMPLETEMENT votre DAW" -ForegroundColor White
Write-Host "  2. Relancez-le" -ForegroundColor White
Write-Host "  3. Chargez OpenVoxTuner" -ForegroundColor White
Write-Host "  4. Testez avec audio vocal" -ForegroundColor White
Write-Host ""
Write-Host "Logs disponibles dans: E:\Documents\OpenVoxTuner.log" -ForegroundColor Cyan
Write-Host ""

Read-Host "Appuyez sur Entree pour quitter"
