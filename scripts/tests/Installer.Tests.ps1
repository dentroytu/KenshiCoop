<#
.SYNOPSIS
  Zero-game fixtures for the one-click installer (kit\installer). Builds fake
  Steam libraries / Kenshi folders under %TEMP% and drives the module's
  functions against them - no Kenshi, no Steam, no network.

.DESCRIPTION
  Covers what a wrong answer would break for a player:
    * libraryfolders.vdf parsing (modern + legacy) -> Kenshi on another drive
    * Find-KenshiInstalls: finds it in a secondary library, dedupes, ignores a
      folder without kenshi_x64.exe
    * Get-REKenshiState: missing / present-but-disabled / enabled, known build
      recognized by KenshiLib.dll hash, unknown otherwise
    * Install-TokelaCoopFiles: copies the mod, keeps an existing
      coop_config.json, updates the DLL
    * Enable-TokelaCoopMod: creates mods.cfg, appends once (idempotent,
      case-insensitive), keeps LF files LF
    * upgrade from KenshiCoop (the name up to v0.53): the old mods.cfg line is
      replaced in place, the old coop_config.json is carried over, the old
      mods\KenshiCoop folder is removed
    * Find-WorkshopTokelaCoop: flags a Workshop copy (either name) that would
      load twice
    * the pinned RE_Kenshi download refuses a file with the wrong SHA-256
  Also parses Install-TokelaCoop.ps1 so a syntax error fails CI.

  Exit code = number of failed assertions (0 = PASS), like Contract.Tests.ps1.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path   # scripts\tests
$repoRoot  = Split-Path -Parent (Split-Path -Parent $scriptDir)
$instDir   = Join-Path $repoRoot 'kit\installer'
Import-Module (Join-Path $instDir 'TokelaCoopInstaller.psm1') -Force -DisableNameChecking

$script:Pass = 0
$script:Fail = 0
function Check {
    param([string]$Name, [bool]$Cond)
    if ($Cond) { $script:Pass++; Write-Host "  ok   $Name" }
    else       { $script:Fail++; Write-Host "  FAIL $Name" }
}

$tmp = Join-Path $env:TEMP ('kc_installer_tests_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

function New-FakeKenshi([string]$Dir) {
    New-Item -ItemType Directory -Force -Path $Dir | Out-Null
    Set-Content -LiteralPath (Join-Path $Dir 'kenshi_x64.exe') -Value 'fake'
    return $Dir
}

try {
    Write-Host '== installer script parses =='
    $errs = $null
    [void][System.Management.Automation.Language.Parser]::ParseFile(
        (Join-Path $instDir 'Install-TokelaCoop.ps1'), [ref]$null, [ref]$errs)
    Check 'Install-TokelaCoop.ps1 has no parse errors' ($errs.Count -eq 0)
    Check 'kit launcher exists' (Test-Path -LiteralPath (Join-Path $repoRoot 'kit\Instalar TokelaCoop.cmd'))

    Write-Host '== libraryfolders.vdf =='
    $modern = @'
"libraryfolders"
{
	"0"
	{
		"path"		"C:\\Program Files (x86)\\Steam"
		"apps"
		{
			"228980"		"123"
		}
	}
	"1"
	{
		"path"		"E:\\SteamLibrary"
		"apps"
		{
			"233860"		"12345678"
		}
	}
}
'@
    $p = @(ConvertFrom-LibraryFoldersVdf $modern)
    Check 'modern vdf: two libraries' ($p.Count -eq 2)
    Check 'modern vdf: unescaped path' ($p -contains 'E:\SteamLibrary')
    Check 'modern vdf: app ids are not paths' (-not ($p -contains '12345678'))
    $legacy = "`"LibraryFolders`"`n{`n`t`"TimeNextStatsReport`"`t`t`"1600000000`"`n`t`"1`"`t`t`"D:\\Games\\Steam`"`n}"
    $p = @(ConvertFrom-LibraryFoldersVdf $legacy)
    Check 'legacy vdf: numbered library path' ($p.Count -eq 1 -and $p[0] -eq 'D:\Games\Steam')

    Write-Host '== Find-KenshiInstalls =='
    $steamRoot = Join-Path $tmp 'Steam'
    $lib2      = Join-Path $tmp 'Lib2'
    New-Item -ItemType Directory -Force -Path (Join-Path $steamRoot 'steamapps') | Out-Null
    $vdf = "`"libraryfolders`"`n{`n`t`"0`"`n`t{`n`t`t`"path`"`t`t`"" + ($steamRoot -replace '\\', '\\') +
           "`"`n`t}`n`t`"1`"`n`t{`n`t`t`"path`"`t`t`"" + ($lib2 -replace '\\', '\\') + "`"`n`t}`n}"
    Set-Content -LiteralPath (Join-Path $steamRoot 'steamapps\libraryfolders.vdf') -Value $vdf
    # Kenshi only in the secondary library; the main one has an empty Kenshi dir.
    New-Item -ItemType Directory -Force -Path (Join-Path $steamRoot 'steamapps\common\Kenshi') | Out-Null
    $k = New-FakeKenshi (Join-Path $lib2 'steamapps\common\Kenshi')
    $found = @(Find-KenshiInstalls -SteamRoots @($steamRoot) -NoRegistry)
    Check 'finds Kenshi in a secondary library' ($found.Count -eq 1 -and $found[0] -eq (Resolve-Path $k).ProviderPath)
    $found = @(Find-KenshiInstalls -SteamRoots @($steamRoot, $steamRoot) -ExtraDirs @($k) -NoRegistry)
    Check 'dedupes the same install' ($found.Count -eq 1)
    $found = @(Find-KenshiInstalls -SteamRoots @() -NoRegistry)
    Check 'nothing found -> empty' ($found.Count -eq 0)

    Write-Host '== Get-REKenshiState =='
    $s = Get-REKenshiState $k
    Check 'missing RE_Kenshi' (-not $s.Installed -and -not $s.Enabled -and $s.Version -eq 'unknown')
    Set-Content -LiteralPath (Join-Path $k 'RE_Kenshi.dll') -Value 'x'
    Set-Content -LiteralPath (Join-Path $k 'Plugins_x64.cfg') -Value "Plugin=RenderSystem_Direct3D11_x64`r`nPlugin=Plugin_ParticleFX_x64"
    $s = Get-REKenshiState $k
    Check 'present but disabled' ($s.Installed -and -not $s.Enabled)
    Set-Content -LiteralPath (Join-Path $k 'Plugins_x64.cfg') -Value "Plugin=RE_Kenshi`r`nPlugin=RenderSystem_Direct3D11_x64"
    Set-Content -LiteralPath (Join-Path $k 'KenshiLib.dll') -Value 'not a real KenshiLib'
    $s = Get-REKenshiState $k
    Check 'enabled, unknown build' ($s.Installed -and $s.Enabled -and $s.Version -eq 'unknown')

    Write-Host '== Install-TokelaCoopFiles =='
    $kitMod = Join-Path $tmp 'kit\TokelaCoop'
    New-Item -ItemType Directory -Force -Path $kitMod | Out-Null
    foreach ($n in 'TokelaCoop.dll', 'TokelaCoop.mod', 'RE_Kenshi.json') {
        Set-Content -LiteralPath (Join-Path $kitMod $n) -Value "v1 $n"
    }
    Set-Content -LiteralPath (Join-Path $kitMod 'coop_config.json') -Value '{ "transport": "steam" }'
    $dst = Install-TokelaCoopFiles $kitMod $k
    Check 'copied into mods\TokelaCoop' (Test-Path -LiteralPath (Join-Path $k 'mods\TokelaCoop\TokelaCoop.dll'))
    Check 'fresh install gets the default config' (Test-Path -LiteralPath (Join-Path $dst 'coop_config.json'))
    Set-Content -LiteralPath (Join-Path $dst 'coop_config.json') -Value '{ "transport": "udp", "ip": "10.0.0.2" }'
    Set-Content -LiteralPath (Join-Path $kitMod 'TokelaCoop.dll') -Value 'v2 dll'
    [void](Install-TokelaCoopFiles $kitMod $k)
    Check 'update replaces the DLL' ((Get-Content -LiteralPath (Join-Path $dst 'TokelaCoop.dll') -Raw).Trim() -eq 'v2 dll')
    Check 'update keeps the player''s coop_config.json' ((Get-Content -LiteralPath (Join-Path $dst 'coop_config.json') -Raw) -match '10\.0\.0\.2')

    Write-Host '== Enable-TokelaCoopMod =='
    $cfg = Join-Path $k 'data\mods.cfg'
    Check 'creates mods.cfg when missing' ((Enable-TokelaCoopMod $k) -and (Test-Path -LiteralPath $cfg))
    Check 'second run is a no-op' (-not (Enable-TokelaCoopMod $k))
    [System.IO.File]::WriteAllText($cfg, "Dark UI.mod`ntokelacoop.MOD`n")
    Check 'case-insensitive match is a no-op' (-not (Enable-TokelaCoopMod $k))
    [System.IO.File]::WriteAllText($cfg, "Dark UI.mod`nNice Map.mod`n")
    Check 'appends when absent' (Enable-TokelaCoopMod $k)
    $txt = [System.IO.File]::ReadAllText($cfg)
    Check 'keeps order, appends last' ($txt -eq "Dark UI.mod`nNice Map.mod`nTokelaCoop.mod`n")
    Check 'keeps LF line endings' ($txt -notmatch "`r")

    Write-Host '== upgrade from KenshiCoop (the name up to v0.53) =='
    # mods.cfg: the old line is replaced IN PLACE (load order kept), never both.
    [System.IO.File]::WriteAllText($cfg, "Dark UI.mod`r`nkenshicoop.MOD`r`nNice Map.mod`r`n")
    Check 'old KenshiCoop.mod line is a change' (Enable-TokelaCoopMod $k)
    $txt = [System.IO.File]::ReadAllText($cfg)
    Check 'old line replaced in the same place' ($txt -eq "Dark UI.mod`r`nTokelaCoop.mod`r`nNice Map.mod`r`n")
    Check 'second run after the swap is a no-op' (-not (Enable-TokelaCoopMod $k))
    [System.IO.File]::WriteAllText($cfg, "TokelaCoop.mod`nDark UI.mod`nKenshiCoop.mod`nTokelaCoop.mod`n")
    [void](Enable-TokelaCoopMod $k)
    Check 'both names listed -> only TokelaCoop, once, first place' `
        ([System.IO.File]::ReadAllText($cfg) -eq "TokelaCoop.mod`nDark UI.mod`n")

    # Files: the player's old config is carried over, then the old folder goes.
    $k2 = New-FakeKenshi (Join-Path $tmp 'Upgrade\Kenshi')
    $oldDir = Join-Path $k2 'mods\KenshiCoop'
    New-Item -ItemType Directory -Force -Path $oldDir | Out-Null
    foreach ($n in 'KenshiCoop.dll', 'KenshiCoop.mod', 'RE_Kenshi.json') {
        Set-Content -LiteralPath (Join-Path $oldDir $n) -Value "old $n"
    }
    Set-Content -LiteralPath (Join-Path $oldDir 'coop_config.json') -Value '{ "transport": "udp", "ip": "192.168.1.50" }'
    $dst2 = Install-TokelaCoopFiles $kitMod $k2
    Check 'old coop_config.json carried into mods\TokelaCoop' `
        ((Get-Content -LiteralPath (Join-Path $dst2 'coop_config.json') -Raw) -match '192\.168\.1\.50')
    Set-Content -LiteralPath (Join-Path $dst2 'coop_config.json') -Value '{ "transport": "udp", "ip": "10.9.9.9" }'
    [void](Install-TokelaCoopFiles $kitMod $k2)
    Check 'an existing TokelaCoop config beats the old one' `
        ((Get-Content -LiteralPath (Join-Path $dst2 'coop_config.json') -Raw) -match '10\.9\.9\.9')
    Check 'old folder removed' ((Remove-LegacyKenshiCoop $k2) -ne '' -and -not (Test-Path -LiteralPath $oldDir))
    Check 'nothing to remove the second time' ((Remove-LegacyKenshiCoop $k2) -eq '')
    Check 'the new install is untouched' (Test-Path -LiteralPath (Join-Path $dst2 'TokelaCoop.dll'))

    Write-Host '== Find-WorkshopTokelaCoop =='
    Check 'no Workshop copy' (@(Find-WorkshopTokelaCoop $k).Count -eq 0)
    $wsItem = Join-Path $lib2 'steamapps\workshop\content\233860\999'
    New-Item -ItemType Directory -Force -Path $wsItem | Out-Null
    Set-Content -LiteralPath (Join-Path $wsItem 'TokelaCoop.dll') -Value 'x'
    Check 'flags a Workshop copy' (@(Find-WorkshopTokelaCoop $k).Count -eq 1)
    $wsOld = Join-Path $lib2 'steamapps\workshop\content\233860\998'
    New-Item -ItemType Directory -Force -Path $wsOld | Out-Null
    Set-Content -LiteralPath (Join-Path $wsOld 'KenshiCoop.dll') -Value 'x'
    Check 'flags an old KenshiCoop Workshop copy too' (@(Find-WorkshopTokelaCoop $k).Count -eq 2)

    Write-Host '== shipped TokelaCoop.mod =='
    Import-Module (Join-Path $repoRoot 'scripts\ModText.psm1') -Force
    $shipped = Join-Path $repoRoot 'dist\mods\TokelaCoop\TokelaCoop.mod'
    $info = Get-ModInfo $shipped
    # Save identifiers: frozen under the name the records were first published with.
    $ids = @($info.Records | ForEach-Object { $_.StringId })
    $want = @(1..6 | ForEach-Object { "$_-KenshiCoop-MultiplayerStart.mod" })
    Check 'the 6 record StringIds are the frozen ones' (($ids -join '|') -eq ($want -join '|'))
    $names = @($info.Records | ForEach-Object { $_.Name })
    Check 'the two starts are named TokelaCoop' ($names -contains 'TokelaCoop (Wanderer x2)' -and $names -contains 'TokelaCoop+ (Wanderer x2)')
    $verLine = Select-String -Path (Join-Path $repoRoot 'src\netproto\Version.h') -Pattern '#define\s+TOKELACOOP_VERSION\s+"([0-9.]+)"'
    $ver = $verLine.Matches[0].Groups[1].Value
    Check "the description names TokelaCoop v$ver" ($info.Description -match [regex]::Escape("TokelaCoop v$ver"))
    $copy = Join-Path $tmp 'stamp.mod'
    Copy-Item -LiteralPath $shipped -Destination $copy
    [void](Edit-ModFile -Path $copy -Description ($info.Description -replace 'TokelaCoop v[0-9.]+', 'TokelaCoop v9.99'))
    $info2 = Get-ModInfo $copy
    Check 'stamping another version keeps every record and StringId' `
        (($info2.Records | ForEach-Object { $_.StringId }) -join '|' -eq ($want -join '|') -and $info2.Description -match 'TokelaCoop v9\.99')

    Write-Host '== pinned RE_Kenshi download =='
    $rel = Get-KcRelease
    Check 'pinned release has a URL, hash and exe' ($rel.Url -match '^https://github\.com/' -and $rel.Sha256.Length -eq 64 -and $rel.Exe)
    $bad = Join-Path $tmp 'bad.zip'
    Set-Content -LiteralPath $bad -Value 'tampered'
    Check 'hash check rejects a tampered file' (-not (Test-FileSha256 $bad $rel.Sha256))
} catch {
    $script:Fail++
    Write-Host "  FAIL unexpected error: $($_.Exception.Message)"
} finally {
    Remove-Item -Recurse -Force -LiteralPath $tmp -ErrorAction SilentlyContinue
}

Write-Host ''
Write-Host ("installer fixtures: {0}/{1} checks passed{2}" -f `
    $script:Pass, ($script:Pass + $script:Fail), $(if ($script:Fail) { ' - FAIL' } else { ' - PASS' }))
exit $script:Fail
