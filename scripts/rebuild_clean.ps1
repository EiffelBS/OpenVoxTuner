# rebuild_clean.ps1
# Completely cleans the build folder and regenerates the project
# Useful when CMake is broken or to start from scratch

param(
    [ValidateSet("Debug", "Release", "Both")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $ProjectRoot "build"

Write-Host "=== Clean and Full Rebuild ===" -ForegroundColor Cyan
Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
Write-Host ""

# 1. Complete removal of the build folder
if (Test-Path $BuildDir) {
    Write-Host "[1/3] Removing the build folder..." -ForegroundColor Cyan
    try {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Host "  -> Build folder removed" -ForegroundColor Green
    } catch {
        Write-Host "ERROR: Unable to remove the build folder" -ForegroundColor Red
        Write-Host "Close Visual Studio and all applications using the build files" -ForegroundColor Yellow
        Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Yellow
        Read-Host "Press Enter to exit"
        exit 1
    }
} else {
    Write-Host "[1/3] Build folder does not exist (already clean)" -ForegroundColor Green
}

Write-Host ""

# 2. Regenerate via CMake
Write-Host "[2/3] Generating project via CMake..." -ForegroundColor Cyan
try {
    & .\build.ps1 -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "CMake generation/build failed"
    }
    Write-Host "  -> Project built successfully" -ForegroundColor Green
} catch {
    Write-Host "ERROR during CMake generation/build!" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "=== Full Rebuild Completed! ===" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "  1. Open Visual Studio: build\OpenVoxTuner.sln" -ForegroundColor White
Write-Host "  2. Install the VST3: .\create_dev_symlink.ps1 (in Admin mode)" -ForegroundColor White
Write-Host "  3. Test in your DAW" -ForegroundColor White
Write-Host ""
