# open_vs.ps1
# Opens Visual Studio with the OpenVoxTuner solution
# Can be launched from any project folder

$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path $PSScriptRoot -Parent
$SolutionPath = Join-Path $ProjectRoot "build\OpenVoxTuner.sln"

Write-Host "=== Opening Visual Studio ===" -ForegroundColor Cyan

# Check that the solution exists
if (-not (Test-Path $SolutionPath)) {
    Write-Host "ERROR: The Visual Studio solution does not exist yet!" -ForegroundColor Red
    Write-Host "Expected path: $SolutionPath" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Generate the project first with:" -ForegroundColor Yellow
    Write-Host "  .\build.ps1" -ForegroundColor White
    Write-Host "OR" -ForegroundColor Yellow
    Write-Host "  .\rebuild_clean.ps1" -ForegroundColor White
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# Locate Visual Studio
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
    Write-Host "ERROR: Visual Studio 2022 not found!" -ForegroundColor Red
    Write-Host "Install Visual Studio 2022 Community/Professional/Enterprise" -ForegroundColor Yellow
    Write-Host ""
    Read-Host "Press Enter to exit"
    exit 1
}

# Open Visual Studio
Write-Host "Opening: $SolutionPath" -ForegroundColor Yellow
Write-Host "With: $vsExe" -ForegroundColor Yellow
Write-Host ""

Start-Process -FilePath $vsExe -ArgumentList "`"$SolutionPath`""

Write-Host "Visual Studio launched!" -ForegroundColor Green
