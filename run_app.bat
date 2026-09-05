@echo off
REM Launcher script for ESP32QtApp
taskkill /F /IM ESP32QtApp.exe >nul 2>&1
timeout /t 1 /nobreak >nul

if not exist "%~dp0build\ESP32QtApp.exe" (
    echo =========================================================
    echo [提示] 尚未偵測到編譯執行檔 (build\ESP32QtApp.exe)！
    echo 這通常是因為剛從 GitHub clone 專案。
    echo 正在為您自動執行 build.bat 進行編譯...
    echo =========================================================
    call "%~dp0build.bat"
    if not exist "%~dp0build\ESP32QtApp.exe" (
        echo [錯誤] 編譯未完成，無法啟動。
        pause
        exit /b 1
    )
)

setlocal
if exist "C:\msys64\ucrt64\bin\cmake.exe" (
    set "PATH=C:\msys64\ucrt64\bin;%PATH%"
)
cd /d "%~dp0build"
start "" "ESP32QtApp.exe" %*
endlocal
