SETLOCAL EnableDelayedExpansion

@REM  # Test the project

cd build

ctest -j 4 -C %CONFIGURATION% || exit /b !ERRORLEVEL!