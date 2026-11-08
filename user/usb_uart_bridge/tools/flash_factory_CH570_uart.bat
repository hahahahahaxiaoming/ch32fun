@echo off
setlocal EnableExtensions
title CH570 USB UART Factory Download - Bootloader + APP
if not "%~2"=="" goto :usage
set "PREVIEW="
if /i "%~1"=="-DryRun" set "PREVIEW=-DryRun"
if not "%~1"=="" if not defined PREVIEW goto :usage

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash_bootloader.ps1" -Mode uart %PREVIEW%
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" echo ERROR: Factory download failed, code %RESULT%.
pause
exit /b %RESULT%

:usage
echo Usage: %~nx0 [-DryRun]
pause
exit /b 2
