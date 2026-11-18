@echo off
rem QUEUE_LEN=1, Consumer > Producer
rem Результат: results\4_len1_cons_high.txt і скриншоти results\4_len1_cons_high*.png
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=100 lines=36
title Lab3 FreeRTOS Queues - QUEUE_LEN=1, Consumer ^> Producer
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (близько 7 с)...
call build.bat 1 1 2 > results\4_len1_cons_high.txt 2>&1
cls

powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\4_len1_cons_high.txt 33
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\4_len1_cons_high.png
pause >nul
