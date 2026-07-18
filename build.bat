@echo off
setlocal

echo ==================================
echo        Building GameScript
echo ==================================

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found.
    echo Install CMake and add it to PATH.
    pause
    exit /b 1
)

if not exist build (
    mkdir build
)

cd build

echo.
echo Configuring project...

cmake .. -G "Visual Studio 18 2026" -A x64

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed.
    pause
    exit /b 1
)

echo.
echo Building Release...

cmake --build . --config Release

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed.
    pause
    exit /b 1
)

echo.
echo ==================================
echo Build completed successfully!
echo ==================================

pause