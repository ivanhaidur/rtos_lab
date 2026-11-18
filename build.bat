@echo off
rem ============================================================
rem  Lab4 FreeRTOS semaphores / mutex - збірка й запуск (MSVC)
rem
rem    build.bat [DEMO]
rem      1  Binary Semaphore, варіант 2 (Handler вищий за сенсори)
rem      2  Binary Semaphore, Handler нижчий за сенсори
rem      3  Counting Semaphore, пул ресурсів
rem      4  спільна консоль без м'ютекса
rem      5  спільна консоль з м'ютексом
rem
rem  Шлях до ядра:  set FREERTOS_KERNEL=...\Source
rem ============================================================
setlocal
chcp 65001 >nul

set DEMO=1
if not "%~1"=="" set DEMO=%~1
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
cl /nologo /utf-8 /W3 /Od /Zi /D_CRT_SECURE_NO_WARNINGS /DDEMO=%DEMO% ^
   /I. /I"%K%\include" /I"%K%\portable\MSVC-MingW" ^
   main.c "%K%\tasks.c" "%K%\queue.c" "%K%\list.c" ^
   "%K%\portable\MemMang\heap_4.c" "%K%\portable\MSVC-MingW\port.c" ^
   /Fobuild\ /Fdbuild\ /Febuild\lab4.exe /link winmm.lib >build\build.log
if errorlevel 1 (
    type build\build.log
    echo ПОМИЛКА ЗБІРКИ
    exit /b 1
)

build\lab4.exe
endlocal
