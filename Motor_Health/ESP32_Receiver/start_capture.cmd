@echo off
setlocal
cd /d "%~dp0"
if errorlevel 1 (
    echo ERROR: Could not open the Demo project directory.
    pause
    exit /b 1
)

set "CAPTURE_PYTHON=C:\Espressif\tools\python\v6.1\venv\Scripts\python.exe"
if not exist "%CAPTURE_PYTHON%" (
    echo ERROR: Python interpreter not found: "%CAPTURE_PYTHON%"
    pause
    exit /b 1
)
if not exist "scripts\capture_csv.py" (
    echo ERROR: Capture script not found: "%CD%\scripts\capture_csv.py"
    pause
    exit /b 1
)

echo Close ESP-IDF Monitor before using COM6.
echo CSV files will be saved in "%CD%\captures".
echo Enter commanded duty, test condition and run ID when prompted.
echo Declared duty stays fixed for this run; it does not control the STM32.
echo Press Ctrl+C to stop recording and close the CSV file.
echo.
"%CAPTURE_PYTHON%" "scripts\capture_csv.py" --port COM6 --baud 115200
set "CAPTURE_EXIT=%ERRORLEVEL%"
if not "%CAPTURE_EXIT%"=="0" (
    echo.
    echo ERROR: Capture exited with code %CAPTURE_EXIT%.
    echo Review the error above. If COM6 is busy, close ESP-IDF Monitor or other serial applications.
)
echo.
pause
exit /b %CAPTURE_EXIT%
