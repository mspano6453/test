[Setup]
AppId={{6E1B7C2A-5D4F-4A8B-9C3E-7A1F2B9D0E11}
AppName=Tea Towel Suite
AppVersion=1.1.0
AppPublisher=Tea Towel Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
OutputDir=..\out
OutputBaseFilename=Tea Towel Suite Installer (Windows)
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=Tea Towel Suite (Tea Towel + Tape Twin)
WizardStyle=modern

[Files]
Source: "..\build\TeaTowel_artefacts\Release\VST3\Tea Towel.vst3\*"; DestDir: "{app}\Tea Towel.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\TapeTwin_artefacts\Release\VST3\Tape Twin.vst3\*"; DestDir: "{app}\Tape Twin.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\Grooves\*"; DestDir: "{commondocs}\Tea Towel\Grooves"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
FinishedLabel=Tea Towel and Tape Twin are installed. In Ableton Live, open Settings > Plug-Ins and press Rescan.
