[Setup]
AppId={{6E1B7C2A-5D4F-4A8B-9C3E-7A1F2B9D0E11}
AppName=Tea Towel Suite
AppVersion=1.5.0
AppPublisher=Tea Towel Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
UsePreviousAppDir=no
DisableProgramGroupPage=yes
DisableReadyPage=yes
OutputDir=..\out
OutputBaseFilename=Tea Towel Suite Installer (Windows)
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=Tea Towel Suite (Tea Towel, Tape Twin, Lathe, Schaltwerk, Loon)
WizardStyle=modern

[InstallDelete]
; clear out old copies (including the nested folders an earlier installer left behind)
Type: filesandordirs; Name: "{commoncf64}\VST3\Tea Towel.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Tape Twin.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Lathe.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Schaltwerk.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Loon.vst3"

[Files]
Source: "..\build\TeaTowel_artefacts\Release\VST3\Tea Towel.vst3\*"; DestDir: "{app}\Tea Towel.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\TapeTwin_artefacts\Release\VST3\Tape Twin.vst3\*"; DestDir: "{app}\Tape Twin.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Lathe_artefacts\Release\VST3\Lathe.vst3\*"; DestDir: "{app}\Lathe.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Schaltwerk_artefacts\Release\VST3\Schaltwerk.vst3\*"; DestDir: "{app}\Schaltwerk.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Loon_artefacts\Release\VST3\Loon.vst3\*"; DestDir: "{app}\Loon.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\Grooves\*"; DestDir: "{commondocs}\Tea Towel\Grooves"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
FinishedLabel=Tea Towel, Tape Twin, Lathe, Schaltwerk and Loon are installed. In Ableton Live, open Settings > Plug-Ins and press Rescan.
