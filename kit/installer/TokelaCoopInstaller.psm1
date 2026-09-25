# TokelaCoopInstaller.psm1 - the logic behind "Instalar TokelaCoop.cmd".
#
# Every function here is side-effect-light and parameterized on paths so
# scripts/tests/Installer.Tests.ps1 can drive it against fake Kenshi/Steam
# folders in CI. The interactive flow (prompts, RE_Kenshi's own installer,
# elevation) lives in Install-TokelaCoop.ps1.
#
# Windows PowerShell 5.1 compatible (what every player has): no ternary, no
# null-coalescing, no pipeline chain operators.

Set-StrictMode -Version 2

$script:KenshiAppId = '233860'

# RE_Kenshi builds this fork is checked against. The plugin's KenshiLib imports
# were verified present in both (CLAUDE.md, "Compatibilidad comprobada").
# Key = SHA-256 of <Kenshi>\KenshiLib.dll, which RE_Kenshi installs.
$script:KnownKenshiLib = @{
    '290570B31BCF690743BFBBF18EA9D16B02F01F594BE9DE727DB3D8101C3721C2' = '0.3.4'
    '5B6240622D06801DC39896E385EFB834B8E1FE2FCDF86067D071AE7A04F6C83B' = '0.3.5'
}

# The RE_Kenshi release the installer downloads when RE_Kenshi is missing.
$script:REKenshiRelease = @{
    Version = '0.3.5'
    Url     = 'https://github.com/BFrizzleFoShizzle/RE_Kenshi/releases/download/v0.3.5/RE_Kenshi_v0.3.5.zip'
    Sha256  = '291BBE13F83AB849A5227400E715CAB574B088C688856457A4B69A9AFC089260'
    Exe     = 'RE_Kenshi_v0.3.5.exe'
}

function Get-KcRelease { return $script:REKenshiRelease }

# ---- Messages ------------------------------------------------------------------

function Test-KcSpanish {
    try { return ([System.Globalization.CultureInfo]::CurrentUICulture.TwoLetterISOLanguageName -eq 'es') }
    catch { return $false }
}

# T 'texto' 'text': Spanish on a Spanish Windows, English otherwise.
function T([string]$es, [string]$en) {
    if (Test-KcSpanish) { return $es }
    return $en
}

# ---- Finding Kenshi --------------------------------------------------------------

# Library roots listed in a Steam libraryfolders.vdf (both the modern
# "path" "X" form and the legacy "1" "X" form). Backslashes are VDF-escaped.
function ConvertFrom-LibraryFoldersVdf([string]$Text) {
    $paths = @()
    if (-not $Text) { return $paths }
    foreach ($line in ($Text -split "`r?`n")) {
        $m = [regex]::Match($line, '^\s*"(path|\d+)"\s+"(.+)"\s*$')
        if (-not $m.Success) { continue }
        $v = $m.Groups[2].Value -replace '\\\\', '\'
        if ($m.Groups[1].Value -ne 'path' -and $v -notmatch '^[A-Za-z]:\\') { continue }
        $paths += $v
    }
    return $paths
}

function Get-SteamRoots {
    $roots = @()
    $keys = @(
        @{ Path = 'HKCU:\Software\Valve\Steam';              Name = 'SteamPath' },
        @{ Path = 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam';  Name = 'InstallPath' },
        @{ Path = 'HKLM:\SOFTWARE\Valve\Steam';              Name = 'InstallPath' }
    )
    foreach ($k in $keys) {
        try {
            $v = (Get-ItemProperty -Path $k.Path -Name $k.Name -ErrorAction Stop).($k.Name)
            if ($v) { $roots += ($v -replace '/', '\') }
        } catch {}
    }
    return $roots
}

# Every Steam library folder (the Steam root itself plus libraryfolders.vdf).
function Get-SteamLibraries([string[]]$SteamRoots) {
    $libs = @()
    foreach ($root in $SteamRoots) {
        if (-not $root) { continue }
        $libs += $root
        $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            $libs += ConvertFrom-LibraryFoldersVdf (Get-Content -LiteralPath $vdf -Raw)
        }
    }
    return $libs
}

function Test-KenshiDir([string]$Path) {
    if (-not $Path) { return $false }
    return (Test-Path -LiteralPath (Join-Path $Path 'kenshi_x64.exe'))
}

function Get-GogKenshiDirs {
    $dirs = @()
    foreach ($base in @('HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games', 'HKLM:\SOFTWARE\GOG.com\Games')) {
        try {
            foreach ($g in (Get-ChildItem -Path $base -ErrorAction Stop)) {
                $p = Get-ItemProperty -Path $g.PSPath -ErrorAction SilentlyContinue
                if ($p -and ($p.PSObject.Properties.Name -contains 'gameName') -and
                    $p.gameName -like '*Kenshi*' -and ($p.PSObject.Properties.Name -contains 'path')) {
                    $dirs += $p.path
                }
            }
        } catch {}
    }
    return $dirs
}

# All Kenshi installs found, Steam first, no duplicates. Parameters exist so the
# tests can inject fake roots; normal callers pass nothing.
function Find-KenshiInstalls {
    param(
        [string[]]$SteamRoots = $null,
        [string[]]$ExtraDirs  = $null,
        [switch]$NoRegistry
    )
    if ($null -eq $SteamRoots) {
        if ($NoRegistry) { $SteamRoots = @() } else { $SteamRoots = Get-SteamRoots }
    }
    $candidates = @()
    foreach ($lib in (Get-SteamLibraries $SteamRoots)) {
        $candidates += (Join-Path $lib 'steamapps\common\Kenshi')
    }
    if (-not $NoRegistry) {
        $candidates += Get-GogKenshiDirs
        $candidates += @(
            "${env:ProgramFiles(x86)}\Steam\steamapps\common\Kenshi",
            'C:\GOG Games\Kenshi',
            "${env:ProgramFiles(x86)}\GOG Galaxy\Games\Kenshi"
        )
    }
    if ($ExtraDirs) { $candidates += $ExtraDirs }

    $found = @()
    $seen = @{}
    foreach ($c in $candidates) {
        if (-not (Test-KenshiDir $c)) { continue }
        $full = (Resolve-Path -LiteralPath $c).ProviderPath.TrimEnd('\')
        $key = $full.ToLowerInvariant()
        if ($seen.ContainsKey($key)) { continue }
        $seen[$key] = $true
        $found += $full
    }
    return $found
}

# ---- RE_Kenshi -------------------------------------------------------------------

# Installed = RE_Kenshi.dll present; Enabled = Plugins_x64.cfg loads it;
# Version = known build (by KenshiLib.dll hash) or 'unknown'.
function Get-REKenshiState([string]$KenshiDir) {
    $dll = Join-Path $KenshiDir 'RE_Kenshi.dll'
    $cfg = Join-Path $KenshiDir 'Plugins_x64.cfg'
    $lib = Join-Path $KenshiDir 'KenshiLib.dll'
    $state = @{ Installed = $false; Enabled = $false; Version = 'unknown' }
    $state.Installed = (Test-Path -LiteralPath $dll)
    if (Test-Path -LiteralPath $cfg) {
        foreach ($line in (Get-Content -LiteralPath $cfg)) {
            if ($line.Trim() -match '^Plugin\s*=\s*RE_Kenshi\s*$') { $state.Enabled = $true; break }
        }
    }
    if (Test-Path -LiteralPath $lib) {
        $h = (Get-FileHash -Algorithm SHA256 -LiteralPath $lib).Hash.ToUpperInvariant()
        if ($script:KnownKenshiLib.ContainsKey($h)) { $state.Version = $script:KnownKenshiLib[$h] }
    }
    return $state
}

function Test-FileSha256([string]$Path, [string]$Expected) {
    $h = (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
    return ($h.ToUpperInvariant() -eq $Expected.ToUpperInvariant())
}

# Download + verify + extract the pinned RE_Kenshi release. Returns the path of
# its installer .exe. Throws on a hash mismatch (never runs an unverified exe).
function Get-REKenshiInstaller([string]$WorkDir) {
    $rel = $script:REKenshiRelease
    New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
    $zip = Join-Path $WorkDir ('RE_Kenshi_v' + $rel.Version + '.zip')
    if (-not ((Test-Path -LiteralPath $zip) -and (Test-FileSha256 $zip $rel.Sha256))) {
        [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072 # TLS 1.2
        $old = $ProgressPreference
        $ProgressPreference = 'SilentlyContinue' # the progress bar makes IWR ~10x slower on 5.1
        try { Invoke-WebRequest -Uri $rel.Url -OutFile $zip -UseBasicParsing }
        finally { $ProgressPreference = $old }
    }
    if (-not (Test-FileSha256 $zip $rel.Sha256)) {
        Remove-Item -LiteralPath $zip -Force -ErrorAction SilentlyContinue
        throw (T 'La descarga de RE_Kenshi no coincide con la huella esperada; no se ejecuta.' `
                 'The RE_Kenshi download does not match the expected hash; not running it.')
    }
    $out = Join-Path $WorkDir ('RE_Kenshi_v' + $rel.Version)
    if (Test-Path -LiteralPath $out) { Remove-Item -Recurse -Force -LiteralPath $out }
    Expand-Archive -LiteralPath $zip -DestinationPath $out -Force
    Get-ChildItem -LiteralPath $out -Recurse -File | Unblock-File -ErrorAction SilentlyContinue
    $exe = Join-Path $out $rel.Exe
    if (-not (Test-Path -LiteralPath $exe)) { throw "RE_Kenshi installer not found in the release zip: $($rel.Exe)" }
    return $exe
}

# ---- TokelaCoop files ---------------------------------------------------------------

# Up to v0.53 the mod was called KenshiCoop: folder mods\KenshiCoop, file
# KenshiCoop.dll, mods.cfg line KenshiCoop.mod. An upgrade must leave only
# TokelaCoop active - RE_Kenshi loads the plugin of every active mod, and two
# copies hook the same engine functions.
$script:LegacyName = 'KenshiCoop'

# Copy the kit's TokelaCoop folder into <Kenshi>\mods\TokelaCoop. The player's
# coop_config.json (it may hold LAN settings) wins over the kit's default: an
# existing mods\TokelaCoop one first, else the one from an old mods\KenshiCoop.
# Returns the destination folder.
function Install-TokelaCoopFiles([string]$KitModDir, [string]$KenshiDir) {
    $dst = Join-Path $KenshiDir 'mods\TokelaCoop'
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    $cfg = Join-Path $dst 'coop_config.json'
    $legacyCfg = Join-Path $KenshiDir ('mods\' + $script:LegacyName + '\coop_config.json')
    if (-not (Test-Path -LiteralPath $cfg) -and (Test-Path -LiteralPath $legacyCfg)) {
        Copy-Item -LiteralPath $legacyCfg -Destination $cfg -Force
    }
    foreach ($f in (Get-ChildItem -LiteralPath $KitModDir -File)) {
        $target = Join-Path $dst $f.Name
        if ($f.Name -eq 'coop_config.json' -and (Test-Path -LiteralPath $target)) { continue }
        Copy-Item -LiteralPath $f.FullName -Destination $target -Force
    }
    Get-ChildItem -LiteralPath $dst -File | Unblock-File -ErrorAction SilentlyContinue
    return $dst
}

# Make sure TokelaCoop.mod is in <Kenshi>\data\mods.cfg (Kenshi's active-mod
# list, one file name per line, load order) and the old KenshiCoop.mod is not.
# An old KenshiCoop.mod line is REPLACED in place, so the load order the mods
# check compares between the two players stays the same; otherwise TokelaCoop.mod
# is appended. Duplicates are dropped and the file's line endings kept. Returns
# $true if the file was changed.
function Enable-TokelaCoopMod([string]$KenshiDir, [string]$ModFile = 'TokelaCoop.mod') {
    $cfg = Join-Path $KenshiDir 'data\mods.cfg'
    $legacy = $script:LegacyName + '.mod'
    $lines = @()
    $nl = "`r`n"
    if (Test-Path -LiteralPath $cfg) {
        $raw = [System.IO.File]::ReadAllText($cfg)
        if ($raw -notmatch "`r`n" -and $raw -match "`n") { $nl = "`n" }
        $lines = @($raw -split "`r?`n" | Where-Object { $_ -ne '' })
    } else {
        New-Item -ItemType Directory -Force -Path (Split-Path $cfg) | Out-Null
    }
    $out = @()
    $have = $false
    $changed = $false
    foreach ($l in $lines) {
        $t = $l.Trim()
        if ($t -ieq $ModFile) {
            if ($have) { $changed = $true; continue }   # a second copy of the line
            $have = $true; $out += $l; continue
        }
        if ($t -ieq $legacy) {
            if (-not $have) { $out += $ModFile; $have = $true }   # same place in the order
            $changed = $true
            continue
        }
        $out += $l
    }
    if (-not $have) { $out += $ModFile; $changed = $true }
    if (-not $changed) { return $false }
    [System.IO.File]::WriteAllText($cfg, (($out -join $nl) + $nl), (New-Object System.Text.UTF8Encoding($false)))
    return $true
}

# Remove an old <Kenshi>\mods\KenshiCoop. Run it AFTER Enable-TokelaCoopMod, so
# that even if the removal fails the old mod is no longer active. Returns the
# removed folder, or '' when there was none. Throws if it cannot be removed.
function Remove-LegacyKenshiCoop([string]$KenshiDir) {
    $old = Join-Path $KenshiDir ('mods\' + $script:LegacyName)
    if (-not (Test-Path -LiteralPath $old)) { return '' }
    Remove-Item -LiteralPath $old -Recurse -Force
    return $old
}

# Steam Workshop copies of the mod next to this install, under either name
# (TokelaCoop.dll, or the old KenshiCoop.dll): RE_Kenshi would load the plugin
# twice. Returns the offending folders.
function Find-WorkshopTokelaCoop([string]$KenshiDir) {
    $ws = Join-Path $KenshiDir ('..\..\workshop\content\' + $script:KenshiAppId)
    $hits = @()
    if (Test-Path -LiteralPath $ws) {
        foreach ($d in (Get-ChildItem -LiteralPath $ws -Directory -ErrorAction SilentlyContinue)) {
            foreach ($dll in @('TokelaCoop.dll', ($script:LegacyName + '.dll'))) {
                if (Get-ChildItem -LiteralPath $d.FullName -Recurse -Filter $dll -ErrorAction SilentlyContinue) {
                    $hits += $d.FullName
                    break
                }
            }
        }
    }
    return $hits
}

function Test-KenshiRunning {
    return [bool](Get-Process -Name 'kenshi_x64', 'kenshi' -ErrorAction SilentlyContinue)
}

# Can we write into the Kenshi folder without elevation?
function Test-DirWritable([string]$Dir) {
    $probe = Join-Path $Dir ('.kc_write_test_' + [guid]::NewGuid().ToString('N'))
    try {
        [System.IO.File]::WriteAllText($probe, 'x')
        Remove-Item -LiteralPath $probe -Force
        return $true
    } catch { return $false }
}

Export-ModuleMember -Function *
