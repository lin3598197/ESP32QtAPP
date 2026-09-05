@echo off
setlocal
echo ===================================================
echo   ESP32QtApp One-Click Build Script
echo ===================================================

REM 1. Set up compiler and Qt paths if MSYS2 exists
if exist "C:\msys64\ucrt64\bin\cmake.exe" (
    set "PATH=C:\msys64\ucrt64\bin;%PATH%"
)

REM 2. Check for cmake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found! Please install CMake and Qt 6.
    pause
    exit /b 1
)

REM 3. Configure and build
echo [1/3] Configuring CMake project...
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    pause
    exit /b 1
)

echo [2/3] Building ESP32QtApp...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

REM 4. Deploy Qt DLLs if windeployqt exists
echo [3/3] Deploying Qt runtime libraries...
where windeployqt6 >nul 2>&1
if %ERRORLEVEL% equ 0 (
    windeployqt6 --compiler-runtime --no-translations "%~dp0build\ESP32QtApp.exe" >nul 2>&1
) else (
    where windeployqt >nul 2>&1
    if %ERRORLEVEL% equ 0 (
        windeployqt --compiler-runtime --no-translations "%~dp0build\ESP32QtApp.exe" >nul 2>&1
    )
)

echo ===================================================
echo [SUCCESS] Build completed! You can now run run_app.bat
echo ===================================================
endlocal
