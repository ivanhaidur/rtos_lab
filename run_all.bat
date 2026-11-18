@echo off
rem Усі п'ять демонстрацій по черзі
cd /d "%~dp0"
start "" /wait conhost.exe cmd /c ""%~dp0run_1_binary.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_2_binary_lowprio.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_3_counting.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_4_mutex_off.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_5_mutex_on.bat" inner <nul"
