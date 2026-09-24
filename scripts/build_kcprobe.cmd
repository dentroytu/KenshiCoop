@echo off
REM Build dist\kcprobe.exe - a fake KenshiCoop peer for testing the handshake
REM (version / session-full refusals, receive gate, goodbye) against a real
REM host on one PC (src\kcprobe). Test-only: not part of the player kit.
REM Same vendored+patched ENet sources and v100 toolchain as the plugin.
setlocal

set "REPO=%~dp0.."
pushd "%REPO%" >nul
set "REPO=%CD%"
popd >nul

set "VS10=C:\Program Files (x86)\Microsoft Visual Studio 10.0"
set "VC=%VS10%\VC"
set "SDK=C:\Program Files\Microsoft SDKs\Windows\v7.1"

set "PATH=%VC%\bin\amd64;%VC%\bin;%VS10%\Common7\IDE;%SDK%\Bin\x64;%SDK%\Bin;%PATH%"
set "INCLUDE=%VC%\include;%SDK%\Include;%REPO%\third_party\vc10_compat;%REPO%\third_party\enet\enet\include"
set "LIB=%VC%\lib\amd64;%SDK%\Lib\x64"

if not exist "%REPO%\dist" mkdir "%REPO%\dist"
if not exist "%REPO%\build\kcprobe" mkdir "%REPO%\build\kcprobe"

set "ENET=%REPO%\third_party\enet\enet"

echo === Building kcprobe.exe (Release^|x64, v100) ===
cl.exe /nologo /O2 /EHsc /W3 /DWIN32 ^
    /Fo"%REPO%\build\kcprobe\\" ^
    /Fe"%REPO%\dist\kcprobe.exe" ^
    "%REPO%\src\kcprobe\main.cpp" ^
    "%ENET%\callbacks.c" "%ENET%\compress.c" "%ENET%\host.c" "%ENET%\list.c" ^
    "%ENET%\packet.c" "%ENET%\peer.c" "%ENET%\protocol.c" "%ENET%\win32.c" ^
    ws2_32.lib winmm.lib
if errorlevel 1 (
    echo kcprobe build FAILED
    exit /b 1
)
echo kcprobe built: %REPO%\dist\kcprobe.exe
exit /b 0
