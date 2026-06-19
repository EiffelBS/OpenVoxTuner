[Setup]
AppName=OpenVoxTuner
AppVersion=0.1.0
AppPublisher=EiffelBS
AppPublisherURL=https://github.com/EiffelBS
DefaultDirName={commoncf64}\VST3\OpenVoxTuner.vst3
DisableDirPage=yes
DefaultGroupName=OpenVoxTuner
DisableProgramGroupPage=yes
OutputDir=..\build\installer
OutputBaseFilename=OpenVoxTuner_Windows_Installer
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
SetupIconFile=compiler:SetupClassicIcon.ico
UninstallDisplayIcon={app}\unins000.exe

[Dirs]
Name: "{commoncf64}\VST3\OpenVoxTuner.vst3"
Name: "{autopf}\OpenVoxTuner"

[InstallDelete]
; Delete any existing single-file VST3 or old directory before installing the new bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\OpenVoxTuner.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Autotune Clone.vst3"

[Files]
; VST3 plugin (JUCE bundle structure for VST3)
Source: "..\build\OpenVoxTuner_artefacts\Release\VST3\OpenVoxTuner.vst3\*"; DestDir: "{commoncf64}\VST3\OpenVoxTuner.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

; Standalone Application
Source: "..\build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe"; DestDir: "{autopf}\OpenVoxTuner"; Flags: ignoreversion

[Icons]
Name: "{group}\OpenVoxTuner Standalone"; Filename: "{autopf}\OpenVoxTuner\OpenVoxTuner.exe"
Name: "{group}\Uninstall OpenVoxTuner"; Filename: "{uninstallexe}"
Name: "{autoprograms}\OpenVoxTuner Standalone"; Filename: "{autopf}\OpenVoxTuner\OpenVoxTuner.exe"

[Run]
; Option to launch the standalone app after installation
Filename: "{autopf}\OpenVoxTuner\OpenVoxTuner.exe"; Description: "Launch OpenVoxTuner Standalone"; Flags: nowait postinstall skipifsilent
