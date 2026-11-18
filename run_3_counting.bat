@echo off
rem DEMO 3 Counting Semaphore
rem Результат: results\3_counting.txt і скриншот results\3_counting.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=100 lines=40
title Lab4 FreeRTOS - DEMO 3 Counting Semaphore
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск...
call build.bat 3 > results\3_counting.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\3_counting.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\3_counting.png
pause >nul
