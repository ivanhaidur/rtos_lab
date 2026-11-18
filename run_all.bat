@echo off
rem Усі чотири експерименти по черзі
cd /d "%~dp0"
start "" /wait conhost.exe cmd /c ""%~dp0run_1_len10_prod_high.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_2_len10_cons_high.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_3_len1_prod_high.bat" inner <nul"
start "" /wait conhost.exe cmd /c ""%~dp0run_4_len1_cons_high.bat" inner <nul"
