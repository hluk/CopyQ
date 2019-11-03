@echo on

%QTDIR%\bin\qmake --version

set kf5_build=%APPVEYOR_BUILD_FOLDER%\utils\appveyor\kf5_build.bat
set snoretoast_build=%APPVEYOR_BUILD_FOLDER%\utils\appveyor\snoretoast_build.bat

set KF5_FULLVER=%KF5_VERSION%.%KF5_PATCH%
set INSTALL_PREFIX=%APPVEYOR_BUILD_FOLDER%\usr\kf-%KF5_FULLVER%-%CMAKE_GENERATOR_ARCH%
set CMAKE_PREFIX=%INSTALL_PREFIX%\lib\cmake
set CMAKE_PREFIX_PATH=%CMAKE_PREFIX%;%INSTALL_PREFIX%\share\ECM\cmake
mkdir %INSTALL_PREFIX%

set PATH=%PATH%;%INSTALL_PREFIX%\bin

mkdir dependencies
cd dependencies || goto :error

if not exist %CMAKE_PREFIX%\libsnoretoast %snoretoast_build%
if not exist %INSTALL_PREFIX%\share\ECM %kf5_build% extra-cmake-modules
if not exist %CMAKE_PREFIX%\KF5Config %kf5_build% kconfig
if not exist %CMAKE_PREFIX%\KF5WindowSystem %kf5_build% kwindowsystem
if not exist %CMAKE_PREFIX%\KF5CoreAddons %kf5_build% kcoreaddons
if not exist %CMAKE_PREFIX%\KF5Notifications-ignore %kf5_build% knotifications

REM appveyor PushArtifact "%cd%\..\build\knotifications\CMakeFiles\CMakeError.log"
REM appveyor PushArtifact "%cd%\..\build\knotifications\CMakeFiles\CMakeOutput.log"

cd ..

cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release^
 -G "%CMAKE_GENERATOR%" -A "%CMAKE_GENERATOR_ARCH%"^
 -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"^
 -DCMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION=.^
 -DWITH_TESTS=ON || goto :error

:error
exit /b %errorlevel%
