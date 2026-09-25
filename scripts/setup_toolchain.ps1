<#
.SYNOPSIS
  Install the local VC++2010 (v100) x64 toolchain needed to build TokelaCoop.dll:
  Windows SDK 7.1 + VC2010 SP1 compiler update (KB2519277) + the VS7 registry key
  MSBuild needs to find the v100 platform toolset.

.DESCRIPTION
  Mirrors .github/workflows/build.yml step-for-step (same installer URLs, same
  MSI subset, same registry fix) so a local build behaves identically to CI.
  Idempotent: skips the download if already cached under C:\toolchain-dl, and
  each MSI install is safe to re-run.

  Requires: VS2022 Build Tools already installed (for MSBuild.exe, located via
  vswhere) - this script only adds the legacy v100 compiler/SDK bits.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\setup_toolchain.ps1
#>
[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

$SDK71_ISO_URL   = "https://download.microsoft.com/download/F/1/0/F10113F5-B750-4969-A255-274341AC6BCE/GRMSDKX_EN_DVD.iso"
$KB2519277_URL   = "https://download.microsoft.com/download/7/5/0/75040801-126C-4591-BCE4-4CD1FD1499AA/VC-Compiler-KB2519277.exe"
$DL_DIR          = "C:\toolchain-dl"

New-Item -ItemType Directory -Force $DL_DIR | Out-Null

Write-Host "=== Download toolchain installers ==="
if (!(Test-Path "$DL_DIR\sdk71.iso")) {
    Write-Host "Downloading Windows SDK 7.1 ISO (~600MB) ..."
    curl.exe -sSfL -o "$DL_DIR\sdk71.iso" $SDK71_ISO_URL
} else { Write-Host "sdk71.iso already cached" }
if (!(Test-Path "$DL_DIR\kb2519277.exe")) {
    Write-Host "Downloading VC2010 SP1 compiler update (KB2519277) ..."
    curl.exe -sSfL -o "$DL_DIR\kb2519277.exe" $KB2519277_URL
} else { Write-Host "kb2519277.exe already cached" }
Get-ChildItem $DL_DIR | Format-Table Name, Length

Write-Host ""
Write-Host "=== Install Windows SDK 7.1 + VC2010 compiler update ==="

# SDK 7.1 setup refuses to install next to a newer VC2010 redistributable.
$keys = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
$redists = Get-ItemProperty $keys -ErrorAction SilentlyContinue |
    Where-Object { $_.DisplayName -match 'Visual C\+\+ 2010 .*Redistributable' }
foreach ($r in $redists) {
    Write-Host "Uninstalling $($r.DisplayName) $($r.PSChildName)"
    Start-Process msiexec.exe -ArgumentList "/x $($r.PSChildName) /qn /norestart" -Wait
}

$img = Mount-DiskImage -ImagePath "$DL_DIR\sdk71.iso" -PassThru
try {
    $drive = ($img | Get-Volume).DriveLetter + ':'
    # SDKSetup.exe is a .NET 2.0 app that exits immediately on modern Windows, so
    # install the component MSIs directly: VC2010 compilers + CRT, Windows
    # headers/libs + MSBuild v100 toolset, and the SDK tools (rc/mt).
    $msis = 'vc_stdx86\vc_stdx86.msi', 'vc_stdamd64\vc_stdamd64.msi',
            'WinSDKBuild_amd64\WinSDKBuild_amd64.msi', 'WinSDKTools_amd64\WinSDKTools_amd64.msi',
            'WinSDKWin32Tools_amd64\WinSDKWin32Tools_amd64.msi'
    foreach ($m in $msis) {
        $path = Join-Path "$drive\Setup" $m
        if (!(Test-Path $path)) { Get-ChildItem (Split-Path $path) | Format-Table Name; throw "missing $path" }
        $log = Join-Path $DL_DIR ([IO.Path]::GetFileNameWithoutExtension($m) + '.log')
        $p = Start-Process msiexec.exe -ArgumentList "/i `"$path`" /qn /norestart /l*v `"$log`"" -Wait -PassThru
        Write-Host "$m exit code: $($p.ExitCode)"
    }
} finally {
    Dismount-DiskImage -ImagePath "$DL_DIR\sdk71.iso" | Out-Null
}

$p = Start-Process "$DL_DIR\kb2519277.exe" -ArgumentList '/q', '/norestart' -Wait -PassThru
Write-Host "KB2519277 exit code: $($p.ExitCode)"

# Put the VC2010 redistributables back: removing them above is only needed while
# the SDK installs, and Kenshi's launcher is an MFC app that will not start
# without mfc100u.dll ("no se encontró mfc100u.dll"). Steam ships the official,
# Microsoft-signed installers with every Steamworks game.
$redistDir = "${env:ProgramFiles(x86)}\Steam\steamapps\common\Steamworks Shared\_CommonRedist\vcredist\2010"
foreach ($arch in 'x64', 'x86') {
    $exe = Join-Path $redistDir "vcredist_$arch.exe"
    if (Test-Path $exe) {
        $p = Start-Process $exe -ArgumentList '/q', '/norestart' -Wait -PassThru
        Write-Host "VC2010 redistributable $arch exit code: $($p.ExitCode)"
    } else {
        Write-Warning "Not found: $exe - reinstall 'Microsoft Visual C++ 2010 Redistributable' ($arch) by hand or Kenshi's launcher will not start."
    }
}

# Lets MSBuild locate the v100 toolset (same as tools/fix_vs7_registry.ps1).
$vs7 = 'HKLM:\SOFTWARE\Wow6432Node\Microsoft\VisualStudio\SxS\VS7'
if (!(Test-Path $vs7)) { New-Item -Path $vs7 -Force | Out-Null }
New-ItemProperty -Path $vs7 -Name '10.0' -Value 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\' -PropertyType String -Force | Out-Null

Write-Host ""
Write-Host "=== Verify ==="
$checks = 'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin\amd64\cl.exe',
          'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include\stdio.h',
          'C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\lib\amd64\msvcrt.lib',
          'C:\Program Files\Microsoft SDKs\Windows\v7.1\Include\windows.h',
          'C:\Program Files\Microsoft SDKs\Windows\v7.1\Lib\x64\kernel32.lib',
          'C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\x64\mt.exe',
          'C:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\x64\rc.exe',
          'C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\Platforms\x64\PlatformToolsets\v100',
          "$env:WINDIR\System32\mfc100u.dll"
$missing = @($checks | Where-Object { !(Test-Path $_) })
$checks | ForEach-Object { Write-Host ("{0}  {1}" -f $(if (Test-Path $_) { 'OK     ' } else { 'MISSING' }), $_) }
if ($missing.Count) {
    Get-ChildItem 'C:\Program Files (x86)\Microsoft Visual Studio 10.0', 'C:\Program Files\Microsoft SDKs\Windows', 'C:\Program Files (x86)\MSBuild\Microsoft.Cpp' -ErrorAction SilentlyContinue | Format-Table FullName
    throw "v100 toolchain incomplete"
}
# cl.exe prints its banner on stderr; through cmd so PowerShell 5.1 doesn't turn
# it into a terminating NativeCommandError under ErrorActionPreference=Stop.
cmd /c "`"$($checks[0])`" 2>&1" | Select-Object -First 1
Write-Host ""
Write-Host "RESULT: v100 toolchain ready."
