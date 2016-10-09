set Root="%APPVEYOR_BUILD_FOLDER%\build"
set QtRoot="%QTDIR%"
set Source="%APPVEYOR_BUILD_FOLDER%"
set BuildConf=Release
set DestDir=Bundle\copyq

mkdir "%DestDir%"
xcopy /F "%Root%\%BuildConf%\copyq.exe" "%DestDir%"

xcopy /F "%Source%\AUTHORS" "%DestDir%"
xcopy /F "%Source%\LICENSE" "%DestDir%"
xcopy /F "%Source%\README.md" "%DestDir%"

mkdir "%DestDir%\themes"
xcopy /F "%Source%\shared\themes\*" "%DestDir%\themes"

mkdir "%DestDir%\translations"
xcopy /F "%Root%\src\*.qm" "%DestDir%\translations"

mkdir "%DestDir%\plugins"
xcopy /F "%Root%\plugins\%BuildConf%\*.dll" "%DestDir%\plugins"

%QTDIR%\bin\windeployqt --no-translations --no-compiler-runtime --no-system-d3d-compiler --no-angle --no-opengl-sw %DestDir%\copyq.exe
