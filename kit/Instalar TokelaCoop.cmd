@echo off
rem TokelaCoop one-click installer: double-click this file.
rem Runs installer\Install-TokelaCoop.ps1 (plain text - open it to see what it does).
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\Install-TokelaCoop.ps1" %*
exit /b %errorlevel%
