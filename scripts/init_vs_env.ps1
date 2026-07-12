# ============================================================================
# init_vs_env.ps1
# Initialise l'environnement MSVC + Windows SDK en PowerShell pur
# (sans cmd.exe, qui est bloque dans certains environnements).
# A sourcer avec :  . .\init_vs_env.ps1
# ============================================================================

$ErrorActionPreference = 'Stop'

# --- Localisation dynamique de Visual Studio 2022 (edition-agnostique) ---
# vswhere est installe avec VS (Community/Professional/Enterprise) et renvoie
# le chemin reel d'installation, peu importe l'edition. Cela permet au meme
# script de fonctionner aussi bien sur une machine locale (VS Community) que
# sur un runner CI (VS Enterprise).
function Get-VS2022InstallPath {
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    $path = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($path) { return $path.Trim() }
    return $null
}

# Chemin par defaut (fallback si vswhere indisponible, ex. VS Community local)
$vsRoot = Get-VS2022InstallPath
if (-not $vsRoot) {
    $vsRoot = "C:\Program Files\Microsoft Visual Studio\2022\Community"
}

# MSVC : on prend la version installee la plus recente (robuste entre editions)
$msvcBase = Join-Path $vsRoot "VC\Tools\MSVC"
$msvcVer  = (Get-ChildItem $msvcBase -Directory -ErrorAction SilentlyContinue |
             Sort-Object Name | Select-Object -Last 1).Name
if (-not $msvcVer) { $msvcVer = "14.44.35207" }   # fallback
$vsToolsRoot = Join-Path $msvcBase $msvcVer

# Windows SDK : on prend la version 10.0.* la plus recente installee
$sdkRoot = "C:\Program Files (x86)\Windows Kits\10"
$sdkVersion = (Get-ChildItem "$sdkRoot\Include" -Directory -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -like "10.0.*" } |
               Sort-Object Name | Select-Object -Last 1).Name
if (-not $sdkVersion) { $sdkVersion = "10.0.19041.0" }  # fallback

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
# Note : dans certaines installations, MSVC n'a que le sous-dossier "onecore".
# On inclut les deux pour etre robuste aux deux configurations.
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
$env:VCToolsInstallDir = "$vsRoot\VC\Tools\MSVC\$msvcVer"
$env:VCToolsVersion = $msvcVer
$env:WindowsSdkDir = "$sdkRoot\"
$env:WindowsSDKVersion = "$sdkVersion\"
$env:UniversalCRTSdkDir = "$sdkRoot\"
$env:UCRTVersion = $sdkVersion
$env:VSINSTALLDIR = $vsRoot
$env:DevEnvDir = "$vsRoot\Common7\IDE"
$env:VS170COMNTOOLS = "$vsRoot\Common7\Tools\"

# === CMake dans le PATH (fallback, generalement deja present) ===
if (Test-Path "C:\Program Files\CMake\bin") {
    $env:Path = "C:\Program Files\CMake\bin;$env:Path"
}

# === Verifications ===
$cl = (Get-Command cl.exe -ErrorAction SilentlyContinue).Source
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source

Write-Host "Environnement MSVC initialise :" -ForegroundColor Green
Write-Host "  VS edition root : $vsRoot"
Write-Host "  MSVC version   : $env:VCToolsVersion"
Write-Host "  Windows SDK    : $env:WindowsSDKVersion"
Write-Host "  cl.exe         : $cl"
Write-Host "  cmake          : $cmake"
Write-Host "  ninja          : $ninja"
