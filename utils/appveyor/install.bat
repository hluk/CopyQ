@echo on

set PATH=%QTDIR%\bin;%PATH%
set CMAKE_PREFIX_PATH=%QTDIR%\lib\cmake

dir %QTDIR%\bin || goto :error
dir %QTDIR%\lib\cmake || goto :error

dir "%OPENSSL_PATH%" || goto :error
dir "%OPENSSL_PATH%\%LIBCRYPTO%" || goto :error
dir "%OPENSSL_PATH%\%LIBSSL%" || goto :error

REM Note: Following removes sh.exe from PATH so that CMake can generate MinGW Makefile.
PATH=%MINGW_PATH%\bin;%PATH:C:\Program Files\Git\usr\bin;=%

:error
exit /b %errorlevel%
