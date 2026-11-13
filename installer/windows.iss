[Setup]
AppId={{6E1B7C2A-5D4F-4A8B-9C3E-7A1F2B9D0E11}
AppName=DEMOCRAZY
AppVersion=4.0.0
AppPublisher=DEMOCRAZY
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
UsePreviousAppDir=no
DisableProgramGroupPage=yes
DisableReadyPage=yes
OutputDir=..\out
OutputBaseFilename=DEMOCRAZY Installer (Windows)
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=DEMOCRAZY (Tea Towel, Tape Twin, Lathe, Schaltwerk, Loon, Carousel, Undertow, Parlour, Ether, Chamber, Jangle, Harmonia, Bandleader)
WizardStyle=modern

[InstallDelete]
Type: filesandordirs; Name: "{commondocs}\Tea Towel\Grooves"
; clear out old copies (including the nested folders an earlier installer left behind)
Type: filesandordirs; Name: "{commoncf64}\VST3\Tea Towel.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Tape Twin.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Lathe.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Schaltwerk.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Loon.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Carousel.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Undertow.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Parlour.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Ether.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Chamber.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Jangle.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Harmonia.vst3"
Type: filesandordirs; Name: "{commoncf64}\VST3\Bandleader.vst3"

[Files]
Source: "..\build\TeaTowel_artefacts\Release\VST3\Tea Towel.vst3\*"; DestDir: "{app}\Tea Towel.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\TapeTwin_artefacts\Release\VST3\Tape Twin.vst3\*"; DestDir: "{app}\Tape Twin.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Lathe_artefacts\Release\VST3\Lathe.vst3\*"; DestDir: "{app}\Lathe.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Schaltwerk_artefacts\Release\VST3\Schaltwerk.vst3\*"; DestDir: "{app}\Schaltwerk.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Loon_artefacts\Release\VST3\Loon.vst3\*"; DestDir: "{app}\Loon.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Carousel_artefacts\Release\VST3\Carousel.vst3\*"; DestDir: "{app}\Carousel.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Undertow_artefacts\Release\VST3\Undertow.vst3\*"; DestDir: "{app}\Undertow.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Parlour_artefacts\Release\VST3\Parlour.vst3\*"; DestDir: "{app}\Parlour.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Ether_artefacts\Release\VST3\Ether.vst3\*"; DestDir: "{app}\Ether.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Chamber_artefacts\Release\VST3\Chamber.vst3\*"; DestDir: "{app}\Chamber.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Jangle_artefacts\Release\VST3\Jangle.vst3\*"; DestDir: "{app}\Jangle.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Harmonia_artefacts\Release\VST3\Harmonia.vst3\*"; DestDir: "{app}\Harmonia.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\build\Bandleader_artefacts\Release\VST3\Bandleader.vst3\*"; DestDir: "{app}\Bandleader.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\Grooves\*"; DestDir: "{commondocs}\DEMOCRAZY\Grooves"; Flags: recursesubdirs createallsubdirs ignoreversion

[Messages]
FinishedLabel=DEMOCRAZY is installed: Tea Towel, Tape Twin, Lathe, Schaltwerk, Loon, Carousel, Undertow, Parlour, Ether, Chamber, Jangle, Harmonia and Bandleader. In Ableton Live, open Settings > Plug-Ins and press Rescan.
