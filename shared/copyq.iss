; 1. Open this file with Inno Setup with Unicode support and preprocessor.
; 2. Change "#defines" below (or see below how to use COPYQ_INNO_SETUP environment variable).
; 3. Compile "setup.exe".

; Path for output installation file
#define Output                   "."
#define MyAppName                "CopyQ"
#define MyAppNameMin             "copyq"
#define MyAppCopyright           "Lukas Holecek"
#define MyAppCopyrightStartYear  "2009"
#define MyAppCopyrightEndYear    GetDateTimeString('yyyy','','')

[Setup]
AppId={{9DF1F443-EA0B-4C75-A4D3-767A7783228E}
AppName={#MyAppName}
AppVersion={#AppVersion}
AppVerName={#MyAppName} {#AppVersion}

AppCopyright={#MyAppCopyright} {#MyAppCopyrightStartYear}-{#MyAppCopyrightEndYear}
AppPublisher={#MyAppCopyright}

AppPublisherURL=http://hluk.github.io/CopyQ/
AppSupportURL=http://hluk.github.io/CopyQ/
AppUpdatesURL=http://hluk.github.io/CopyQ/

VersionInfoDescription={#MyAppName} installer
VersionInfoProductName={#MyAppName} {#AppVersion}
VersionInfoVersion={#AppVersion}

UninstallDisplayName={#MyAppName} {#AppVersion}
UninstallDisplayIcon={app}\copyq.exe

WizardStyle=Modern
UsePreviousLanguage=no

DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}

ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

AllowNoIcons=yes
LicenseFile={#Source}\LICENSE
OutputDir={#Output}
PrivilegesRequiredOverridesAllowed=dialog
OutputBaseFilename={#MyAppNameMin}-{#AppVersion}-setup
Compression=lzma
SolidCompression=yes
SetupIconFile={#Source}\src\images\icon.ico
WizardImageFile=logo.bmp
WizardSmallImageFile=logo-small.bmp
CloseApplications=force

[Languages]
Name: en; MessagesFile: "compiler:Default.isl"
Name: cz; MessagesFile: "compiler:Languages\Czech.isl"
Name: de; MessagesFile: "compiler:Languages\German.isl"
Name: es; MessagesFile: "compiler:Languages\Spanish.isl"
Name: it; MessagesFile: "compiler:Languages\Italian.isl"
Name: ko; MessagesFile: "Languages\Korean.isl"

[CustomMessages]
en.ProgramFiles=Program Files
en.Translations=Translations
en.Plugins=Plugins
en.PluginText=Text with Highlighting
en.PluginImages=Images
en.PluginWeb=Web Pages
en.PluginNotes=Notes
en.PluginEncrypted=Encryption
en.PluginFakeVim=FakeVim Editor
en.PluginSynchronize=Synchronize Items to Disk
en.PluginTags=Item Tags
en.PluginPinned=Pinned Items

cz.ProgramFiles=Soubory programu
cz.Translations=Překlady
cz.Plugins=Zásuvné moduly
cz.PluginText=Text se zvýrazňováním
cz.PluginImages=Obrázky
cz.PluginWeb=Webové stránky
cz.PluginNotes=Poznámky
cz.PluginEncrypted=Šifrování
cz.PluginFakeVim=FakeVim editor
cz.PluginSynchronize=Synchronizace prvků na disk
cz.PluginTags=Štítky u prvků
cz.PluginPinned=Připnuté prvky

de.AutoStartProgram=Starte %1 automatisch

es.ProgramFiles=Archivos de programa
es.Plugins=Complementos
es.PluginText=Texto resaltado
es.PluginImages=Imágenes
es.PluginWeb=Páginas web

it.ProgramFiles=File programma
it.Translations=Traduzioni
it.Plugins=Plugin
it.PluginText=Testo evidenziato
it.PluginImages=Immagini
it.PluginWeb=Pagine web
it.PluginNotes=Note
it.PluginEncrypted=Criptazione
it.PluginFakeVim=Editor FakeVim
it.PluginSynchronize=Sincronizza elementi disco
it.PluginTags=Etichette elementi
it.PluginPinned=Elementi bloccati

ko.ProgramFiles=Program Files
ko.Translations=번역
ko.Plugins=플러그인
ko.PluginText=강조 표시가 있는 텍스트
ko.PluginImages=이미지
ko.PluginWeb=웹 페이지
ko.PluginNotes=노트
ko.PluginEncrypted=암호화
ko.PluginFakeVim=FakeVim 편집기
ko.PluginSynchronize=항목을 디스크에 동기화
ko.PluginTags=항목 태그
ko.PluginPinned=고정된 항목

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
Name: "plugins/notes"; Description: "{cm:PluginNotes}"; Types: full
Name: "plugins/encrypted"; Description: "{cm:PluginEncrypted}"; Types: full
Name: "plugins/fakevim"; Description: "{cm:PluginFakeVim}"; Types: full
Name: "plugins/synchronize"; Description: "{cm:PluginSynchronize}"; Types: full
Name: "plugins/tags"; Description: "{cm:PluginTags}"; Types: full
Name: "plugins/pinned"; Description: "{cm:PluginPinned}"; Types: full

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; Flags: unchecked
Name: "startup"; Description: {cm:AutoStartProgram,CopyQ}; Flags: unchecked

[Files]
Source: "{#Root}\copyq.exe"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\snoretoast.exe"; DestDir: "{app}"; Components: program; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#Root}\AUTHORS"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\README.md"; DestDir: "{app}"; Components: program; Flags: ignoreversion
Source: "{#Root}\themes\*"; DestDir: "{app}\themes"; Components: program; Flags: ignoreversion
Source: "{#Root}\translations\*.qm"; DestDir: "{app}\translations"; Components: translations; Flags: ignoreversion
Source: "{#Root}\plugins\*itemtext.dll"; DestDir: "{app}\plugins"; Components: plugins/text; Flags: ignoreversion
Source: "{#Root}\plugins\*itemimage.dll"; DestDir: "{app}\plugins"; Components: plugins/images; Flags: ignoreversion
Source: "{#Root}\plugins\*itemnotes.dll"; DestDir: "{app}\plugins"; Components: plugins/notes; Flags: ignoreversion
Source: "{#Root}\plugins\*itemencrypted.dll"; DestDir: "{app}\plugins"; Components: plugins/encrypted; Flags: ignoreversion
Source: "{#Root}\plugins\*itemfakevim.dll"; DestDir: "{app}\plugins"; Components: plugins/fakevim; Flags: ignoreversion
Source: "{#Root}\plugins\*itemsync.dll"; DestDir: "{app}\plugins"; Components: plugins/synchronize; Flags: ignoreversion
Source: "{#Root}\plugins\*itemtags.dll"; DestDir: "{app}\plugins"; Components: plugins/tags; Flags: ignoreversion
Source: "{#Root}\plugins\*itempinned.dll"; DestDir: "{app}\plugins"; Components: plugins/pinned; Flags: ignoreversion

; Qt and toolchain
Source: "{#Root}\bearer\*.dll"; DestDir: "{app}\bearer"; Components: program; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\imageformats\*.dll"; DestDir: "{app}\imageformats"; Components: program; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\platforms\*.dll"; DestDir: "{app}\platforms"; Components: program; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\styles\*.dll"; DestDir: "{app}\styles"; Components: program; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\tls\*.dll"; DestDir: "{app}\tls"; Components: program; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#Root}\*.dll"; DestDir: "{app}"; Components: program; Flags: ignoreversion

[Icons]
Name: "{group}\CopyQ"; Filename: "{app}\copyq.exe"; Parameters: "--start-server show"
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
