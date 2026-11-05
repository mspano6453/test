[Setup]
AppId={{6E1B7C2A-5D4F-4A8B-9C3E-7A1F2B9D0E11}
AppName=Tea Towel
AppVersion=1.0.0
AppPublisher=Tea Towel Audio
DefaultDirName={commoncf64}\VST3\Tea Towel.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
OutputDir=..\out
OutputBaseFilename=Tea Towel Installer (Windows)
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=Tea Towel
WizardStyle=modern

[Files]
Source: "..\build\TeaTowel_artefacts\Release\VST3\Tea Towel.vst3\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "..\Grooves\*"; DestDir: "{commondocs}\Tea Towel\Grooves"; Flags: ignoreversion

[Messages]
FinishedLabel=Tea Towel is installed. In Ableton Live, open Settings > Plug-Ins and press Rescan.
