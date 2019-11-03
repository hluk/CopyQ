@echo on

curl -LO https://download.kde.org/stable/frameworks/%KF5_VERSION%/%1-%KF5_FULLVER%.zip || goto :error

cmake -E tar xf %1-%KF5_FULLVER%.zip --format=zip

cd %1-%KF5_FULLVER% || goto :error

for %%p in (%APPVEYOR_BUILD_FOLDER%\utils\appveyor\patches\%1\*.patch) do call :apply_patch %%p || goto :error

cmake -H. -B../build/%1 -DCMAKE_BUILD_TYPE=Release^
 -G "%CMAKE_GENERATOR%" -A "%CMAKE_GENERATOR_ARCH%"^
 -DKCONFIG_USE_GUI=OFF^
 -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"^
 -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" || goto :error

cmake --build ../build/%1 --config Release --target install || goto :error

cd ..
goto:eof

:error
exit /b %errorlevel%

:apply_patch
    C:\msys64\usr\bin\patch -p1 < %~1 || goto :error
goto:eof
