@echo on

%QTDIR%\bin\qmake --version

set KF5_FULLVER=%KF5_VERSION%.%KF5_PATCH%

mkdir dependencies
cd dependencies || goto :error

call :phonon_get || goto :error

call :kf5_build extra-cmake-modules || goto :error

call :kf5_build kconfig || goto :error

choco install -y gperf || goto :error

call :kf5_build kwindowsystem || goto :error
call :kf5_build kcoreaddons || goto :error
call :kf5_build knotifications || goto :error

cd ..

cmake -H. -Bbuild -DCMAKE_BUILD_TYPE=Release^
 -G "%CMAKE_GENERATOR%" -A "%CMAKE_GENERATOR_ARCH%"^
 -DWITH_TESTS=ON || goto :error

:error
exit /b %errorlevel%

:phonon_get
    curl -LO %MSYS_BASE_URL%/%PHONON%.pkg.tar.xz || goto :error
    cmake -E tar xvf %PHONON%.pkg.tar.xz || goto :error
exit /b 0

:kf5_build
    curl -LO https://download.kde.org/stable/frameworks/%KF5_VERSION%/%~1-%KF5_FULLVER%.zip || goto :error

    cmake -E tar xvf %~1-%KF5_FULLVER%.zip --format=zip

    cd %~1-%KF5_FULLVER% || goto :error

    for %%p in (%APPVEYOR_BUILD_FOLDER%\utils\appveyor\%~1-*.patch) do call :apply_patch %%p || goto :error

    cmake -H. -B../build/%~1 -DCMAKE_BUILD_TYPE=Release^
     -G "%CMAKE_GENERATOR%" -A "%CMAKE_GENERATOR_ARCH%"^
     -DKCONFIG_USE_GUI=OFF^
     -DCMAKE_PREFIX_PATH="%APPVEYOR_BUILD_FOLDER%\dependencies\mingw64\lib\cmake"^
     -DCMAKE_INSTALL_PREFIX="%QTDIR%" || goto :error

    cmake --build ../build/%~1 --target install || goto :error

    cd ..
exit /b 0

:apply_patch
    C:\msys64\usr\bin\patch -p1 < %~1 || goto :error
exit /b 0
