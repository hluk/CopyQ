[Setup]
AppId={{9DF1F443-EA0B-4C75-A4D3-767A7783228E}
AppName=CopyQ
AppVersion=1.8.2
;AppVerName=CopyQ 1.8.2
AppPublisher=Lukas Holecek
AppPublisherURL=https://code.google.com/p/copyq/
AppSupportURL=https://code.google.com/p/copyq/
AppUpdatesURL=https://code.google.com/p/copyq/
DefaultDirName={pf}\CopyQ
DefaultGroupName=CopyQ
AllowNoIcons=yes
LicenseFile=C:\dev\copyq-1.8.2-windows\LICENSE
OutputDir=C:\dev
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

[Files]
Source: "C:\dev\copyq-1.8.2-windows\copyq.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\plugins\*"; DestDir: "{app}\plugins"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "C:\dev\copyq-1.8.2-windows\copyq.com"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\AUTHORS"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\msvcp100.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\msvcr100.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\QtCore4.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\QtGui4.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\QtNetwork4.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\QtScript4.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\QtSvg4.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\QtWebKit4.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\QtXml4.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\dev\copyq-1.8.2-windows\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\CopyQ"; Filename: "{app}\copyq.exe"
Name: "{commondesktop}\CopyQ"; Filename: "{app}\copyq.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\copyq.exe"; Description: "{cm:LaunchProgram,CopyQ}"; Flags: nowait postinstall skipifsilent

