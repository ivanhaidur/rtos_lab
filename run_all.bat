@echo off
rem Усі три запуски по черзі
cd /d "%~dp0"
start "" /wait conhost.exe cmd /c ""%~dp0run_1_variant.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_2_overwrite_slow.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_3_without_overwrite_slow.bat" inner <nul"
