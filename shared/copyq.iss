; Use Inno Setup with Unicode support and preprocessor.
#define AppVersion "1.8.3"
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
Name: en; MessagesFile: "compiler:Default.isl"
Name: cz; MessagesFile: "compiler:Languages\Czech.isl"
Name: de; MessagesFile: "compiler:Languages\German.isl"
Name: es; MessagesFile: "compiler:Languages\Spanish.isl"

[CustomMessages]
en.ProgramFiles=Program Files
en.Plugins=Plugins
en.PluginText=Text with Highlighting
en.PluginImages=Images
en.PluginWeb=Web Pages
en.PluginData=Various Data

cz.ProgramFiles=Soubory programu
cz.Plugins=Zásuvné moduly
cz.PluginText=Text se zvýrazňováním
cz.PluginImages=Obrázky
cz.PluginWeb=Webové stránky
cz.PluginData=Různá data

es.ProgramFiles=Archivos de programa
es.Plugins=Complementos
es.PluginText=Texto resaltado
es.PluginImages=Imágenes
es.PluginWeb=Páginas web
es.PluginData=Varios datos

[Types]
Name: "full"; Description: "{code:GetFullInstallation}"
Name: "compact"; Description: "{code:GetCompactInstallation}"
Name: "custom"; Description: "{code:GetCustomInstallation}"; Flags: iscustom

[Components]
Name: "program"; Description: "{cm:ProgramFiles}"; Types: full compact custom; Flags: fixed
Name: "plugins"; Description: "{cm:Plugins}"; Types: full
Name: "plugins/text"; Description: "{cm:PluginText}"; Types: full
Name: "plugins/images"; Description: "{cm:PluginImages}"; Types: full
Name: "plugins/web"; Description: "{cm:PluginWeb}"; Types: full
Name: "plugins/data"; Description: "{cm:PluginData}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: {cm:AutoStartProgram,CopyQ}; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#Root}\copyq.exe"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\copyq.com"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\AUTHORS"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\README.md"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\msvcp100.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\msvcr100.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\QtCore4.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\QtGui4.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\QtNetwork4.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\QtScript4.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\QtSvg4.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\QtXml4.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\QtWebKit4.dll"; DestDir: "{app}"; Components: plugins/web
Source: "{#Root}\plugins\itemtext.dll"; DestDir: "{app}\plugins"; Components: plugins/text; Flags: ignoreversion
Source: "{#Root}\plugins\itemimage.dll"; DestDir: "{app}\plugins"; Components: plugins/images; Flags: ignoreversion
Source: "{#Root}\plugins\itemweb.dll"; DestDir: "{app}\plugins"; Components: plugins/web; Flags: ignoreversion
Source: "{#Root}\plugins\itemdata.dll"; DestDir: "{app}\plugins"; Components: plugins/data; Flags: ignoreversion
Source: "{#Root}\imageformats\*"; DestDir: "{app}\imageformats"; Components: plugins/images plugins/web; Flags: recursesubdirs createallsubdirs
Source: "{#Root}\imageformats\qico4.dll"; DestDir: "{app}\imageformats"; Components: program
Source: "{#Root}\imageformats\qsvg4.dll"; DestDir: "{app}\imageformats"; Components: program
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\CopyQ"; Filename: "{app}\copyq.exe"
Name: "{commondesktop}\CopyQ"; Filename: "{app}\copyq.exe"; Tasks: desktopicon
Name: "{userstartup}\CopyQ"; Filename: "{app}\copyq.exe"; Tasks: startup

[Run]
Filename: "{app}\copyq.exe"; Description: "{cm:LaunchProgram,CopyQ}"; Flags: nowait postinstall skipifsilent

[Code]
function GetFullInstallation(Param: string): string;
begin
	Result := SetupMessage(msgFullInstallation);
end;

function GetCustomInstallation(Param: string): string;
begin
	Result := SetupMessage(msgCustomInstallation);
end;

function GetCompactInstallation(Param: string): string;
begin
	Result := SetupMessage(msgCompactInstallation);
end;
