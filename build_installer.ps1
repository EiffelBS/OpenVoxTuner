param (
    [switch]$NoBuild = $false
)

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

# 2. Build the project in Release mode
if (-Not $NoBuild) {
    Write-Host "`n[1/2] Building project in Release mode..." -ForegroundColor Cyan
    .\build.ps1
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed! Aborting installer generation."
        exit $LASTEXITCODE
    }
} else {
    Write-Host "`n[1/2] Skipping build step (-NoBuild specified)..." -ForegroundColor Yellow
}

# 3. Verify that the build artifacts exist
$vst3Path = "build\OpenVoxTuner_artefacts\Release\VST3\OpenVoxTuner.vst3"
$standalonePath = "build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe"

if (-Not (Test-Path $vst3Path)) {
    Write-Error "VST3 build artifact not found at: $vst3Path"
    exit 1
}

if (-Not (Test-Path $standalonePath)) {
    Write-Error "Standalone build artifact not found at: $standalonePath"
    exit 1
}

# 4. Generate the installer using Inno Setup
Write-Host "`n[2/2] Generating Windows Installer (.exe)..." -ForegroundColor Cyan
& $iscc "installer\OpenVoxTuner.iss"

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSUCCESS! Installer generated at:" -ForegroundColor Green
    Write-Host " -> $(Resolve-Path 'build\installer\OpenVoxTuner_Windows_Installer.exe')" -ForegroundColor White
} else {
    Write-Error "Failed to generate installer. ISCC exited with code $LASTEXITCODE"
    exit $LASTEXITCODE
}
