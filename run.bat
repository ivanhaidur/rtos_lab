@echo off
rem Lab2 FreeRTOS TLS: збірка, запуск, журнал results\lab2.txt і скриншоти
rem results\lab2_start.png (початок) та results\lab2_end.png (кінець)
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=110 lines=36
title Lab2 FreeRTOS TLS - Haidur Ivan, IDX=2
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (близько 8 с)...
call build.bat > results\lab2.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\lab2.txt 33 head
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\lab2_start.png
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\lab2.txt 33
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\lab2_end.png
pause >nul
