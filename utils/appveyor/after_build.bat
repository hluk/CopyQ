@echo on

git describe --tags --always HEAD > _git_tag.tmp || goto :error
set /p AppVersion=<_git_tag.tmp
del _git_tag.tmp

set Source=%APPVEYOR_BUILD_FOLDER%
set Name=copyq-%AppVersion%
set Destination=%APPVEYOR_BUILD_FOLDER%\%Name%
set BuildRoot=%APPVEYOR_BUILD_FOLDER%\build
set Executable=%Destination%\copyq.exe
set Build=%BuildRoot%\%BUILD_SUB_DIR%
set BuildPlugins=%BuildRoot%\plugins\%BUILD_SUB_DIR%

mkdir "%Destination%"

dir /s %QTDIR%\bin\
xcopy /F "%QTDIR%\bin\*KF5*.dll" "%Destination%" || goto :error
xcopy /F "%Source%\dependencies\mingw64\bin\libphonon4qt5.dll" "%Destination%" || goto :error

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

xcopy /F "%OPENSSL_PATH%\%LIBCRYPTO%" "%Destination%" || goto :error
xcopy /F "%OPENSSL_PATH%\%LIBSSL%" "%Destination%" || goto :error

%QTDIR%\bin\windeployqt --version
%QTDIR%\bin\windeployqt --help
%QTDIR%\bin\windeployqt --release --no-system-d3d-compiler --no-angle --no-opengl-sw --no-quick^
    "%Destination%\libKF5ConfigCore.dll"^
    "%Destination%\libKF5Notifications.dll"^
    "%Executable%" || goto :error

7z a "%Name%.zip" -r "%Destination%" || goto :error

appveyor PushArtifact "%Name%.zip"
objdump -x "%Destination%\libKF5Notifications.dll" | findstr /C:"DLL Name"
objdump -x "%Build%\copyq.exe" | findstr /C:"DLL Name"

REM Note: Following removes system-installed dlls to verify required libs are included.
del C:\Windows\System32\libcrypto-* || goto :error
del C:\Windows\System32\libssl-* || goto :error
del C:\Windows\SysWOW64\libcrypto-* || goto :error
del C:\Windows\SysWOW64\libssl-* || goto :error
set OldPath=%PATH%
set PATH=%Destination%

set QT_FORCE_STDERR_LOGGING=1
set COPYQ_TESTS_RERUN_FAILED=1
"%Executable%" --help || goto :error
"%Executable%" --version || goto :error
"%Executable%" --info || goto :error
REM "%Executable%" tests || goto :error

REM Take a screen shot of the main window and notifications.
start "" "%Executable%"
"%Executable%" show || "%Executable%" show || "%Executable%" show || goto :error
"%Executable%" popup TEST MESSAGE || goto :error
"%Executable%" notification^
 .title Test^
 .message Message...^
 .button OK cmd data^
 .button Close cmd data || goto :error
"%Executable%" screenshot > screenshot.png || goto :error
"%Executable%" exit || goto :error
set PATH=%OldPath%
appveyor PushArtifact screenshot.png

choco install -y InnoSetup || goto :error
"C:\ProgramData\chocolatey\bin\ISCC.exe" "/O%APPVEYOR_BUILD_FOLDER%" "/DAppVersion=%AppVersion%" "/DRoot=%Destination%" "/DSource=%Source%" "%Source%\Shared\copyq.iss" || goto :error

:error
exit /b %errorlevel%
