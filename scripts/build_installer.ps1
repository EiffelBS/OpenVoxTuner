param (
    [switch]$NoBuild = $false,
    [string]$JucePath = $env:JUCE_PATH
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
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

    # Remove old CMake cache to avoid generator conflicts
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }

    # Source the MSVC environment.
    . (Join-Path $PSScriptRoot "init_vs_env.ps1") 2>$null | Out-Null

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

    # Configure CMake
    & $cmake -S $ProjectRoot -B $BuildDir -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_BUILD_TYPE=Release `
        -DJUCE_PATH="$JucePath"
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed."
        exit $LASTEXITCODE
    }

    Write-Host "`n[2/2] Building Release targets..." -ForegroundColor Cyan
    
    # Run build in a cmd.exe context with the correct PATH for MSBuild custom commands.
    # MSBuild spawns child cmd.exe processes for custom build steps and they need
    # access to Windows system utilities like attrib.exe.
    $buildScript = Join-Path $PSScriptRoot "build_helper.cmd"
    $cmakeExe = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    
    # Les chemins VS/SDK proviennent de init_vs_env.ps1 (sourcé ci-dessus) et
    # sont resolus dynamiquement via vswhere : cela fonctionne pour toute edition
    # de VS 2022 (Community en local, Enterprise sur les runners CI).
    
    $msvcBin = "$vsToolsRoot\bin\Hostx64\x64"
    $sdkBin = "$sdkRoot\bin\$sdkVersion\x64"
    
    $buildContent = @"
@echo off
setlocal
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;$msvcBin;$sdkBin;$env:Path
set INCLUDE=$vsToolsRoot\include;$sdkRoot\Include\$sdkVersion\ucrt;$sdkRoot\Include\$sdkVersion\um;$sdkRoot\Include\$sdkVersion\shared;$sdkRoot\Include\$sdkVersion\winrt;$sdkRoot\Include\$sdkVersion\cppwinrt
set LIB=$vsToolsRoot\lib\onecore\x64;$vsToolsRoot\lib\x64;$sdkRoot\Lib\$sdkVersion\ucrt\x64;$sdkRoot\Lib\$sdkVersion\um\x64
set VCINSTALLDIR=$vsRoot\VC
set VCToolsInstallDir=$vsToolsRoot
set VCToolsVersion=$msvcVer
set WindowsSdkDir=$sdkRoot\
set WindowsSDKVersion=$sdkVersion\
set UniversalCRTSdkDir=$sdkRoot\
set UCRTVersion=$sdkVersion
set VSINSTALLDIR=$vsRoot
set DevEnvDir=$vsRoot\Common7\IDE
set VS170COMNTOOLS=$vsRoot\Common7\Tools\
set COMSPEC=%SystemRoot%\system32\cmd.exe
set PATHEXT=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC
"$cmakeExe" --build $BuildDir --config Release --target OpenVoxTuner_VST3 OpenVoxTuner_Standalone
exit /b %errorlevel%
"@
    $buildContent | Out-File -Encoding ASCII $buildScript
    
    cmd.exe /c $buildScript
    $buildLASTEXITCODE = $LASTEXITCODE
    
    if ($buildLASTEXITCODE -ne 0) {
        Write-Error "Build failed! Aborting installer generation."
        exit $buildLASTEXITCODE
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
Push-Location $ProjectRoot
& $iscc "installer\OpenVoxTuner.iss"
Pop-Location

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nSUCCESS! Installer generated at:" -ForegroundColor Green
    Write-Host " -> $(Resolve-Path (Join-Path $BuildDir 'installer\OpenVoxTuner_Windows_Installer.exe'))" -ForegroundColor White
} else {
    Write-Error "Failed to generate installer. ISCC exited with code $LASTEXITCODE"
    exit $LASTEXITCODE
}
