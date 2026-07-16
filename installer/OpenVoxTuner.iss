[Setup]
AppName=OpenVoxTuner
AppVersion=0.1.0
AppPublisher=EiffelBS
AppPublisherURL=https://github.com/EiffelBS
DefaultDirName={autopf}\OpenVoxTuner
DisableDirPage=yes
DefaultGroupName=OpenVoxTuner
DisableProgramGroupPage=yes
OutputDir=..\build\installer
ShowComponentSizes=yes
OutputBaseFilename=OpenVoxTuner_Windows_Installer
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
SetupIconFile=compiler:SetupClassicIcon.ico
UninstallDisplayIcon={app}\unins000.exe

[Types]
Name: "full"; Description: "Installation complète (Recommandé)"
Name: "custom"; Description: "Installation personnalisée"; Flags: iscustom

[Components]
Name: "vst3"; Description: "Plugin VST3"; Types: full custom
Name: "standalone"; Description: "Application autonome (Standalone)"; Types: full custom
Name: "companion"; Description: "OpenVoxKey (plugin compagnon de detection de tonalite)"; Types: full custom

[Dirs]
Name: "{commoncf64}\VST3\OpenVoxTuner.vst3"; Components: vst3
Name: "{commoncf64}\VST3\OpenVoxKey.vst3"; Components: companion
Name: "{app}"

[InstallDelete]
; Delete any existing single-file VST3 or old directory before installing the new bundle
Type: filesandordirs; Name: "{commoncf64}\VST3\OpenVoxTuner.vst3"; Components: vst3
Type: filesandordirs; Name: "{commoncf64}\VST3\Autotune Clone.vst3"; Components: vst3
Type: filesandordirs; Name: "{commoncf64}\VST3\OpenVoxKey.vst3"; Components: companion

[Files]
; VST3 plugin (JUCE bundle structure for VST3)
Source: "..\build\OpenVoxTuner_artefacts\Release\VST3\OpenVoxTuner.vst3\*"; DestDir: "{commoncf64}\VST3\OpenVoxTuner.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

; OpenVoxKey companion VST3 (key/scale detector that publishes to the shared bridge)
Source: "..\build\OpenVoxKey_artefacts\Release\VST3\OpenVoxKey.vst3\*"; DestDir: "{commoncf64}\VST3\OpenVoxKey.vst3"; Components: companion; Flags: ignoreversion recursesubdirs createallsubdirs

; Standalone Application
Source: "..\build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

[Icons]
Name: "{group}\OpenVoxTuner Standalone"; Filename: "{app}\OpenVoxTuner.exe"; Components: standalone
Name: "{group}\Uninstall OpenVoxTuner"; Filename: "{uninstallexe}"
Name: "{autoprograms}\OpenVoxTuner Standalone"; Filename: "{app}\OpenVoxTuner.exe"; Components: standalone

[Run]
; Option to launch the standalone app after installation
Filename: "{app}\OpenVoxTuner.exe"; Description: "Launch OpenVoxTuner Standalone"; Flags: nowait postinstall skipifsilent; Components: standalone
