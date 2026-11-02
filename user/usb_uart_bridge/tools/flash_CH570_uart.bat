@echo off
setlocal EnableExtensions
chcp 65001 >nul
title CH570 USB uart Download
pushd "%~dp0" || exit /b 1
set "RESULT=1"
py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3, 7) else 1)" >nul 2>nul
if not errorlevel 1 (
    py -3 "download.py" --chip CH570 --mode uart %*
    goto :result
)
python -c "import sys; sys.exit(0 if sys.version_info >= (3, 7) else 1)" >nul 2>nul
if not errorlevel 1 (
    python "download.py" --chip CH570 --mode uart %*
    goto :result
)
echo ERROR: Python 3.7 or newer is required.
goto :finish
:result
set "RESULT=%ERRORLEVEL%"
:finish
if not "%RESULT%"=="0" echo ERROR: Download failed, code %RESULT%.
popd
pause
endlocal & exit /b %RESULT%
