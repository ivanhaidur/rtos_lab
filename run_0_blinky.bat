@echo off
if not "%~1"=="inner" (
    start "" conhost.exe cmd /c ""%~f0" inner"
    exit /b
)
rem Збірка та запуск демо main_blinky (WIN32-MSVC, mainCREATE_SIMPLE_BLINKY_DEMO_ONLY = 1)
chcp 65001 >nul
mode con: cols=100 lines=30
title FreeRTOS WIN32-MSVC - main_blinky
set LAB=%~dp0
if not exist "%LAB%results" mkdir "%LAB%results"
cd /d "%LAB%..\WIN32-MSVC"
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set MSB="%%i"
echo Збірка WIN32.sln ...
%MSB% WIN32.sln /m /nologo /v:minimal /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145 /p:WindowsTargetPlatformVersion=10.0 > "%LAB%results\0_blinky_build.txt" 2>&1
if errorlevel 1 (
    type "%LAB%results\0_blinky_build.txt"
    echo ПОМИЛКА ЗБІРКИ
    pause
    exit /b 1
)
cls
echo FreeRTOS WIN32-MSVC demo: main_blinky (працює 8 с)
start /b "" x64\Debug\RTOSDemo.exe
powershell -NoProfile -Command "Start-Sleep -Seconds 8"
powershell -NoProfile -ExecutionPolicy Bypass -File "%LAB%shot.ps1" "%LAB%results\0_blinky.png"
taskkill /im RTOSDemo.exe /f >nul 2>&1
pause >nul
