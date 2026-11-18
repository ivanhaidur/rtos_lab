@echo off
rem ============================================================
rem  Lab3 FreeRTOS Queues - збірка й запуск через Visual Studio (MSVC)
rem
rem    build.bat [QUEUE_LEN] [PROD_PRIO] [CONS_PRIO]
rem    build.bat              довжина 10, Producer(2) > Consumer(1)
rem    build.bat 10 1 2       Consumer > Producer
rem    build.bat 1 2 1        черга на 1 елемент
rem
rem  Шлях до ядра:  set FREERTOS_KERNEL=...\Source
rem ============================================================
setlocal
chcp 65001 >nul

set QLEN=10
set PP=2
set CP=1
if not "%~1"=="" set QLEN=%~1
if not "%~2"=="" set PP=%~2
if not "%~3"=="" set CP=%~3
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
cl /nologo /utf-8 /W3 /Od /Zi /D_CRT_SECURE_NO_WARNINGS ^
   /DQUEUE_LEN=%QLEN% /DPROD_PRIO=%PP% /DCONS_PRIO=%CP% ^
   /I. /I"%K%\include" /I"%K%\portable\MSVC-MingW" ^
   main.c "%K%\tasks.c" "%K%\queue.c" "%K%\list.c" ^
   "%K%\portable\MemMang\heap_4.c" "%K%\portable\MSVC-MingW\port.c" ^
   /Fobuild\ /Fdbuild\ /Febuild\lab3.exe /link winmm.lib >build\build.log
if errorlevel 1 (
    type build\build.log
    echo ПОМИЛКА ЗБІРКИ
    exit /b 1
)

build\lab3.exe
endlocal
