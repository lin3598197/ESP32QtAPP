@echo off
REM Launcher script for ESP32QtApp
taskkill /F /IM ESP32QtApp.exe >nul 2>&1
timeout /t 1 /nobreak >nul
setlocal
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
cd /d "%~dp0build"
start "" "ESP32QtApp.exe" %*
endlocal
