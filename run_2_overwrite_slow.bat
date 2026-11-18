@echo off
rem eSetValueWithOverwrite, Control 450 ms
rem Результат: results\2_overwrite_slow.txt і скриншоти results\2_overwrite_slow*.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=105 lines=40
title Lab5 FreeRTOS - eSetValueWithOverwrite, Control 450 ms
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (близько 11 с)...
call build.bat 1 450 > results\2_overwrite_slow.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\2_overwrite_slow.txt 38 head
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\2_overwrite_slow_start.png
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\2_overwrite_slow.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\2_overwrite_slow.png
pause >nul
