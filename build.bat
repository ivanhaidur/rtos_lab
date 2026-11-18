@echo off
rem ============================================================
rem  Lab5 FreeRTOS task notifications + timers - збірка й запуск (MSVC)
rem
rem    build.bat [OVERWRITE] [CONTROL_WORK_MS]
rem    build.bat            Overwrite, ControlTask 10 мс (варіант)
rem    build.bat 1 450      Overwrite, ControlTask 450 мс
rem    build.bat 0 450      WithoutOverwrite, ControlTask 450 мс
rem
rem  Шлях до ядра:  set FREERTOS_KERNEL=...\Source
rem ============================================================
setlocal
chcp 65001 >nul

set OW=1
set CW=10
if not "%~1"=="" set OW=%~1
if not "%~2"=="" set CW=%~2
if "%FREERTOS_KERNEL%"=="" set FREERTOS_KERNEL=..\..\Source
set K=%FREERTOS_KERNEL%

rem --- знайти Visual Studio з компілятором C++ ---
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (
    echo Visual Studio з компонентом "Desktop development with C++" не знайдено.
    exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul

if not exist build mkdir build
cl /nologo /utf-8 /W3 /Od /Zi /D_CRT_SECURE_NO_WARNINGS /DNOTIFY_OVERWRITE=%OW% /DCONTROL_WORK_MS=%CW% ^
   /I. /I"%K%\include" /I"%K%\portable\MSVC-MingW" ^
   main.c "%K%\tasks.c" "%K%\queue.c" "%K%\list.c" "%K%\timers.c" ^
   "%K%\portable\MemMang\heap_4.c" "%K%\portable\MSVC-MingW\port.c" ^
   /Fobuild\ /Fdbuild\ /Febuild\lab5.exe /link winmm.lib >build\build.log
if errorlevel 1 (
    type build\build.log
    echo ПОМИЛКА ЗБІРКИ
    exit /b 1
)

build\lab5.exe
endlocal
