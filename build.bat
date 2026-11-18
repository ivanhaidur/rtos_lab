@echo off
rem ============================================================
rem  Lab1 FreeRTOS - збірка й запуск через Visual Studio (MSVC)
rem
rem    build.bat              пріоритети за варіантом, TIME_SLICING=1
rem    build.bat 1 1          рівні пріоритети, TIME_SLICING=1
rem    build.bat 1 0          рівні пріоритети, TIME_SLICING=0
rem
rem  Префікс імен задач: set PREFIX=ABCD_ перед запуском
rem  Шлях до ядра:       set FREERTOS_KERNEL=...\Source
rem ============================================================
setlocal
chcp 65001 >nul

set EQ=0
set TS=1
if not "%~1"=="" set EQ=%~1
if not "%~2"=="" set TS=%~2
if "%PREFIX%"=="" set PREFIX=HAIV_
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
   /I. /I"%K%\include" /I"%K%\portable\MSVC-MingW" ^
   /DconfigUSE_TIME_SLICING=%TS% /DLAB_EQUAL_PRIORITIES=%EQ% /DLAB_PREFIX=\"%PREFIX%\" ^
   main.c "%K%\tasks.c" "%K%\queue.c" "%K%\list.c" ^
   "%K%\portable\MemMang\heap_4.c" "%K%\portable\MSVC-MingW\port.c" ^
   /Fobuild\ /Fdbuild\ /Febuild\lab1.exe /link winmm.lib >build\build.log
if errorlevel 1 (
    type build\build.log
    echo ПОМИЛКА ЗБІРКИ
    exit /b 1
)

build\lab1.exe
endlocal
