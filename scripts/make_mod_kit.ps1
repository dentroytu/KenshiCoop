<#
.SYNOPSIS
  Package the PLAYER release: a single folder called "TokelaCoop" with the mod
  files inside, that a player copies straight into <Kenshi>\mods\. No install
  scripts, no launchers, no bundled save.

.DESCRIPTION
  Assembles dist\mod-kit\ as:
    TokelaCoop\                 <- the drop-in mod folder (copy this into mods\)
      TokelaCoop.dll              the plugin (protocol-version-matched; a mismatch
                                  is rejected at handshake by design)
      TokelaCoop.mod              mod-list entry so it shows in Kenshi's Mods menu
      RE_Kenshi.json              tells RE_Kenshi to load the plugin
      coop_config.json            only needed for LAN/direct-UDP; Steam play is
                                  configured entirely in-game (F2)
    Instalar TokelaCoop.cmd     <- one-click installer (runs installer\*.ps1)
    installer\                  <- Install-TokelaCoop.ps1 + TokelaCoopInstaller.psm1
    README.txt                  <- install/play instructions (NOT copied into
                                  mods, so it never clutters the game folder)
  ...then zips it to dist\TokelaCoop-kit.zip (the release artifact the README
  and the GitHub release point at).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\make_mod_kit.ps1

.EXAMPLE
  # Reuse the current build instead of recompiling.
  powershell -ExecutionPolicy Bypass -File scripts\make_mod_kit.ps1 -SkipBuild
#>
[CmdletBinding()]
param(
    [switch]$SkipBuild,
    # Where to find TokelaCoop.mod / RE_Kenshi.json if they aren't in dist\mods.
    [string]$HostDir = "C:\Program Files (x86)\Steam\steamapps\common\Kenshi"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Split-Path -Parent $scriptDir

if (-not $SkipBuild) {
    # The PLAYER release ships the Release config: the shipped DLL excludes the
    # scenario harness (~12k lines) and does not define TOKELACOOP_HARNESS
    # (Phase 1 build separation). The test pipeline uses Harness instead.
    Write-Host "=== build plugin (Release / shipped, no scenario harness) ==="
    & cmd.exe /c "`"$scriptDir\build_plugin.cmd`" Release"
    if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
}

# Resolve the four mod files from the first place each exists.
function Resolve-First([string[]]$candidates, [string]$what) {
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return $c } }
    throw "$what not found (looked in: $($candidates -join '; '))"
}
# Only the fresh Release build: an old tracked DLL used to be a fallback here and
# could ship a stale plugin under the new name without anyone noticing.
$dll  = Resolve-First @(
    (Join-Path $repoRoot "src\plugin\x64\Release\TokelaCoop.dll")
) "TokelaCoop.dll (build the Release config first)"

# The release version: src\netproto\Version.h is the one place it is written.
$verLine = Select-String -Path (Join-Path $repoRoot "src\netproto\Version.h") `
    -Pattern '#define\s+TOKELACOOP_VERSION\s+"([0-9]+\.[0-9]+)"' | Select-Object -First 1
if (-not $verLine) { throw "TOKELACOOP_VERSION not found in src\netproto\Version.h" }
$version = $verLine.Matches[0].Groups[1].Value
Write-Host "TokelaCoop version: v$version"

# Canonical shipped-DLL hash (Phase 1 provenance). Package from ONE DLL and
# assert the packaged copy is byte-identical to it, so the release artifact's
# SHA-256 is verifiable rather than a mutable file tracked under dist\.
$canonSha = (Get-FileHash -Algorithm SHA256 $dll).Hash
Write-Host "Canonical Release DLL SHA-256: $canonSha"
Write-Host "  source: $dll"
$json = Resolve-First @(
    (Join-Path $repoRoot "dist\mods\TokelaCoop\RE_Kenshi.json"),
    (Join-Path $HostDir  "mods\TokelaCoop\RE_Kenshi.json")
) "RE_Kenshi.json"
$mod  = Resolve-First @(
    (Join-Path $repoRoot "dist\mods\TokelaCoop\TokelaCoop.mod"),
    (Join-Path $HostDir  "mods\TokelaCoop\TokelaCoop.mod")
) "TokelaCoop.mod"

# Rebuild dist\mod-kit from scratch so no stale install script survives.
$kitDir  = Join-Path $repoRoot "dist\mod-kit"
$modDir  = Join-Path $kitDir "TokelaCoop"
if (Test-Path $kitDir) { Remove-Item -Recurse -Force $kitDir }
New-Item -ItemType Directory -Force -Path $modDir | Out-Null

Write-Host "=== assembling TokelaCoop mod folder ==="
Copy-Item $dll  (Join-Path $modDir "TokelaCoop.dll")
Copy-Item $json (Join-Path $modDir "RE_Kenshi.json")
Copy-Item $mod  (Join-Path $modDir "TokelaCoop.mod")

# The .mod description (Kenshi's mod info) names the version: stamp this build's.
# ModText.psm1 recomputes the record lengths and refuses to touch a StringId.
Import-Module (Join-Path $scriptDir "ModText.psm1") -Force
$kitMod = Join-Path $modDir "TokelaCoop.mod"
$desc = (Get-ModInfo $kitMod).Description
$stamped = [regex]::Replace($desc, 'TokelaCoop v[0-9]+\.[0-9]+', "TokelaCoop v$version")
if ($stamped -eq $desc -and $desc -notmatch [regex]::Escape("TokelaCoop v$version")) {
    throw "the .mod description has no 'TokelaCoop vX.YY' to stamp: $desc"
}
if ($stamped -ne $desc) { [void](Edit-ModFile -Path $kitMod -Description $stamped) }
Write-Host "  .mod description: $((Get-ModInfo $kitMod).Description.Substring(0, 40))..."

# coop_config.json (LAN/UDP only; Steam play needs no config). Written fresh so
# the release always ships a clean default.
@'
{
  // TokelaCoop config. For a normal Steam game you do NOT need to edit this file:
  // your friend's Steam ID is entered in-game (press F2, click "Copy my Steam ID"
  // to share yours, then "Paste friend's Steam ID" to enter theirs), and nothing
  // is written back to disk.
  //
  // This file only matters for a LAN / direct-UDP game: set "transport": "udp"
  // and put the host's address in "ip" (and "port" if you changed it). ip/port are
  // re-read each time you click Connect, so you can edit them without restarting.
  "transport": "steam",
  "ip": "127.0.0.1",
  "port": 27800,
  "autoConnect": false
}
'@ | Set-Content (Join-Path $modDir "coop_config.json") -Encoding UTF8

# Player-facing files that live next to the mod folder (NOT copied into the
# game): the README, the one-click installer launcher and its scripts. Sources
# are tracked under kit\ so they can be edited and tested (Installer.Tests.ps1).
$kitSrc = Join-Path $repoRoot "kit"
Copy-Item (Join-Path $kitSrc "README.txt") (Join-Path $kitDir "README.txt")
Copy-Item -LiteralPath (Join-Path $kitSrc "Instalar TokelaCoop.cmd") -Destination $kitDir
Copy-Item -Recurse (Join-Path $kitSrc "installer") (Join-Path $kitDir "installer")

# Provenance: assert the PACKAGED DLL is byte-identical to the canonical build,
# then record the hash next to the kit so the release artifact is verifiable.
$packagedDll = Join-Path $modDir "TokelaCoop.dll"
$packagedSha = (Get-FileHash -Algorithm SHA256 $packagedDll).Hash
if ($packagedSha -ne $canonSha) {
    throw "packaged DLL hash ($packagedSha) != canonical Release DLL hash ($canonSha)"
}
$protoLine = Select-String -Path (Join-Path $repoRoot "src\netproto\Wire.h") `
    -Pattern 'PROTOCOL_VERSION\s*=\s*(\d+)' | Select-Object -First 1
$proto = if ($protoLine) { $protoLine.Matches[0].Groups[1].Value } else { "?" }
@{
    name            = "TokelaCoop"
    version         = $version
    dllSha256       = $canonSha
    protocolVersion = $proto
    builtUtc        = (Get-Date).ToUniversalTime().ToString("o")
    config          = "Release"
} | ConvertTo-Json | Set-Content (Join-Path $kitDir "PROVENANCE.json") -Encoding UTF8
Write-Host "Packaged DLL SHA-256 verified == canonical."

# Zip: TokelaCoop\ + installer\ + the .cmd launcher + README.txt + PROVENANCE.json.
$zip = Join-Path $repoRoot "dist\TokelaCoop-kit.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path (Join-Path $kitDir "*") -DestinationPath $zip

Write-Host ""
Write-Host "Mod folder: $modDir"
Write-Host "Kit zipped: $zip"
Get-ChildItem -Recurse $kitDir | ForEach-Object {
    Write-Host ("  " + $_.FullName.Substring($kitDir.Length + 1))
}
