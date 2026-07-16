@echo off
setlocal
set PATH=C:\Windows\System32;C:\Windows;C:\Windows\System32\Wbem;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64;C:\Program Files\CMake\bin
set INCLUDE=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\winrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt
set LIB=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\onecore\x64;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64
set VCINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Community\VC
set VCToolsInstallDir=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207
set VCToolsVersion=14.44.35207
set WindowsSdkDir=C:\Program Files (x86)\Windows Kits\10\
set WindowsSDKVersion=10.0.26100.0\
set UniversalCRTSdkDir=C:\Program Files (x86)\Windows Kits\10\
set UCRTVersion=10.0.26100.0
set VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\Community
set DevEnvDir=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE
set VS170COMNTOOLS=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\
set COMSPEC=%SystemRoot%\system32\cmd.exe
set PATHEXT=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH;.MSC
"C:\Program Files\CMake\bin\cmake.exe" --build C:\Users\User\Documents\trae_projects\OpenVoxTuner\build --config Release --target OpenVoxTuner_VST3 OpenVoxTuner_Standalone OpenVoxKey_VST3
exit /b %errorlevel%
