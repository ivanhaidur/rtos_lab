@echo off
rem Lab6: збірка, запуск, журнал results\lab6.txt і скриншоти
rem results\lab6_start.png (початок) та results\lab6_end.png (кінець)
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
chcp 65001 >nul
mode con: cols=112 lines=40
title Lab6 FreeRTOS - stream/message buffers, event groups (Haidur Ivan, variant 2)
cd /d "%~dp0"
if not exist results mkdir results
echo Збірка і запуск (близько 11 с)...
call build.bat > results\lab6.txt 2>&1
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\lab6.txt 38 head
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\lab6_start.png
cls
powershell -NoProfile -ExecutionPolicy Bypass -File show.ps1 results\lab6.txt 37
powershell -NoProfile -ExecutionPolicy Bypass -File shot.ps1 results\lab6_end.png
pause >nul
