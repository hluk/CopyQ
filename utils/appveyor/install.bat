set PATH=%QTDIR%\bin;%PATH%
set CMAKE_PREFIX_PATH=%QTDIR%\lib\cmake

REM Note: Following removes sh.exe from PATH so that CMake can generate MinGW Makefile.
if NOT [%MINGW_PATH%] == [] set PATH=%MINGW_PATH%\bin;%PATH:C:\Program Files\Git\usr\bin;=%
if [%VC_VARS_ARCH%] == [] call "%VCINSTALLDIR%\vcvarsall.bat" %VC_VARS_ARCH%
