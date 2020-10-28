@echo on

set bin=snoretoast-v%SNORETOAST_VERSION%
curl -LO https://invent.kde.org/libraries/snoretoast/-/archive/v%SNORETOAST_VERSION%/%bin%.zip || goto :error
cmake -E tar xf %bin%.zip --format=zip || goto :error

cd %bin% || goto :error

cmake -H. -B../build/%bin% -DCMAKE_BUILD_TYPE=Release^
 -G "%CMAKE_GENERATOR%" -A "%CMAKE_GENERATOR_ARCH%"^
 -DCMAKE_PREFIX_PATH="%CMAKE_PREFIX_PATH%"^
 -DCMAKE_INSTALL_PREFIX="%INSTALL_PREFIX%" || goto :error

cmake --build ../build/%bin% --config Release --target install || goto :error

cd ..
goto:eof

:error
exit /b %errorlevel%
