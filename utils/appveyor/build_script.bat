@echo on

cmake --build build/ || goto :error

:error
exit /b %errorlevel%
