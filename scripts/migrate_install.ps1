<#
.SYNOPSIS
  Leave only TokelaCoop active in a Kenshi install: the dev twin of what the
  player installer does after copying the files (deploy.cmd runs it on the host
  install and on Kenshi-Join).

.DESCRIPTION
  Up to v0.53 the mod was KenshiCoop (mods\KenshiCoop, KenshiCoop.mod). RE_Kenshi
  loads the plugin of every ACTIVE mod, so an install that still lists
  KenshiCoop.mod would load the old DLL next to (or instead of) the new one.
    1. carry mods\KenshiCoop\coop_config.json into mods\TokelaCoop if it has none;
    2. data\mods.cfg: the KenshiCoop.mod line becomes TokelaCoop.mod in place
       (added at the end if neither is listed);
    3. delete mods\KenshiCoop.
  Uses the installer module, so this is the code Installer.Tests.ps1 covers.
  Exit code 0 = done, 1 = something could not be changed (Kenshi still open?).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\migrate_install.ps1 -KenshiDir "C:\Program Files (x86)\Steam\steamapps\common\Kenshi"
#>
[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$KenshiDir)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Import-Module (Join-Path $repoRoot 'kit\installer\TokelaCoopInstaller.psm1') -Force -DisableNameChecking

try {
    $dst = Join-Path $KenshiDir 'mods\TokelaCoop'
    $cfg = Join-Path $dst 'coop_config.json'
    $old = Join-Path $KenshiDir 'mods\KenshiCoop\coop_config.json'
    if (-not (Test-Path -LiteralPath $cfg) -and (Test-Path -LiteralPath $old)) {
        New-Item -ItemType Directory -Force -Path $dst | Out-Null
        Copy-Item -LiteralPath $old -Destination $cfg
        Write-Host "Carried coop_config.json over from mods\KenshiCoop"
    }
    if (Enable-TokelaCoopMod $KenshiDir) { Write-Host "data\mods.cfg: TokelaCoop.mod enabled (KenshiCoop.mod removed)" }
    $kept = Save-LegacyConfig $KenshiDir
    if ($kept) { Write-Host "The old coop_config.json differed; kept it as $kept" }
    $gone = Remove-LegacyKenshiCoop $KenshiDir
    if ($gone) { Write-Host "Removed the old $gone" }
    exit 0
} catch {
    Write-Host "ERROR: could not switch '$KenshiDir' to TokelaCoop: $($_.Exception.Message)"
    Write-Host "       Close every Kenshi and retry."
    exit 1
}
