<#
.SYNOPSIS
  Set the TokelaCoop release version everywhere it is written, in one step.

.DESCRIPTION
  src\netproto\Version.h is the source of truth (the game shows it), but three
  tracked files carry a copy that the tests or players see:
    * src\netproto\Version.h                   #define TOKELACOOP_VERSION "X.YY"
    * dist\mods\TokelaCoop\TokelaCoop.mod      the description ("TokelaCoop vX.YY ...")
                                              - deploy.cmd installs this copy as is,
                                              and Installer.Tests checks it matches
    * tools\MultiplayerStartGen\Program.cs     the same description, for a regenerate
    * README.md                                "**Current version: vX.YY.**"
  The .mod is rewritten with ModText.psm1, which recomputes record lengths and
  refuses to touch a StringId. Then run the tests, commit, and tag vX.YY (the
  release job refuses a tag that is not "v" + Version.h's value).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\set_version.ps1 -Version 0.55
#>
[CmdletBinding()]
param([Parameter(Mandatory = $true)][ValidatePattern('^[0-9]+\.[0-9]+$')][string]$Version)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$enc = New-Object System.Text.UTF8Encoding($false)

# Rewrite a text file with one regex, keeping its bytes otherwise (BOM, CRLF).
function Set-InFile([string]$Rel, [string]$Pattern, [string]$Replacement) {
    $path = Join-Path $repoRoot $Rel
    $bytes = [IO.File]::ReadAllBytes($path)
    $bom = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
    $text = $enc.GetString($bytes, $(if ($bom) { 3 } else { 0 }), $bytes.Length - $(if ($bom) { 3 } else { 0 }))
    if ($text -notmatch $Pattern) { throw "${Rel}: pattern not found: $Pattern" }
    $new = [regex]::Replace($text, $Pattern, $Replacement)
    $out = $enc.GetBytes($new)
    if ($bom) { $out = [byte[]](0xEF, 0xBB, 0xBF) + $out }
    [IO.File]::WriteAllBytes($path, $out)
    Write-Host "  $Rel"
}

Write-Host "TokelaCoop v${Version}:"
Set-InFile 'src\netproto\Version.h' '#define TOKELACOOP_VERSION "[0-9]+\.[0-9]+"' "#define TOKELACOOP_VERSION `"$Version`""
Set-InFile 'tools\MultiplayerStartGen\Program.cs' '"TokelaCoop v[0-9]+\.[0-9]+ \(formerly' "`"TokelaCoop v$Version (formerly"
Set-InFile 'README.md' '\*\*Current version: v[0-9]+\.[0-9]+\.\*\*' "**Current version: v$Version.**"

Import-Module (Join-Path $repoRoot 'scripts\ModText.psm1') -Force
$mod = Join-Path $repoRoot 'dist\mods\TokelaCoop\TokelaCoop.mod'
$desc = (Get-ModInfo $mod).Description
if ($desc -notmatch 'TokelaCoop v[0-9]+\.[0-9]+') { throw "the .mod description has no 'TokelaCoop vX.YY': $desc" }
[void](Edit-ModFile -Path $mod -Description ([regex]::Replace($desc, 'TokelaCoop v[0-9]+\.[0-9]+', "TokelaCoop v$Version")))
Write-Host "  dist\mods\TokelaCoop\TokelaCoop.mod"
Write-Host "Next: run scripts\tests\Installer.Tests.ps1, commit, then tag v$Version."
