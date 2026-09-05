@echo off
REM Launcher script for ESP32QtApp
setlocal
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
cd /d "%~dp0build"
start "" "ESP32QtApp.exe" %*
endlocal
