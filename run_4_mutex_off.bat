@echo off
rem DEMO 4 console without mutex
rem Результат: results\4_mutex_off.txt і скриншот results\4_mutex_off.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=100 lines=40
title Lab4 FreeRTOS - DEMO 4 console without mutex
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск...
call build.bat 4 > results\4_mutex_off.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\4_mutex_off.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\4_mutex_off.png
pause >nul
