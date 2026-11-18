@echo off
rem DEMO 2 Binary Semaphore, Handler low priority
rem Результат: results\2_binary_lowprio.txt і скриншот results\2_binary_lowprio.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=100 lines=40
title Lab4 FreeRTOS - DEMO 2 Binary Semaphore, Handler low priority
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск...
call build.bat 2 > results\2_binary_lowprio.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\2_binary_lowprio.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\2_binary_lowprio.png
pause >nul
