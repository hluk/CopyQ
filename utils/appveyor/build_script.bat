@echo on

cmake --build build/ --config %BUILD_CONFIGURATION%
exit /b %errorlevel%
