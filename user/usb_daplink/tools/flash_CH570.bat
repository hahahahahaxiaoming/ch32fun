@echo off
setlocal
cd /d "%~dp0.."
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\firmware.ps1
if errorlevel 1 goto done
..\..\minichlink\minichlink.exe -C b003boot -w build\CH570\usb_daplink.bin 0x1000 -b
:done
pause
