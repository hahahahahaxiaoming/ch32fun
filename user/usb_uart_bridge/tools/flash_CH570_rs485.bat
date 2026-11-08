@echo off
setlocal EnableExtensions
title CH570 USB RS485 APP Download
pushd "%~dp0..\..\.." || exit /b 1
set "RESULT=1"
set "APP=user\usb_uart_bridge\build\CH570\rs485\usb_rs485.bin"
if not exist "minichlink\minichlink.exe" (
    echo ERROR: minichlink.exe is missing.
    goto :finish
)
if not exist "%APP%" (
    echo ERROR: Missing %APP%. Run make rs485 in usb_uart_bridge.
    goto :finish
)
echo Writing RS485 APP. Only the target board should be in Boot mode.
"minichlink\minichlink.exe" -C b003boot -w "%APP%" 0x1000 -b
set "RESULT=%ERRORLEVEL%"
:finish
if not "%RESULT%"=="0" echo ERROR: Bridge upgrade failed, code %RESULT%.
popd
pause
endlocal & exit /b %RESULT%
