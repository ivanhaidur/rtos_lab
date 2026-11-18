@echo off
rem рівні пріоритети, TIME_SLICING=0
rem Відкривається в окремому вікні консолі; результат: results\3_equal_ts0.txt і results\3_equal_ts0.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=125 lines=36
title Lab1 FreeRTOS - рівні пріоритети, TIME_SLICING=0
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (4,2 с)...
call build.bat 1 0 > results\3_equal_ts0.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\3_equal_ts0.txt 33
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\3_equal_ts0.png
pause >nul
