@echo off
rem variant 2, eSetValueWithOverwrite, Control 10 ms
rem Результат: results\1_variant.txt і скриншоти results\1_variant*.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=105 lines=40
title Lab5 FreeRTOS - variant 2, eSetValueWithOverwrite, Control 10 ms
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (близько 11 с)...
call build.bat 1 10 > results\1_variant.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\1_variant.txt 38 head
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\1_variant_start.png
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\1_variant.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\1_variant.png
pause >nul
