; 1. Open this file with Inno Setup with Unicode support and preprocessor.
; 2. Change "#defines" below (or see below how to use COPYQ_INNO_SETUP environment variable).
; 3. Compile "setup.exe".
#define AppVersion "2.6.1"

; Path with build
#define Root "C:\dev\build-copyq-Desktop-Release"

; Path with source code
#define Source "C:\dev\copyq"

; Path for output installation file
#define Output "C:\dev"

; Choose Qt version.
#define Qt 4
;#define Qt 5

; Choose toolchain (Qt libraries must be built with this toolchain).
#define Toolchain "MSVC10"
;#define Toolchain "MinGW"

; Choose path to Qt installation.
#if Qt == 4
# if Toolchain == "MSVC10"
#   define QtRoot "C:\Qt\4.8.7"
# else
#   define QtRoot "C:\Qt\4.8.7-mingw"
# endif
#else
# define QtRoot "C:\Qt\5.2.0"
#endif

; msbuild generates Release and Debug subfolders
;#define BuildConf "Release"
#define BuildConf ""

; MSVC
#if Toolchain == "MSVC10"
# define WindowsRoot "C:\Windows\SysWOW64"
#endif

; To make changes permanent you can the settings above into a custom file.
; The path for this file is specified using environment variable COPYQ_INNO_SETUP.
#if GetEnv("COPYQ_INNO_SETUP") != ""
# include GetEnv("COPYQ_INNO_SETUP")
#endif

[Setup]
AppId={{9DF1F443-EA0B-4C75-A4D3-767A7783228E}
AppName=CopyQ
AppVersion={#AppVersion}
AppVerName=CopyQ {#AppVersion}
AppPublisher=Lukas Holecek
AppPublisherURL=http://hluk.github.io/CopyQ/
AppSupportURL=http://hluk.github.io/CopyQ/
AppUpdatesURL=http://hluk.github.io/CopyQ/
DefaultDirName={pf}\CopyQ
DefaultGroupName=CopyQ
AllowNoIcons=yes
LicenseFile={#Source}\LICENSE
OutputDir={#Output}
OutputBaseFilename=copyq-{#AppVersion}-setup
Compression=lzma
SolidCompression=yes
SetupIconFile={#Source}\src\images\icon.ico
WizardImageFile=logo.bmp
WizardSmallImageFile=logo-small.bmp

[Languages]
Name: en; MessagesFile: "compiler:Default.isl"
Name: cz; MessagesFile: "compiler:Languages\Czech.isl"
Name: de; MessagesFile: "compiler:Languages\German.isl"
Name: es; MessagesFile: "compiler:Languages\Spanish.isl"

[CustomMessages]
en.ProgramFiles=Program Files
en.Translations=Translations
en.Plugins=Plugins
en.PluginText=Text with Highlighting
en.PluginImages=Images
en.PluginWeb=Web Pages
en.PluginData=Various Data
en.PluginNotes=Notes
en.PluginEncrypted=Encryption
en.PluginFakeVim=FakeVim Editor
en.PluginSynchronize=Synchronize Items to Disk
en.PluginTags=Item Tags

cz.ProgramFiles=Soubory programu
cz.Translations=Překlady
cz.Plugins=Zásuvné moduly
cz.PluginText=Text se zvýrazňováním
cz.PluginImages=Obrázky
cz.PluginWeb=Webové stránky
cz.PluginData=Různá data
cz.PluginNotes=Poznámky
cz.PluginEncrypted=Šifrování
cz.PluginFakeVim=FakeVim editor
cz.PluginSynchronize=Synchronizace prvků na disk
cz.PluginTags=Štítky u prvků

de.AutoStartProgram=Starte %1 automatisch

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
Name: "translations"; Description: "{cm:Translations}"; Types: full compact custom
Name: "plugins"; Description: "{cm:Plugins}"; Types: full
Name: "plugins/text"; Description: "{cm:PluginText}"; Types: full
Name: "plugins/images"; Description: "{cm:PluginImages}"; Types: full
Name: "plugins/web"; Description: "{cm:PluginWeb}"; Types: full
Name: "plugins/data"; Description: "{cm:PluginData}"; Types: full
Name: "plugins/notes"; Description: "{cm:PluginNotes}"; Types: full
Name: "plugins/encrypted"; Description: "{cm:PluginEncrypted}"; Types: full
Name: "plugins/fakevim"; Description: "{cm:PluginFakeVim}"; Types: full
Name: "plugins/synchronize"; Description: "{cm:PluginSynchronize}"; Types: full
Name: "plugins/tags"; Description: "{cm:PluginTags}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked
Name: "startup"; Description: {cm:AutoStartProgram,CopyQ}; Flags: unchecked

[Files]
Source: "{#Root}\{#BuildConf}\copyq.exe"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Source}\AUTHORS"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Source}\LICENSE"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Source}\README.md"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Source}\shared\themes\*"; DestDir: "{app}\themes"; Components: program; Flags: ignoreversion
Source: "{#Root}\src\*.qm"; DestDir: "{app}\translations"; Components: translations; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemtext.dll"; DestDir: "{app}\plugins"; Components: plugins/text; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemimage.dll"; DestDir: "{app}\plugins"; Components: plugins/images; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemweb.dll"; DestDir: "{app}\plugins"; Components: plugins/web; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemdata.dll"; DestDir: "{app}\plugins"; Components: plugins/data; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemnotes.dll"; DestDir: "{app}\plugins"; Components: plugins/notes; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemencrypted.dll"; DestDir: "{app}\plugins"; Components: plugins/encrypted; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemfakevim.dll"; DestDir: "{app}\plugins"; Components: plugins/fakevim; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemsync.dll"; DestDir: "{app}\plugins"; Components: plugins/synchronize; Flags: ignoreversion
Source: "{#Root}\plugins\{#BuildConf}\*itemtags.dll"; DestDir: "{app}\plugins"; Components: plugins/tags; Flags: ignoreversion

; Qt
#if Qt == 4
Source: "{#QtRoot}\plugins\imageformats\*.dll"; Excludes: "*d4.dll"; DestDir: "{app}\imageformats"; Components: plugins/images plugins/web; Flags: recursesubdirs createallsubdirs
Source: "{#QtRoot}\bin\QtCore4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\QtGui4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\QtNetwork4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\QtScript4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\QtSvg4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\QtXml4.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\QtWebKit4.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\plugins\imageformats\qico4.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\plugins\imageformats\qsvg4.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\translations\qt_*.qm"; DestDir: "{app}\translations"; Components: translations; Flags: skipifsourcedoesntexist
#else
Source: "{#QtRoot}\plugins\imageformats\*.dll"; Excludes: "*d.dll"; DestDir: "{app}\imageformats"; Components: plugins/images plugins/web; Flags: recursesubdirs createallsubdirs
Source: "{#QtRoot}\bin\icudt51.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\icuin51.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\icuuc51.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\libGLESv2.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\libEGL.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Core.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Gui.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Network.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Script.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Svg.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Xml.dll"; DestDir: "{app}"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Widgets.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5WebKit.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5WebKitWidgets.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5OpenGL.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5PrintSupport.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Qml.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Quick.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Sensors.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5Sql.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\bin\Qt5V8.dll"; DestDir: "{app}"; Components: plugins/web; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\plugins\imageformats\qico.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
Source: "{#QtRoot}\plugins\imageformats\qsvg.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: skipifsourcedoesntexist
#endif

; Toolchain
#if Toolchain == "MSVC10"
Source: "{#WindowsRoot}\msvcp100.dll"; DestDir: "{app}"; Components: program
Source: "{#WindowsRoot}\msvcr100.dll"; DestDir: "{app}"; Components: program
#else
Source: "{#QtRoot}\bin\libgcc_s_dw2-1.dll"; DestDir: "{app}"; Components: program
Source: "{#QtRoot}\bin\libstdc++-6.dll"; DestDir: "{app}"; Components: program
Source: "{#QtRoot}\bin\libwinpthread-1.dll"; DestDir: "{app}"; Components: program
#endif

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
