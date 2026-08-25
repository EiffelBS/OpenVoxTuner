# ============================================================================
# build.ps1
# Configures and builds the OpenVoxTuner plugin via CMake + MSVC.
# Does not require Projucer.
#
# Prerequisites:
#   - JUCE 8 in C:\JUCE
#   - Visual Studio 2022 (MSVC v143)
#   - Windows 10 SDK
#   - CMake >= 3.22 (installed automatically via winget if missing)
#   - VST3 SDK (optional, declared in the .jucer / CMake)
#
# Usage:
#   . .\init_vs_env.ps1   # OR let build.ps1 do it automatically
#   .\build.ps1
#
#   .\build.ps1 -Configuration Debug
#   .\build.ps1 -SkipConfigure
#   .\build.ps1 -Generator NMake   # or Ninja (may hang in some environments)
# ============================================================================

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [ValidateSet("NMake", "Ninja", "NMakeMakefiles", "NMakeMulti", "VS2022")]
    [string]$Generator = "VS2022",

    [switch]$SkipConfigure,

    [switch]$RunTests
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $ProjectRoot "build"
$JucePath = "C:\JUCE"

# === Source the MSVC environment (idempotent) ===
. (Join-Path $PSScriptRoot "init_vs_env.ps1") 2>$null | Out-Null

# Check that cl.exe is available.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue))
{
    Write-Host "cl.exe not found even after init_vs_env.ps1" -ForegroundColor Red
    Write-Host "Check your Visual Studio 2022 installation (C++ MSVC v143 component)."
    exit 1
}

# Check CMake.
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake)
{
    Write-Host "CMake not found. Install it via winget or from https://cmake.org/download/" -ForegroundColor Red
    exit 1
}

# Check JUCE.
if (-not (Test-Path $JucePath))
{
    Write-Host "JUCE not found in $JucePath" -ForegroundColor Red
    exit 1
}

# === CMake configuration (step 1) ===
if (-not $SkipConfigure)
{
    Write-Host "[1/3] Configuring CMake ($Generator)..." -ForegroundColor Cyan

    # Map generator short name to the cmake string.
    $cmakeGen = switch ($Generator)
    {
        "VS2022"            { "Visual Studio 17 2022" }
        default             { $Generator }
    }
    $archArg = @()
    if ($Generator -eq "VS2022") { $archArg = @("-A", "x64") }

    & $cmake -S $ProjectRoot -B $BuildDir -G $cmakeGen @archArg `
        -DCMAKE_BUILD_TYPE=$Configuration `
        -DJUCE_PATH="$JucePath"
    if ($LASTEXITCODE -ne 0)
    {
        Write-Host "CMake configuration failed (code $LASTEXITCODE)." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}
else
{
    Write-Host "[1/3] CMake configuration skipped (SkipConfigure)." -ForegroundColor DarkGray
}

$systemRoot = if ($env:SystemRoot) { $env:SystemRoot } else { "C:\Windows" }
$attribExe = Join-Path (Join-Path $systemRoot "System32") "attrib.exe"
$vcxproj = Join-Path $BuildDir "OpenVoxTuner_VST3.vcxproj"
if ((Test-Path $attribExe) -and (Test-Path $vcxproj))
{
    $content = Get-Content -LiteralPath $vcxproj -Raw
    $content = $content -replace 'attrib \+s', "`"$attribExe`" +s"
    Set-Content -LiteralPath $vcxproj -Value $content -Encoding UTF8
}

# === Generate JuceHeader.h via juceaide (workaround) ===
# The JUCE 8 CMake support does not generate JuceHeader.h automatically
# when the target is a shared library (plugin). So we generate it
# manually with juceaide, then copy it to the right place.
$juceaide = Get-ChildItem $BuildDir -Recurse -Filter "juceaide.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$defsFile = Join-Path $BuildDir "OpenVoxTuner_artefacts\JuceLibraryCode\$Configuration\Defs.txt"
$configDir = Join-Path $BuildDir "OpenVoxTuner_artefacts\JuceLibraryCode\$Configuration"
$topDir = Split-Path $configDir -Parent
if ($juceaide -and (Test-Path $defsFile))
{
    Write-Host "[1b/3] Generating JuceHeader.h via juceaide..." -ForegroundColor Cyan
    & $juceaide.FullName header $defsFile (Join-Path $configDir "JuceHeader.h") 2>&1 | Out-Null
    # Copy to the parent folder so the default include path works.
    Copy-Item -Path (Join-Path $configDir "JuceHeader.h") -Destination (Join-Path $topDir "JuceHeader.h") -Force
}
else
{
    Write-Host "[1b/3] juceaide not found, assuming JuceHeader.h is already generated." -ForegroundColor DarkGray
}

# Same for the OpenVoxTunerTests target (required to build the tests).
$testsDefsFile = Join-Path $BuildDir "OpenVoxTunerTests_artefacts\JuceLibraryCode\$Configuration\Defs.txt"
$testsConfigDir = Join-Path $BuildDir "OpenVoxTunerTests_artefacts\JuceLibraryCode\$Configuration"
$testsTopDir = Split-Path $testsConfigDir -Parent
if ($juceaide -and (Test-Path $testsDefsFile))
{
    Write-Host "[1c/3] Generating JuceHeader.h (OpenVoxTunerTests target)..." -ForegroundColor Cyan
    & $juceaide.FullName header $testsDefsFile (Join-Path $testsConfigDir "JuceHeader.h") 2>&1 | Out-Null
    Copy-Item -Path (Join-Path $testsConfigDir "JuceHeader.h") -Destination (Join-Path $testsTopDir "JuceHeader.h") -Force
}

# === Build (step 2) ===
Write-Host "[2/3] Building $Configuration..." -ForegroundColor Cyan
& $cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0)
{
    Write-Host "Build failed (code $LASTEXITCODE)." -ForegroundColor Red
    exit $LASTEXITCODE
}

# === Tests (step 3) ===
if ($RunTests)
{
    Write-Host "[3/3] Running tests..." -ForegroundColor Cyan
    $testExe = Get-ChildItem $BuildDir -Recurse -Filter "OpenVoxTunerTests.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($testExe)
    {
        & $testExe.FullName
    }
    else
    {
        Write-Host "Test executable not found. Check the CMake configuration." -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=== Build completed successfully ===" -ForegroundColor Green
Write-Host "Outputs:"
Get-ChildItem $BuildDir -Recurse -Filter "*.vst3" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  VST3       : $($_.FullName)" }
Get-ChildItem $BuildDir -Recurse -Filter "OpenVoxTuner.exe" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  Standalone : $($_.FullName)" }
