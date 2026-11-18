@echo off
rem DEMO 1 Binary Semaphore, variant 2
rem Результат: results\1_binary.txt і скриншот results\1_binary.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=100 lines=40
title Lab4 FreeRTOS - DEMO 1 Binary Semaphore, variant 2
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск...
call build.bat 1 > results\1_binary.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\1_binary.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\1_binary.png
pause >nul
