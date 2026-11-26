#define AppName "DukaanOS"
#define AppVersion "1.0.0"
#define AppPublisher "DukaanOS"

[Setup]
AppId={{A7C112A7-5B8E-4DB7-A570-2FE97F87B244}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\Programs\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
OutputDir=dist
OutputBaseFilename=DukaanOS-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\DukaanOS.exe

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "dist\DukaanOS\*"; DestDir: "{app}"; Excludes: "\Data\*"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "dist\DukaanOS\Data\*"; DestDir: "{app}\Data"; Flags: ignoreversion onlyifdoesntexist uninsneveruninstall recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\DukaanOS.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\DukaanOS.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\DukaanOS.exe"; Description: "Launch {#AppName}"; Flags: postinstall nowait skipifsilent
