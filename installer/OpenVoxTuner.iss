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
; Detect the user's Windows UI language so the installer opens in the right language.
; Falls back to English when the locale is unsupported.
LanguageDetectionMethod=locale

[Languages]
Name: "english"; MessagesFile: "lang\ovt_en.isl"
Name: "french"; MessagesFile: "lang\ovt_fr.isl"
Name: "german"; MessagesFile: "lang\ovt_de.isl"
Name: "spanish"; MessagesFile: "lang\ovt_es.isl"
Name: "japanese"; MessagesFile: "lang\ovt_ja.isl"

; Per-language strings (Types, Components, Run, Shortcuts) live in the
; installer/lang/ovt_*.isl files above — one file per language, each including
; the matching stock Inno Setup .isl plus our [CustomMessages] overrides.
; This is the supported multi-language mechanism for this Inno Setup version
; (inline "Name|lang" suffixes in [CustomMessages] are not accepted).

[Types]
Name: "full"; Description: "{cm:MsgFull}"
Name: "custom"; Description: "{cm:MsgCustom}"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:MsgVst3}"; Types: full custom
Name: "standalone"; Description: "{cm:MsgStandalone}"; Types: full custom
Name: "companion"; Description: "{cm:MsgCompanion}"; Types: full custom

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
Name: "{group}\{cm:MsgShortcutStandalone}"; Filename: "{app}\OpenVoxTuner.exe"; Components: standalone
Name: "{group}\{cm:MsgShortcutUninstall}"; Filename: "{uninstallexe}"
Name: "{autoprograms}\{cm:MsgShortcutStandalone}"; Filename: "{app}\OpenVoxTuner.exe"; Components: standalone

[Run]
; Option to launch the standalone app after installation
Filename: "{app}\OpenVoxTuner.exe"; Description: "{cm:MsgRunLaunch}"; Flags: nowait postinstall skipifsilent; Components: standalone
