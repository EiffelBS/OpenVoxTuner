# ============================================================================
# init_vs_env.ps1
# Initialise l'environnement MSVC + Windows SDK en PowerShell pur
# (sans cmd.exe, qui est bloque dans certains environnements).
# A sourcer avec :  . .\init_vs_env.ps1
# ============================================================================

$vsToolsRoot = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207"
$sdkRoot     = "C:\Program Files (x86)\Windows Kits\10"
$sdkVersion  = "10.0.19041.0"
$vsRoot      = "C:\Program Files\Microsoft Visual Studio\2022\Community"

if (-not (Test-Path $vsToolsRoot))
{
    Write-Error "MSVC introuvable : $vsToolsRoot"
    return
}
if (-not (Test-Path $sdkRoot))
{
    Write-Error "Windows SDK introuvable : $sdkRoot"
    return
}

# === PATH ===
$msvcBin = "$vsToolsRoot\bin\Hostx64\x64"
$msvcBinArm = "$vsToolsRoot\bin\Hostx64\arm64"  # cross-compile arm64
$sdkBin = "$sdkRoot\bin\$sdkVersion\x64"

$env:Path = "$msvcBin;$sdkBin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;$env:Path"

# === INCLUDE ===
$env:INCLUDE = @(
    "$vsToolsRoot\include"
    "$sdkRoot\Include\$sdkVersion\ucrt"
    "$sdkRoot\Include\$sdkVersion\um"
    "$sdkRoot\Include\$sdkVersion\shared"
    "$sdkRoot\Include\$sdkVersion\winrt"
    "$sdkRoot\Include\$sdkVersion\cppwinrt"
) -join ";"

# === LIB ===
# Note : dans cette installation, MSVC a seulement le sous-dossier "onecore".
# En installation standard, ce serait $vsToolsRoot\lib\x64. On inclut les deux
# pour etre robuste aux deux configurations.
$env:LIB = @(
    "$vsToolsRoot\lib\onecore\x64"
    "$vsToolsRoot\lib\x64"
    "$sdkRoot\Lib\$sdkVersion\ucrt\x64"
    "$sdkRoot\Lib\$sdkVersion\um\x64"
) -join ";"

# === LIBPATH (lieurs specifiques C++/CLI) ===
$env:LIBPATH = @(
    "$vsToolsRoot\lib\onecore\x64"
    "$vsToolsRoot\lib\x64"
    "$sdkRoot\Lib\$sdkVersion\um\x64"
) -join ";"

# === Variables standards MSVC ===
$env:VCINSTALLDIR = "$vsRoot\VC"
$env:VCToolsInstallDir = "$vsRoot\VC\Tools\MSVC\14.44.35207"
$env:VCToolsVersion = "14.44.35207"
$env:WindowsSdkDir = "$sdkRoot\"
$env:WindowsSDKVersion = "$sdkVersion\"
$env:UniversalCRTSdkDir = "$sdkRoot\"
$env:UCRTVersion = $sdkVersion
$env:VSINSTALLDIR = $vsRoot
$env:DevEnvDir = "$vsRoot\Common7\IDE"
$env:VS170COMNTOOLS = "$vsRoot\Common7\Tools\"

# === CMake dans le PATH ===
$env:Path = "C:\Program Files\CMake\bin;$env:Path"

# === Verifications ===
$cl = (Get-Command cl.exe -ErrorAction SilentlyContinue).Source
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source

Write-Host "Environnement MSVC initialise :" -ForegroundColor Green
Write-Host "  MSVC version   : $env:VCToolsVersion"
Write-Host "  Windows SDK    : $env:WindowsSDKVersion"
Write-Host "  cl.exe         : $cl"
Write-Host "  cmake          : $cmake"
Write-Host "  ninja          : $ninja"
