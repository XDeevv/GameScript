@echo off
setlocal

echo ==================================
echo        Building GameScript
echo ==================================


if "%~1"=="-d" (
    echo Removing build directory...
    if exist build (
        rmdir /s /q build
    )
)


where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake not found.
    echo Install CMake and add it to PATH.
    pause
    exit /b 1
)

set VS_GENERATOR=

echo Detecting Visual Studio...

cmake --help | findstr /C:"Visual Studio 18 2026" >nul
if %ERRORLEVEL% EQU 0 (
    set VS_GENERATOR=Visual Studio 18 2026
    echo Found Visual Studio 2026
) else (
    cmake --help | findstr /C:"Visual Studio 17 2022" >nul
    if %ERRORLEVEL% EQU 0 (
        set VS_GENERATOR=Visual Studio 17 2022
        echo Found Visual Studio 2022
    )
)

if "%VS_GENERATOR%"=="" (
    echo ERROR: No supported Visual Studio installation found.
    echo Required:
    echo   - Visual Studio 2026
    echo   - Visual Studio 2022
    pause
    exit /b 1
)

if not exist build (
    mkdir build
)

cd build

echo.
echo Using generator:
echo %VS_GENERATOR%

echo.
echo Configuring project...

cmake .. -G "%VS_GENERATOR%" -A x64

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

echo Exiting ...