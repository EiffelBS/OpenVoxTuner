# create_dev_symlink.ps1
# Creates a symbolic link to the development VST3
# Benefit: every Visual Studio rebuild automatically updates the plugin in Program Files
# MUST be run in Administrator mode

$ErrorActionPreference = "Stop"

# Configuration: change "Debug" to "Release" as needed
$buildConfig = "Debug"

$repoRoot = Split-Path $PSScriptRoot -Parent

$vst3Source = "$repoRoot\build\OpenVoxTuner_artefacts\$buildConfig\VST3\OpenVoxTuner.vst3"
$vst3Dest = "$env:CommonProgramFiles\VST3\OpenVoxTuner.vst3"

Write-Host "=== Creating a symbolic link for development ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Configuration: $buildConfig" -ForegroundColor Yellow
Write-Host "Source        : $vst3Source" -ForegroundColor Yellow
Write-Host "Destination   : $vst3Dest" -ForegroundColor Yellow
Write-Host ""

# Admin rights check
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as administrator!" -ForegroundColor Red
    Write-Host "Right-click the file > Run as administrator" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# Check that the source exists
if (-not (Test-Path $vst3Source)) {
    Write-Host "ERROR: The source VST3 does not exist!" -ForegroundColor Red
    Write-Host "Path: $vst3Source" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Build the project first in '$buildConfig' configuration inside Visual Studio." -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# Remove the old one (copy or link)
if (Test-Path $vst3Dest) {
    Write-Host "Removing old plugin..." -ForegroundColor Yellow

    # Check whether it is already a symbolic link
    $item = Get-Item $vst3Dest
    if ($item.LinkType -eq "SymbolicLink") {
        Write-Host "  -> Old symbolic link detected (source: $($item.Target))" -ForegroundColor Cyan
    } else {
        Write-Host "  -> Physical copy detected (not a link)" -ForegroundColor Cyan
    }

    Remove-Item -Path $vst3Dest -Recurse -Force
    Write-Host "  -> Removal done" -ForegroundColor Green
}

# Create the symbolic link
Write-Host ""
Write-Host "Creating symbolic link..." -ForegroundColor Cyan
try {
    New-Item -ItemType SymbolicLink -Path $vst3Dest -Target $vst3Source -Force | Out-Null
    Write-Host "  -> Symbolic link created successfully!" -ForegroundColor Green
} catch {
    Write-Host "ERROR while creating the link!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Verification
Write-Host ""
Write-Host "=== Verification ===" -ForegroundColor Cyan
$link = Get-Item $vst3Dest
if ($link.LinkType -eq "SymbolicLink") {
    Write-Host "Type          : Symbolic link" -ForegroundColor Green
    Write-Host "Target        : $($link.Target)" -ForegroundColor White
    Write-Host ""
    Write-Host "SUCCESS!" -ForegroundColor Green
    Write-Host ""
    Write-Host "The OpenVoxTuner plugin is now linked to your $buildConfig build." -ForegroundColor Cyan
    Write-Host "Every time you rebuild in Visual Studio, the plugin in" -ForegroundColor Cyan
    Write-Host "Program Files is automatically updated!" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Important:" -ForegroundColor Yellow
    Write-Host "  1. Fully close your DAW" -ForegroundColor White
    Write-Host "  2. Restart it so it rescans the plugins" -ForegroundColor White
    Write-Host "  3. Load OpenVoxTuner" -ForegroundColor White
    Write-Host ""
    Write-Host "Logs available at: $env:UserProfile\Documents\OpenVoxTuner.log" -ForegroundColor Cyan
} else {
    Write-Host "WARNING: The link is not of type SymbolicLink!" -ForegroundColor Yellow
    Write-Host "Detected type: $($link.LinkType)" -ForegroundColor Yellow
}

Write-Host ""
Read-Host "Press Enter to exit"
