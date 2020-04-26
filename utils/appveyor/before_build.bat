@echo on

%QTDIR%\bin\qmake --version
cmake -H. -Bbuild -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release -G "%CMAKE_GENERATOR%" -A "%CMAKE_GENERATOR_ARCH%"
