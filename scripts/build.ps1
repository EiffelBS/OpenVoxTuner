# ============================================================================
# build.ps1
# Configure et compile le plugin OpenVoxTuner via CMake + MSVC.
# Ne necessite pas Projucer.
#
# Prerequis :
#   - JUCE 8 dans C:\JUCE
#   - Visual Studio 2022 (MSVC v143)
#   - Windows 10 SDK
#   - CMake >= 3.22 (winget l'installe automatiquement si absent)
#   - VST3 SDK (optionnel, declare dans le .jucer / CMake)
#
# Utilisation :
#   . .\init_vs_env.ps1   # OU laissez build.ps1 le faire automatiquement
#   .\build.ps1
#
#   .\build.ps1 -Configuration Debug
#   .\build.ps1 -SkipConfigure
#   .\build.ps1 -Generator NMake   # ou Ninja (peut bloquer sur certains env.)
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

# === Source l'environnement MSVC (idempotent) ===
. (Join-Path $PSScriptRoot "init_vs_env.ps1") 2>$null | Out-Null

# Verifie que cl.exe est disponible.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue))
{
    Write-Host "cl.exe introuvable meme apres init_vs_env.ps1" -ForegroundColor Red
    Write-Host "Verifie l'installation de Visual Studio 2022 (composant C++ MSVC v143)."
    exit 1
}

# Verifie CMake.
$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake)
{
    Write-Host "CMake introuvable. Installe-le via winget ou depuis https://cmake.org/download/" -ForegroundColor Red
    exit 1
}

# Verifie JUCE.
if (-not (Test-Path $JucePath))
{
    Write-Host "JUCE introuvable dans $JucePath" -ForegroundColor Red
    exit 1
}

# === Configuration CMake (1ere etape) ===
if (-not $SkipConfigure)
{
    Write-Host "[1/3] Configuration CMake ($Generator)..." -ForegroundColor Cyan
    $testsFlag = if ($RunTests) { "ON" } else { "OFF" }

    # Mappe le nom court du generateur vers la chaine cmake.
    $cmakeGen = switch ($Generator)
    {
        "VS2022"            { "Visual Studio 17 2022" }
        default             { $Generator }
    }
    $archArg = @()
    if ($Generator -eq "VS2022") { $archArg = @("-A", "x64") }

    & $cmake -S $ProjectRoot -B $BuildDir -G $cmakeGen @archArg `
        -DCMAKE_BUILD_TYPE=$Configuration `
        -DJUCE_PATH="$JucePath" `
        -DAUTOTUNE_BUILD_TESTS=$testsFlag
    if ($LASTEXITCODE -ne 0)
    {
        Write-Host "Echec de la configuration CMake (code $LASTEXITCODE)." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}
else
{
    Write-Host "[1/3] Configuration CMake ignoree (SkipConfigure)." -ForegroundColor DarkGray
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

# === Generation de JuceHeader.h via juceaide (workaround) ===
# Le support CMake de JUCE 8 ne genere pas automatiquement JuceHeader.h
# quand la cible est une bibliotheque partagee (plugin). On le genere
# donc a la main avec juceaide, puis on copie au bon endroit.
$juceaide = Get-ChildItem $BuildDir -Recurse -Filter "juceaide.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
$defsFile = Join-Path $BuildDir "AutotuneClone_artefacts\JuceLibraryCode\$Configuration\Defs.txt"
$configDir = Join-Path $BuildDir "AutotuneClone_artefacts\JuceLibraryCode\$Configuration"
$topDir = Split-Path $configDir -Parent
if ($juceaide -and (Test-Path $defsFile))
{
    Write-Host "[1b/3] Generation de JuceHeader.h via juceaide..." -ForegroundColor Cyan
    & $juceaide.FullName header $defsFile (Join-Path $configDir "JuceHeader.h") 2>&1 | Out-Null
    # Copie au niveau superieur pour que le include path par defaut fonctionne.
    Copy-Item -Path (Join-Path $configDir "JuceHeader.h") -Destination (Join-Path $topDir "JuceHeader.h") -Force
}
else
{
    Write-Host "[1b/3] juceaide introuvable, on suppose que JuceHeader.h est deja genere." -ForegroundColor DarkGray
}

# Idem pour la cible AutotuneTests (necessaire pour compiler les tests).
$testsDefsFile = Join-Path $BuildDir "AutotuneTests_artefacts\JuceLibraryCode\$Configuration\Defs.txt"
$testsConfigDir = Join-Path $BuildDir "AutotuneTests_artefacts\JuceLibraryCode\$Configuration"
$testsTopDir = Split-Path $testsConfigDir -Parent
if ($juceaide -and (Test-Path $testsDefsFile))
{
    Write-Host "[1c/3] Generation de JuceHeader.h (cible AutotuneTests)..." -ForegroundColor Cyan
    & $juceaide.FullName header $testsDefsFile (Join-Path $testsConfigDir "JuceHeader.h") 2>&1 | Out-Null
    Copy-Item -Path (Join-Path $testsConfigDir "JuceHeader.h") -Destination (Join-Path $testsTopDir "JuceHeader.h") -Force
}

# === Build (2eme etape) ===
Write-Host "[2/3] Compilation $Configuration..." -ForegroundColor Cyan
& $cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0)
{
    Write-Host "Echec de la compilation (code $LASTEXITCODE)." -ForegroundColor Red
    exit $LASTEXITCODE
}

# === Tests (3eme etape) ===
if ($RunTests)
{
    Write-Host "[3/3] Execution des tests..." -ForegroundColor Cyan
    $testExe = Get-ChildItem $BuildDir -Recurse -Filter "AutotuneTests.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($testExe)
    {
        & $testExe.FullName
    }
    else
    {
        Write-Host "Executable de test introuvable. Verifie la configuration CMake." -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "=== Build termine avec succes ===" -ForegroundColor Green
Write-Host "Sorties :"
Get-ChildItem $BuildDir -Recurse -Filter "*.vst3" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  VST3       : $($_.FullName)" }
Get-ChildItem $BuildDir -Recurse -Filter "AutotuneClone.exe" -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  Standalone : $($_.FullName)" }
