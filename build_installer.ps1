param (
    [switch]$NoBuild = $false,
    [string]$JucePath = $env:JUCE_PATH
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot "build"
if ([string]::IsNullOrWhiteSpace($JucePath)) {
    $JucePath = "C:\JUCE"
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " OpenVoxTuner - Windows Installer Build" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# 1. Check for Inno Setup compiler
$isccPath = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
$isccPathLocal = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"

if (Test-Path $isccPath) {
    $iscc = $isccPath
} elseif (Test-Path $isccPathLocal) {
    $iscc = $isccPathLocal
} else {
    Write-Host "Inno Setup 6 compiler (ISCC.exe) not found." -ForegroundColor Yellow
    Write-Host "Attempting to install Inno Setup via winget..." -ForegroundColor Cyan

    winget install -e --id JRSoftware.InnoSetup --accept-package-agreements --accept-source-agreements

    if (Test-Path $isccPath) {
        $iscc = $isccPath
    } elseif (Test-Path $isccPathLocal) {
        $iscc = $isccPathLocal
    } else {
        Write-Error "Failed to install Inno Setup. Please install it manually from https://jrsoftware.org/isdl.php"
        exit 1
    }
    Write-Host "Inno Setup installed successfully." -ForegroundColor Green
}

# 2. Build the project in Release mode using CMake directly.
if (-not $NoBuild) {
    Write-Host "`n[1/2] Configuring project with CMake..." -ForegroundColor Cyan

    # Source the MSVC environment.
    . (Join-Path $ProjectRoot "init_vs_env.ps1") 2>$null | Out-Null
# Ensure Windows system utilities are available for MSBuild custom commands
$env:Path = "C:\Windows\System32;$env:Path"
$Env:COMSPEC = "$env:SystemRoot\\system32\\cmd.exe"

    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        Write-Host "cl.exe introuvable meme apres init_vs_env.ps1" -ForegroundColor Red
        exit 1
    }

    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    if (-not $cmake) {
        Write-Host "CMake introuvable." -ForegroundColor Red
        exit 1
    }

    if (-not (Test-Path $JucePath)) {
        Write-Host "JUCE introuvable dans $JucePath" -ForegroundColor Red
        exit 1
    }

    & $cmake -S $ProjectRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_BUILD_TYPE=Release `
        -DJUCE_PATH="$JucePath"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed."
        exit $LASTEXITCODE
    }

    Write-Host "`n[2/2] Building Release targets..." -ForegroundColor Cyan
    & $cmake --build $BuildDir --config Release --target OpenVoxTuner_VST3 OpenVoxTuner_Standalone
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed! Aborting installer generation."
        exit $LASTEXITCODE
    }
} else {
    Write-Host "`n[1/2] Skipping build step (-NoBuild specified)..." -ForegroundColor Yellow
}

# 3. Verify that the build artifacts exist
$vst3Path = Join-Path $BuildDir "OpenVoxTuner_artefacts\Release\VST3\OpenVoxTuner.vst3"
$standalonePath = Join-Path $BuildDir "OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe"

if (-not (Test-Path $vst3Path)) {
    Write-Error "VST3 build artifact not found at: $vst3Path"
    exit 1
}

if (-not (Test-Path $standalonePath)) {
    Write-Error "Standalone build artifact not found at: $standalonePath"
    exit 1
}

# 4. Generate the installer using Inno Setup
Write-Host "`n[3/3] Generating Windows Installer (.exe)..." -ForegroundColor Cyan
& $iscc "installer\OpenVoxTuner.iss"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSUCCESS! Installer generated at:" -ForegroundColor Green
    Write-Host " -> $(Resolve-Path (Join-Path $BuildDir 'installer\OpenVoxTuner_Windows_Installer.exe'))" -ForegroundColor White
} else {
    Write-Error "Failed to generate installer. ISCC exited with code $LASTEXITCODE"
    exit $LASTEXITCODE
}
