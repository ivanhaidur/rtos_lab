@echo off
rem QUEUE_LEN=10, Producer > Consumer
rem Результат: results\1_len10_prod_high.txt і скриншоти results\1_len10_prod_high*.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=100 lines=36
title Lab3 FreeRTOS Queues - QUEUE_LEN=10, Producer ^> Consumer
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (близько 7 с)...
call build.bat 10 2 1 > results\1_len10_prod_high.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\1_len10_prod_high.txt 34 head
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\1_len10_prod_high_start.png
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\1_len10_prod_high.txt 33
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\1_len10_prod_high.png
pause >nul
