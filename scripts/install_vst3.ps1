# Quick install script for the OpenVoxTuner VST3 (for development)
# Run this script in administrator mode (right-click > Run as administrator)
#
# OPTION 1 (RECOMMENDED): Use the Inno Setup installer
#   .\build_installer.ps1
#   Then run: .\build\installer\OpenVoxTuner_Windows_Installer.exe
#
# OPTION 2: Use this script for a quick install during development

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path $PSScriptRoot -Parent

# Configuration
$buildType = "Debug"  # Change to "Release" for the Release build
$vst3Source = "$repoRoot\build\OpenVoxTuner_artefacts\$buildType\VST3\OpenVoxTuner.vst3"
$vst3Dest = "$env:CommonProgramFiles\VST3\OpenVoxTuner.vst3"
$clapSource = "$repoRoot\build\OpenVoxTuner_artefacts\$buildType\CLAP\OpenVoxTuner.clap"
$clapDest = "$env:CommonProgramFiles\CLAP\OpenVoxTuner.clap"

Write-Host "=== Quick install of OpenVoxTuner ===" -ForegroundColor Cyan
Write-Host "Configuration: $buildType" -ForegroundColor Yellow
Write-Host "Source VST3: $vst3Source" -ForegroundColor Yellow
Write-Host "Dest VST3  : $vst3Dest" -ForegroundColor Yellow
Write-Host "Source CLAP: $clapSource" -ForegroundColor Yellow
Write-Host "Dest CLAP  : $clapDest" -ForegroundColor Yellow
Write-Host ""
Write-Host "NOTE: For a full installation (with installer), use instead:" -ForegroundColor Cyan
Write-Host "  .\build_installer.ps1" -ForegroundColor White
Write-Host "  .\build\installer\OpenVoxTuner_Windows_Installer.exe" -ForegroundColor White
Write-Host ""

# Admin rights check
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as administrator!" -ForegroundColor Red
    Write-Host "Right-click the file > Run as administrator" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Check that the VST3 source exists
if (-not (Test-Path $vst3Source)) {
    Write-Host "ERROR: The source VST3 does not exist: $vst3Source" -ForegroundColor Red
    Write-Host "Build the project first in Visual Studio ($buildType configuration)" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Remove the old VST3 if present
if (Test-Path $vst3Dest) {
    Write-Host "Removing old VST3 version..." -ForegroundColor Yellow
    Remove-Item -Path $vst3Dest -Recurse -Force
    Write-Host "Old VST3 version removed" -ForegroundColor Green
}

# Copy the new VST3 (full bundle with all subfolders)
Write-Host "Copying new VST3..." -ForegroundColor Yellow
Copy-Item -Path $vst3Source -Destination $vst3Dest -Recurse -Force

# Handle the CLAP format if available
if (Test-Path $clapSource) {
    if (Test-Path $clapDest) {
        Write-Host "Removing old CLAP version..." -ForegroundColor Yellow
        Remove-Item -Path $clapDest -Force
        Write-Host "Old CLAP version removed" -ForegroundColor Green
    }
    
    Write-Host "Copying new CLAP..." -ForegroundColor Yellow
    $clapDestDir = Split-Path -Path $clapDest -Parent
    if (-not (Test-Path $clapDestDir)) {
        New-Item -ItemType Directory -Path $clapDestDir -Force | Out-Null
    }
    Copy-Item -Path $clapSource -Destination $clapDest -Force
    Write-Host "CLAP installed in: $clapDest" -ForegroundColor Green
} else {
    Write-Host "CLAP not found (target not built for $buildType), skipping CLAP installation" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Installation completed successfully! ===" -ForegroundColor Green
Write-Host "The OpenVoxTuner VST3 plugin is installed in: $vst3Dest" -ForegroundColor Cyan
if (Test-Path $clapDest) {
    Write-Host "The OpenVoxTuner CLAP plugin is installed in: $clapDest" -ForegroundColor Cyan
}
Write-Host ""
Write-Host "Restart your DAW to load the new version." -ForegroundColor Yellow
Write-Host ""

Read-Host "Press Enter to exit"
