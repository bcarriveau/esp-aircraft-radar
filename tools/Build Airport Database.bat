@echo off
setlocal
cd /d "%~dp0\.."

echo Starting Bill's Aircraft Radar regional airport package builder...
echo.
echo This creates release\airports.radarapt for the radar web uploader.
echo It does not rebuild firmware or change the radar's saved location.
echo.

where py >nul 2>nul
if not errorlevel 1 goto use_py

where python >nul 2>nul
if not errorlevel 1 goto use_python

echo ERROR: Python 3 was not found.
echo Install Python 3 or open a PlatformIO terminal and run:
echo     python tools\airport_database_setup.py
set "EXIT_CODE=1"
goto finished

:use_py
py -3 tools\airport_database_setup.py
set "EXIT_CODE=%errorlevel%"
goto finished

:use_python
python tools\airport_database_setup.py
set "EXIT_CODE=%errorlevel%"

:finished
echo.
if not "%EXIT_CODE%"=="0" echo Airport package builder did not complete.
pause
exit /b %EXIT_CODE%
