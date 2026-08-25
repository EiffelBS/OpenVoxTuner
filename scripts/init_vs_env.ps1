# ============================================================================
# init_vs_env.ps1
# Initializes the MSVC + Windows SDK environment in pure PowerShell
# (without cmd.exe, which is blocked in some environments).
# Source it with:  . .\init_vs_env.ps1
# ============================================================================

$ErrorActionPreference = 'Stop'

# --- Dynamic location of Visual Studio 2022 (edition-agnostic) ---
# vswhere is installed with VS (Community/Professional/Enterprise) and returns
# the actual installation path regardless of the edition. This lets the same
# script work both on a local machine (VS Community) and on a CI runner
# (VS Enterprise).
function Get-VS2022InstallPath {
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($path) { return $path.Trim() }
    return $null
}

# Default path (fallback if vswhere is unavailable, e.g. local VS Community)
$vsRoot = Get-VS2022InstallPath
if (-not $vsRoot) {
    $vsRoot = "C:\Program Files\Microsoft Visual Studio\2022\Community"
}

# MSVC: pick the newest installed version (robust across editions)
$msvcBase = Join-Path $vsRoot "VC\Tools\MSVC"
$msvcVer  = (Get-ChildItem $msvcBase -Directory -ErrorAction SilentlyContinue |
             Sort-Object Name | Select-Object -Last 1).Name
if (-not $msvcVer) { $msvcVer = "14.44.35207" }   # fallback
$vsToolsRoot = Join-Path $msvcBase $msvcVer

# Windows SDK: pick the newest installed 10.0.* version
$sdkRoot = "C:\Program Files (x86)\Windows Kits\10"
$sdkVersion = (Get-ChildItem "$sdkRoot\Include" -Directory -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -like "10.0.*" } |
               Sort-Object Name | Select-Object -Last 1).Name
if (-not $sdkVersion) { $sdkVersion = "10.0.19041.0" }  # fallback

if (-not (Test-Path $vsToolsRoot))
{
    Write-Error "MSVC not found: $vsToolsRoot"
    return
}
if (-not (Test-Path $sdkRoot))
{
    Write-Error "Windows SDK not found: $sdkRoot"
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
# Note: in some installations, MSVC only has the "onecore" subfolder.
# We include both to be robust across both configurations.
$env:LIB = @(
    "$vsToolsRoot\lib\onecore\x64"
    "$vsToolsRoot\lib\x64"
    "$sdkRoot\Lib\$sdkVersion\ucrt\x64"
    "$sdkRoot\Lib\$sdkVersion\um\x64"
) -join ";"

# === LIBPATH (C++/CLI-specific linkers) ===
$env:LIBPATH = @(
    "$vsToolsRoot\lib\onecore\x64"
    "$vsToolsRoot\lib\x64"
    "$sdkRoot\Lib\$sdkVersion\um\x64"
) -join ";"

# === Standard MSVC variables ===
$env:VCINSTALLDIR = "$vsRoot\VC"
$env:VCToolsInstallDir = "$vsRoot\VC\Tools\MSVC\$msvcVer"
$env:VCToolsVersion = $msvcVer
$env:WindowsSdkDir = "$sdkRoot\"
$env:WindowsSDKVersion = "$sdkVersion\"
$env:UniversalCRTSdkDir = "$sdkRoot\"
$env:UCRTVersion = $sdkVersion
$env:VSINSTALLDIR = $vsRoot
$env:DevEnvDir = "$vsRoot\Common7\IDE"
$env:VS170COMNTOOLS = "$vsRoot\Common7\Tools\"

# === CMake in PATH (fallback, usually already present) ===
if (Test-Path "C:\Program Files\CMake\bin") {
    $env:Path = "C:\Program Files\CMake\bin;$env:Path"
}

# === Checks ===
$cl = (Get-Command cl.exe -ErrorAction SilentlyContinue).Source
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source

Write-Host "MSVC environment initialized:" -ForegroundColor Green
Write-Host "  VS edition root : $vsRoot"
Write-Host "  MSVC version   : $env:VCToolsVersion"
Write-Host "  Windows SDK    : $env:WindowsSDKVersion"
Write-Host "  cl.exe         : $cl"
Write-Host "  cmake          : $cmake"
Write-Host "  ninja          : $ninja"
