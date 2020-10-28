@echo on

cmake --build build/ --config Release || goto :error

:error
exit /b %errorlevel%
