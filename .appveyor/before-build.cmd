SETLOCAL EnableDelayedExpansion

@REM  # Install 'conan' dependencies

mkdir build
cd build
conan install ..

@REM  # Generate the CMake project

cmake .. -A%PLATFORM% -DEXPECTED_COMPILE_UNIT_TESTS=On || exit /b !ERRORLEVEL!
