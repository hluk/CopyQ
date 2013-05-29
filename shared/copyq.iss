#define AppVersion "1.8.2"
#define Root "C:\dev\copyq"

[Setup]
AppId={{9DF1F443-EA0B-4C75-A4D3-767A7783228E}
AppName=CopyQ
AppVersion={#AppVersion}
AppVerName=CopyQ {#AppVersion}
AppPublisher=Lukas Holecek
AppPublisherURL=https://code.google.com/p/copyq/
AppSupportURL=https://code.google.com/p/copyq/
AppUpdatesURL=https://code.google.com/p/copyq/
DefaultDirName={pf}\CopyQ
DefaultGroupName=CopyQ
AllowNoIcons=yes
LicenseFile={#Root}\LICENSE
OutputDir={#Root}\..
OutputBaseFilename=setup
Compression=lzma
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: {cm:AutoStartProgram,CopyQ}; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#Root}\copyq.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\copyq.com"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\AUTHORS"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\msvcp100.dll"; DestDir: "{app}"
Source: "{#Root}\msvcr100.dll"; DestDir: "{app}"
Source: "{#Root}\QtCore4.dll"; DestDir: "{app}"
Source: "{#Root}\QtGui4.dll"; DestDir: "{app}"
Source: "{#Root}\QtNetwork4.dll"; DestDir: "{app}"
Source: "{#Root}\QtScript4.dll"; DestDir: "{app}"
Source: "{#Root}\QtSvg4.dll"; DestDir: "{app}"
Source: "{#Root}\QtWebKit4.dll"; DestDir: "{app}"
Source: "{#Root}\QtXml4.dll"; DestDir: "{app}"
Source: "{#Root}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\CopyQ"; Filename: "{app}\copyq.exe"
Name: "{commondesktop}\CopyQ"; Filename: "{app}\copyq.exe"; Tasks: desktopicon
Name: "{userstartup}\CopyQ"; Filename: "{app}\copyq.exe"; Tasks: startup

[Run]
Filename: "{app}\copyq.exe"; Description: "{cm:LaunchProgram,CopyQ}"; Flags: nowait postinstall skipifsilent
