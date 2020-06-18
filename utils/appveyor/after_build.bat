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
%QTDIR%\bin\windeployqt --release --no-system-d3d-compiler --no-angle --no-opengl-sw "%Executable%" || goto :error

7z a "%Name%.zip" -r "%Destination%" || goto :error

choco install -y InnoSetup || goto :error
"C:\ProgramData\chocolatey\bin\ISCC.exe" "/O%APPVEYOR_BUILD_FOLDER%" "/DAppVersion=%AppVersion%" "/DRoot=%Destination%" "/DSource=%Source%" "%Source%\Shared\copyq.iss" || goto :error

REM Note: Following removes system-installed dlls to verify required libs are included.
del C:\Windows\System32\libcrypto-* || goto :error
del C:\Windows\System32\libssl-* || goto :error
del C:\Windows\SysWOW64\libcrypto-* || goto :error
del C:\Windows\SysWOW64\libssl-* || goto :error
set PATH=%Destination%

set QT_FORCE_STDERR_LOGGING=1
set COPYQ_TESTS_RERUN_FAILED=1
"%Executable%" --help || goto :error
"%Executable%" --version || goto :error
"%Executable%" --info || goto :error
"%Executable%" tests || goto :error

:error
exit /b %errorlevel%
