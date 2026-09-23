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
    * Install-KenshiCoopFiles: copies the mod, keeps an existing
      coop_config.json, updates the DLL
    * Enable-KenshiCoopMod: creates mods.cfg, appends once (idempotent,
      case-insensitive), keeps LF files LF
    * Find-WorkshopKenshiCoop: flags a Workshop copy that would load twice
    * the pinned RE_Kenshi download refuses a file with the wrong SHA-256
  Also parses Install-KenshiCoop.ps1 so a syntax error fails CI.

  Exit code = number of failed assertions (0 = PASS), like Contract.Tests.ps1.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path   # scripts\tests
$repoRoot  = Split-Path -Parent (Split-Path -Parent $scriptDir)
$instDir   = Join-Path $repoRoot 'kit\installer'
Import-Module (Join-Path $instDir 'KenshiCoopInstaller.psm1') -Force -DisableNameChecking

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
        (Join-Path $instDir 'Install-KenshiCoop.ps1'), [ref]$null, [ref]$errs)
    Check 'Install-KenshiCoop.ps1 has no parse errors' ($errs.Count -eq 0)
    Check 'kit launcher exists' (Test-Path -LiteralPath (Join-Path $repoRoot 'kit\Instalar KenshiCoop.cmd'))

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

    Write-Host '== Install-KenshiCoopFiles =='
    $kitMod = Join-Path $tmp 'kit\KenshiCoop'
    New-Item -ItemType Directory -Force -Path $kitMod | Out-Null
    foreach ($n in 'KenshiCoop.dll', 'KenshiCoop.mod', 'RE_Kenshi.json') {
        Set-Content -LiteralPath (Join-Path $kitMod $n) -Value "v1 $n"
    }
    Set-Content -LiteralPath (Join-Path $kitMod 'coop_config.json') -Value '{ "transport": "steam" }'
    $dst = Install-KenshiCoopFiles $kitMod $k
    Check 'copied into mods\KenshiCoop' (Test-Path -LiteralPath (Join-Path $k 'mods\KenshiCoop\KenshiCoop.dll'))
    Check 'fresh install gets the default config' (Test-Path -LiteralPath (Join-Path $dst 'coop_config.json'))
    Set-Content -LiteralPath (Join-Path $dst 'coop_config.json') -Value '{ "transport": "udp", "ip": "10.0.0.2" }'
    Set-Content -LiteralPath (Join-Path $kitMod 'KenshiCoop.dll') -Value 'v2 dll'
    [void](Install-KenshiCoopFiles $kitMod $k)
    Check 'update replaces the DLL' ((Get-Content -LiteralPath (Join-Path $dst 'KenshiCoop.dll') -Raw).Trim() -eq 'v2 dll')
    Check 'update keeps the player''s coop_config.json' ((Get-Content -LiteralPath (Join-Path $dst 'coop_config.json') -Raw) -match '10\.0\.0\.2')

    Write-Host '== Enable-KenshiCoopMod =='
    $cfg = Join-Path $k 'data\mods.cfg'
    Check 'creates mods.cfg when missing' ((Enable-KenshiCoopMod $k) -and (Test-Path -LiteralPath $cfg))
    Check 'second run is a no-op' (-not (Enable-KenshiCoopMod $k))
    [System.IO.File]::WriteAllText($cfg, "Dark UI.mod`nkenshicoop.MOD`n")
    Check 'case-insensitive match is a no-op' (-not (Enable-KenshiCoopMod $k))
    [System.IO.File]::WriteAllText($cfg, "Dark UI.mod`nNice Map.mod`n")
    Check 'appends when absent' (Enable-KenshiCoopMod $k)
    $txt = [System.IO.File]::ReadAllText($cfg)
    Check 'keeps order, appends last' ($txt -eq "Dark UI.mod`nNice Map.mod`nKenshiCoop.mod`n")
    Check 'keeps LF line endings' ($txt -notmatch "`r")

    Write-Host '== Find-WorkshopKenshiCoop =='
    Check 'no Workshop copy' (@(Find-WorkshopKenshiCoop $k).Count -eq 0)
    $wsItem = Join-Path $lib2 'steamapps\workshop\content\233860\999'
    New-Item -ItemType Directory -Force -Path $wsItem | Out-Null
    Set-Content -LiteralPath (Join-Path $wsItem 'KenshiCoop.dll') -Value 'x'
    Check 'flags a Workshop copy' (@(Find-WorkshopKenshiCoop $k).Count -eq 1)

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
