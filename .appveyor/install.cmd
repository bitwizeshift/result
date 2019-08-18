SETLOCAL EnableDelayedExpansion

@REM  # Install 'conan'

echo "Downloading conan..."
set PATH=%PATH%;%PYTHON%/Scripts/
pip.exe install conan
conan user # Create the conan data directory
conan --version