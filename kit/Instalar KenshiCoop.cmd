@echo off
rem KenshiCoop one-click installer: double-click this file.
rem Runs installer\Install-KenshiCoop.ps1 (plain text - open it to see what it does).
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\Install-KenshiCoop.ps1" %*
exit /b %errorlevel%
