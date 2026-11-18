@echo off
rem пріоритети за варіантом, TIME_SLICING=1
rem Відкривається в окремому вікні консолі; результат: results\1_variant_ts1.txt і results\1_variant_ts1.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=125 lines=36
title Lab1 FreeRTOS - пріоритети за варіантом, TIME_SLICING=1
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (4,2 с)...
call build.bat 0 1 > results\1_variant_ts1.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\1_variant_ts1.txt 33
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\1_variant_ts1.png
pause >nul
