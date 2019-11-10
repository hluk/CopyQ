@echo on

%QTDIR%\bin\qmake --version

cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release^
 -G "%CMAKE_GENERATOR%" -A "%CMAKE_GENERATOR_ARCH%"^
 -DBOOST_ROOT="%BOOST_ROOT%"^
 -DWITH_TESTS=ON || goto :error

:error
exit /b %errorlevel%
