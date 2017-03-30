git describe --tags --always HEAD > _git_tag.tmp
set /p AppVersion=<_git_tag.tmp
del _git_tag.tmp

set QtRoot=%QTDIR%
set Source=%APPVEYOR_BUILD_FOLDER%
set Name=copyq-%AppVersion%
set Destination=%APPVEYOR_BUILD_FOLDER%\%Name%
set BuildRoot=%APPVEYOR_BUILD_FOLDER%\build
set Executable=%Destination%\copyq.exe

if [%VC_VARS_ARCH%] == [] (set Build=%BuildRoot%) else (set Build=%BuildRoot%\Release)
if [%VC_VARS_ARCH%] == [] (set BuildPlugins=%BuildRoot%\plugins) else (set BuildPlugins=%BuildRoot%\plugins\Release)

mkdir "%Destination%"
xcopy /F "%Build%\copyq.exe" "%Destination%" || goto :error

xcopy /F "%Source%\AUTHORS" "%Destination%" || goto :error
xcopy /F "%Source%\LICENSE" "%Destination%" || goto :error
xcopy /F "%Source%\README.md" "%Destination%" || goto :error

mkdir "%Destination%\themes"
xcopy /F "%Source%\shared\themes\*" "%Destination%\themes" || goto :error

mkdir "%Destination%\translations"
xcopy /F "%BuildRoot%\src\*.qm" "%Destination%\translations" || goto :error

mkdir "%Destination%\plugins"
xcopy /F "%BuildPlugins%\*.dll" "%Destination%\plugins" || goto :error

%QTDIR%\bin\windeployqt --release --no-system-d3d-compiler --no-angle --no-opengl-sw "%Executable%" || goto :error

7z a "%Name%.zip" -r "%Destination%" || goto :error

choco install -y InnoSetup
"C:\Program Files (x86)\Inno Setup 5\iscc" "/O%APPVEYOR_BUILD_FOLDER%" "/DAppVersion=%AppVersion%" "/DRoot=%Destination%" "/DSource=%Source%" "%Source%\Shared\copyq.iss" || goto :error

"%Executable%" --help || goto :error
"%Executable%" --version || goto :error
"%Executable%" --info || goto :error
"%Executable%" tests || "%Executable%" tests || goto :error

:error
exit /b %errorlevel%
