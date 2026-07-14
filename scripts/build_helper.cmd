@echo off
setlocal
REM ===========================================================================
REM OpenVoxTuner - Windows build helper (portable, machine-agnostic)
REM
REM Sets up the Visual Studio 2022 + Windows SDK environment for an MSBuild
REM based CMake build, then builds the VST3 and Standalone targets. All paths
REM are resolved dynamically via vswhere, so this script works on any Visual
REM Studio 2022 edition (Community / Professional / Enterprise) and any
REM machine without any hard-coded paths.
REM
REM NOTE: build_installer.ps1 regenerates this file at release time with the
REM exact resolved paths. This committed copy is a portable fallback used when
REM running the helper directly (e.g. "scripts\build_helper.cmd").
REM ===========================================================================

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_ROOT=%%~fI"
set "BUILD_DIR=%PROJECT_ROOT%\build"

REM --- Locate Visual Studio 2022 via vswhere (shipped with VS) ---
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [build_helper] vswhere.exe not found. Please install Visual Studio 2022.
    exit /b 1
)
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"

REM --- Fallback if vswhere returned nothing ---
if not defined VSROOT set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\Community"

REM --- Latest installed MSVC toolset version ---
set "MSVC_BASE=%VSROOT%\VC\Tools\MSVC"
set "VCTOOLSVER="
for /f "delims=" %%d in ('dir /b /ad /on "%MSVC_BASE%" 2^>nul') do set "VCTOOLSVER=%%d"
if not defined VCTOOLSVER set "VCTOOLSVER=14.44.35207"
set "VS_TOOLS_ROOT=%MSVC_BASE%\%VCTOOLSVER%"

REM --- Latest installed Windows SDK 10.0.* version ---
set "SDK_ROOT=%ProgramFiles(x86)%\Windows Kits\10"
set "SDK_VERSION="
for /f "delims=" %%d in ('dir /b /ad /on "%SDK_ROOT%\Include" 2^>nul ^| findstr /r "^10\.0\."') do set "SDK_VERSION=%%d"
if not defined SDK_VERSION set "SDK_VERSION=10.0.19041.0"

set "MSVC_BIN=%VS_TOOLS_ROOT%\bin\Hostx64\x64"
set "SDK_BIN=%SDK_ROOT%\bin\%SDK_VERSION%\x64"
set "CMAKE_BIN=C:\Program Files\CMake\bin"

set "PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;%MSVC_BIN%;%SDK_BIN%;%CMAKE_BIN%"
set "INCLUDE=%VS_TOOLS_ROOT%\include;%SDK_ROOT%\Include\%SDK_VERSION%\ucrt;%SDK_ROOT%\Include\%SDK_VERSION%\um;%SDK_ROOT%\Include\%SDK_VERSION%\shared;%SDK_ROOT%\Include\%SDK_VERSION%\winrt;%SDK_ROOT%\Include\%SDK_VERSION%\cppwinrt"
set "LIB=%VS_TOOLS_ROOT%\lib\onecore\x64;%VS_TOOLS_ROOT%\lib\x64;%SDK_ROOT%\Lib\%SDK_VERSION%\ucrt\x64;%SDK_ROOT%\Lib\%SDK_VERSION%\um\x64"
set "LIBPATH=%VS_TOOLS_ROOT%\lib\onecore\x64;%VS_TOOLS_ROOT%\lib\x64;%SDK_ROOT%\Lib\%SDK_VERSION%\um\x64"
set "VCINSTALLDIR=%VSROOT%\VC"
set "VCToolsInstallDir=%VS_TOOLS_ROOT%"
set "VCToolsVersion=%VCTOOLSVER%"
set "WindowsSdkDir=%SDK_ROOT%\"
set "WindowsSDKVersion=%SDK_VERSION%\"
set "UniversalCRTSdkDir=%SDK_ROOT%\"
set "UCRTVersion=%SDK_VERSION%"
set "VSINSTALLDIR=%VSROOT%"
set "DevEnvDir=%VSROOT%\Common7\IDE"
set "VS170COMNTOOLS=%VSROOT%\Common7\Tools\"

if not exist "%BUILD_DIR%" (
    echo [build_helper] Build directory not found: %BUILD_DIR%
    echo [build_helper] Configure first, e.g. cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DJUCE_PATH=C:\JUCE
    exit /b 1
)

echo [build_helper] Building OpenVoxTuner (MSVC %VCTOOLSVER%, SDK %SDK_VERSION%)
"%CMAKE_BIN%\cmake.exe" --build "%BUILD_DIR%" --config Release --target OpenVoxTuner_VST3 OpenVoxTuner_Standalone
exit /b %errorlevel%
