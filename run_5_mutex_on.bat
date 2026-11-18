@echo off
rem DEMO 5 console with mutex
rem Результат: results\5_mutex_on.txt і скриншот results\5_mutex_on.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=100 lines=40
title Lab4 FreeRTOS - DEMO 5 console with mutex
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск...
call build.bat 5 > results\5_mutex_on.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\5_mutex_on.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\5_mutex_on.png
pause >nul
