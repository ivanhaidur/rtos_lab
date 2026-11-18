@echo off
rem Усі експерименти по черзі
cd /d "%~dp0"
start "" /wait conhost.exe cmd /c ""%~dp0run_0_blinky.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_1_variant.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_2_equal_ts1.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_3_equal_ts0.bat" inner <nul"
