; Use Inno Setup with Unicode support and preprocessor.
#define AppVersion "2.0.0 beta"
#define Root "C:\dev\copyq"

[Setup]
AppId={{9DF1F443-EA0B-4C75-A4D3-767A7783228E}
AppName=CopyQ
AppVersion={#AppVersion}
AppVerName=CopyQ {#AppVersion}
AppPublisher=Lukas Holecek
AppPublisherURL=https://sourceforge.net/projects/copyq/
AppSupportURL=https://sourceforge.net/projects/copyq/
AppUpdatesURL=https://sourceforge.net/projects/copyq/
DefaultDirName={pf}\CopyQ
DefaultGroupName=CopyQ
AllowNoIcons=yes
LicenseFile={#Root}\LICENSE
OutputDir={#Root}\..
OutputBaseFilename=setup
Compression=lzma
SolidCompression=yes
WizardSmallImageFile=logo.bmp

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
en.PluginNotes=Notes
en.PluginEncrypted=Encryption
en.PluginFakeVim=FakeVim Editor
en.PluginSynchronize=Synchronize Items to Disk

cz.ProgramFiles=Soubory programu
cz.Plugins=Zásuvné moduly
cz.PluginText=Text se zvýrazňováním
cz.PluginImages=Obrázky
cz.PluginWeb=Webové stránky
cz.PluginData=Různá data
cz.PluginNotes=Poznámky
cz.PluginEncrypted=Šifrování
cz.PluginFakeVim=FakeVim editor
cz.PluginSynchronize=Synchronizace prvků na disk

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
Name: "plugins/notes"; Description: "{cm:PluginNotes}"; Types: full
Name: "plugins/encrypted"; Description: "{cm:PluginEncrypted}"; Types: full
Name: "plugins/fakevim"; Description: "{cm:PluginFakeVim}"; Types: full
Name: "plugins/synchronize"; Description: "{cm:PluginSynchronize}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: {cm:AutoStartProgram,CopyQ}; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#Root}\copyq.exe"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\copyq.com"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\AUTHORS"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\README.md"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\HACKING"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\themes\*"; DestDir: "{app}\themes"; Components: program; Flags: ignoreversion
Source: "{#Root}\plugins\itemtext.dll"; DestDir: "{app}\plugins"; Components: plugins/text; Flags: ignoreversion
Source: "{#Root}\plugins\itemimage.dll"; DestDir: "{app}\plugins"; Components: plugins/images; Flags: ignoreversion
Source: "{#Root}\plugins\itemweb.dll"; DestDir: "{app}\plugins"; Components: plugins/web; Flags: ignoreversion
Source: "{#Root}\plugins\itemdata.dll"; DestDir: "{app}\plugins"; Components: plugins/data; Flags: ignoreversion
Source: "{#Root}\plugins\itemnotes.dll"; DestDir: "{app}\plugins"; Components: plugins/notes; Flags: ignoreversion
Source: "{#Root}\plugins\itemencrypted.dll"; DestDir: "{app}\plugins"; Components: plugins/encrypted; Flags: ignoreversion
Source: "{#Root}\plugins\itemfakevim.dll"; DestDir: "{app}\plugins"; Components: plugins/fakevim; Flags: ignoreversion
Source: "{#Root}\plugins\itemsync.dll"; DestDir: "{app}\plugins"; Components: plugins/synchronize; Flags: ignoreversion
Source: "{#Root}\msvcp100.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\msvcr100.dll"; DestDir: "{app}"; Components: program
Source: "{#Root}\imageformats\*"; DestDir: "{app}\imageformats"; Components: plugins/images plugins/web; Flags: recursesubdirs createallsubdirs
; Qt 4
Source: "{#Root}\QtCore4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\QtGui4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\QtNetwork4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\QtScript4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\QtSvg4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\QtXml4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\QtWebKit4.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\imageformats\qico4.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\imageformats\qsvg4.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
; Qt 5
Source: "{#Root}\icudt51.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\icuin51.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\icuuc51.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\libGLESv2.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\libEGL.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Core.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Gui.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Network.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Script.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Svg.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Xml.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Widgets.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5WebKit.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5WebKitWidgets.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5OpenGL.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5PrintSupport.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Qml.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Quick.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Sensors.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5Sql.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\Qt5V8.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#Root}\imageformats\qico.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#Root}\imageformats\qsvg.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
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
