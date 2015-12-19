SET Root="%APPVEYOR_BUILD_FOLDER%\build"
SET QtRoot="%CMAKE_PREFIX_PATH%"
SET Source="%APPVEYOR_BUILD_FOLDER%"
SET BuildConf=Release
SET DestDir=Bundle\copyq
SET WindowsRoot=C:\Windows\SysWOW64

mkdir "%DestDir%"
copy "%Root%\%BuildConf%\copyq.exe" "%DestDir%"

copy "%Source%\AUTHORS" "%DestDir%"
copy "%Source%\LICENSE" "%DestDir%"
copy "%Source%\README.md" "%DestDir%"

mkdir "%DestDir%\themes"
copy "%Source%\shared\themes\*" "%DestDir%\themes"

mkdir "%DestDir%\translations"
copy "%Root%\src\*.qm" "%DestDir%\translations"

mkdir "%DestDir%\plugins"
copy "%Root%\plugins\%BuildConf%\*.dll" "%DestDir%\plugins"

mkdir "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qgif.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qicns.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qico.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qjp2.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qjpeg.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qmng.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qsvg.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qtga.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qtiff.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qwbmp.dll" "%DestDir%\imageformats"
copy "%QtRoot%\plugins\imageformats\qwebp.dll" "%DestDir%\imageformats"

mkdir "%DestDir%\platforms"
copy "%QtRoot%\plugins\platforms\qwindows.dll" "%DestDir%\platforms"

copy "%QtRoot%\bin\Qt5Core.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Gui.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Network.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Script.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Svg.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Widgets.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Xml.dll" "%DestDir%"

copy "%QtRoot%\bin\Qt5Test.dll" "%DestDir%"

copy "%QtRoot%\bin\icudt53.dll" "%DestDir%"
copy "%QtRoot%\bin\icuin53.dll" "%DestDir%"
copy "%QtRoot%\bin\icuuc53.dll" "%DestDir%"

copy "%QtRoot%\bin\Qt5Multimedia.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5MultimediaWidgets.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5OpenGL.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Positioning.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5PrintSupport.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Qml.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Quick.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Sensors.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5Sql.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5WebChannel.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5WebKit.dll" "%DestDir%"
copy "%QtRoot%\bin\Qt5WebKitWidgets.dll" "%DestDir%"

copy "%WindowsRoot%\msvcp120.dll" "%DestDir%"
copy "%WindowsRoot%\msvcr120.dll" "%DestDir%"
