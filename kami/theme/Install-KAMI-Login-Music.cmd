@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-KAMI-Login-Music.ps1"
if errorlevel 1 (
  echo.
  echo Could not install the soundtrack. Check the message above.
  pause
)
endlocal
