<#
.SYNOPSIS
  One-click TokelaCoop install: finds Kenshi, makes sure RE_Kenshi is there,
  copies the mod and turns it on. Run through "Instalar TokelaCoop.cmd".

.DESCRIPTION
  1. Find Kenshi (every Steam library, GOG, or ask for the folder).
  2. RE_Kenshi: if missing, download the pinned release from GitHub, check its
     SHA-256 and open its official installer (the only supported way on Kenshi
     1.0.68) with the Kenshi folder already on the clipboard.
  3. Copy the mod into <Kenshi>\mods\TokelaCoop (keeping an existing
     coop_config.json, or carrying over the one from an old mods\KenshiCoop)
     and unblock the files.
  4. Enable TokelaCoop.mod in <Kenshi>\data\mods.cfg; an old KenshiCoop.mod line
     (the name up to v0.53) is replaced in place.
  5. Remove an old mods\KenshiCoop, so the old and new plugin never load together.
  6. Warn about a Workshop copy of the mod under either name (would load twice).
  Re-running it updates the mod in place.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File installer\Install-TokelaCoop.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File installer\Install-TokelaCoop.ps1 -KenshiPath "D:\Games\Kenshi"
#>
[CmdletBinding()]
param(
    [string]$KenshiPath = '',
    [switch]$SkipREKenshi,
    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'
$here   = Split-Path -Parent $MyInvocation.MyCommand.Path
$kitDir = Split-Path -Parent $here
Import-Module (Join-Path $here 'TokelaCoopInstaller.psm1') -Force -DisableNameChecking

function Step([string]$es, [string]$en) { Write-Host ''; Write-Host ('>> ' + (T $es $en)) -ForegroundColor Cyan }
function Ok([string]$es, [string]$en)   { Write-Host ('   [OK] ' + (T $es $en)) -ForegroundColor Green }
function Warn([string]$es, [string]$en) { Write-Host ('   [!]  ' + (T $es $en)) -ForegroundColor Yellow }
function Info([string]$es, [string]$en) { Write-Host ('        ' + (T $es $en)) }

function Pick-Folder {
    try {
        Add-Type -AssemblyName System.Windows.Forms
        $d = New-Object System.Windows.Forms.FolderBrowserDialog
        $d.Description = T 'Elige la carpeta de Kenshi (la que tiene kenshi_x64.exe)' 'Choose your Kenshi folder (the one with kenshi_x64.exe)'
        if ($d.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { return $d.SelectedPath }
    } catch {}
    return (Read-Host (T 'Pega la ruta de la carpeta de Kenshi' 'Paste the path of your Kenshi folder'))
}

$exitCode = 0
try {
    # The kit's version (make_mod_kit.ps1 writes it to PROVENANCE.json).
    $title = 'TokelaCoop'
    try {
        $prov = Get-Content -LiteralPath (Join-Path $kitDir 'PROVENANCE.json') -Raw -ErrorAction Stop | ConvertFrom-Json
        if ($prov.version) { $title = "TokelaCoop v$($prov.version)" }
    } catch {}
    Write-Host '==============================================' -ForegroundColor Cyan
    Write-Host (T "   $title - instalador" "   $title - installer") -ForegroundColor Cyan
    Write-Host '==============================================' -ForegroundColor Cyan

    # Unblock the extracted kit first (a downloaded zip tags every file).
    Get-ChildItem -LiteralPath $kitDir -Recurse -File -ErrorAction SilentlyContinue |
        Unblock-File -ErrorAction SilentlyContinue

    $kitMod = Join-Path $kitDir 'TokelaCoop'
    if (-not (Test-Path -LiteralPath (Join-Path $kitMod 'TokelaCoop.dll'))) {
        throw (T "No encuentro la carpeta TokelaCoop junto al instalador ($kitMod). Descomprime el zip entero y vuelve a probar." `
                 "The TokelaCoop folder is missing next to the installer ($kitMod). Extract the whole zip and try again.")
    }

    # 1. Kenshi ------------------------------------------------------------------
    Step 'Buscando Kenshi...' 'Looking for Kenshi...'
    $kenshi = $KenshiPath
    if (-not $kenshi) {
        $found = @(Find-KenshiInstalls)
        if ($found.Count -ge 1) {
            $kenshi = $found[0]
            if ($found.Count -gt 1) {
                Warn "Hay varias instalaciones; uso la primera. Para otra: -KenshiPath ""ruta""" `
                     "Several installs found; using the first. For another one: -KenshiPath ""path"""
                foreach ($f in $found) { Info $f $f }
            }
        } else {
            Warn 'No la encuentro automáticamente.' 'Could not find it automatically.'
            $kenshi = Pick-Folder
        }
    }
    if (-not (Test-KenshiDir $kenshi)) {
        throw (T "'$kenshi' no es una carpeta de Kenshi (falta kenshi_x64.exe)." `
                 "'$kenshi' is not a Kenshi folder (kenshi_x64.exe is missing).")
    }
    Ok "Kenshi: $kenshi" "Kenshi: $kenshi"

    if (-not (Test-DirWritable $kenshi)) {
        $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
                   [Security.Principal.WindowsBuiltInRole]::Administrator)
        if ($isAdmin) { throw (T "No puedo escribir en '$kenshi'." "Cannot write to '$kenshi'.") }
        Warn 'Hacen falta permisos de administrador para esa carpeta; Windows te los pedirá.' `
             'That folder needs administrator rights; Windows will ask for them.'
        $args2 = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$($MyInvocation.MyCommand.Path)`"",
                   '-KenshiPath', "`"$kenshi`"")
        if ($SkipREKenshi) { $args2 += '-SkipREKenshi' }
        Start-Process -FilePath 'powershell.exe' -ArgumentList $args2 -Verb RunAs
        $NoPause = $true
        return
    }

    while (Test-KenshiRunning) {
        Warn 'Kenshi está abierto. Ciérralo y pulsa Enter para seguir.' 'Kenshi is running. Close it and press Enter to continue.'
        [void](Read-Host)
    }

    # 2. RE_Kenshi --------------------------------------------------------------
    Step 'Comprobando RE_Kenshi (el cargador de mods que necesita TokelaCoop)...' `
         'Checking RE_Kenshi (the mod loader TokelaCoop needs)...'
    $rek = Get-REKenshiState $kenshi
    if ($rek.Installed -and $rek.Enabled) {
        if ($rek.Version -ne 'unknown') { Ok "RE_Kenshi $($rek.Version) instalado." "RE_Kenshi $($rek.Version) installed." }
        else {
            Ok 'RE_Kenshi instalado.' 'RE_Kenshi installed.'
            Warn "No es una versión probada con TokelaCoop (probadas: 0.3.4 y 0.3.5). Si el panel F2 no aparece, reinstala RE_Kenshi $((Get-KcRelease).Version)." `
                 "Not a version tested with TokelaCoop (tested: 0.3.4 and 0.3.5). If the F2 panel does not show, reinstall RE_Kenshi $((Get-KcRelease).Version)."
        }
    } elseif ($SkipREKenshi) {
        Warn 'RE_Kenshi no está instalado (lo saltas con -SkipREKenshi). TokelaCoop no cargará sin él.' `
             'RE_Kenshi is not installed (skipped with -SkipREKenshi). TokelaCoop will not load without it.'
    } else {
        if ($rek.Installed) { Warn 'RE_Kenshi está pero desactivado.' 'RE_Kenshi is present but disabled.' }
        else { Info 'No está instalado. Lo descargo de GitHub (unos 12 MB)...' 'Not installed. Downloading it from GitHub (about 12 MB)...' }
        $exe = Get-REKenshiInstaller (Join-Path $env:TEMP 'TokelaCoop-installer')
        Ok 'Descarga verificada.' 'Download verified.'
        try { Set-Clipboard -Value $kenshi } catch {}
        Write-Host ''
        Write-Host (T '   Se abre ahora el instalador oficial de RE_Kenshi:' '   RE_Kenshi''s official installer opens now:') -ForegroundColor White
        Info "1. Si te pide la carpeta de Kenshi, pega (Ctrl+V): $kenshi" "1. If it asks for the Kenshi folder, paste (Ctrl+V): $kenshi"
        Info '2. Pulsa Install y espera a que termine.' '2. Click Install and wait until it finishes.'
        Info '3. Cierra ese instalador y vuelve a esta ventana.' '3. Close that installer and come back to this window.'
        Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe)
        while ($true) {
            [void](Read-Host (T '   Pulsa Enter cuando hayas terminado con RE_Kenshi' '   Press Enter when you are done with RE_Kenshi'))
            $rek = Get-REKenshiState $kenshi
            if ($rek.Installed -and $rek.Enabled) { Ok 'RE_Kenshi instalado.' 'RE_Kenshi installed.'; break }
            Warn 'Todavía no veo RE_Kenshi en esa carpeta. Termina su instalador (botón Install) y pulsa Enter otra vez.' `
                 'RE_Kenshi is not in that folder yet. Finish its installer (Install button) and press Enter again.'
        }
    }

    # 3. TokelaCoop files --------------------------------------------------------
    Step 'Copiando TokelaCoop a la carpeta mods...' 'Copying TokelaCoop into the mods folder...'
    $dst = Install-TokelaCoopFiles $kitMod $kenshi
    Ok $dst $dst

    # 4. Enable the mod (and switch off the old KenshiCoop) ----------------------
    Step 'Activando TokelaCoop en la lista de mods...' 'Enabling TokelaCoop in the mod list...'
    if (Enable-TokelaCoopMod $kenshi) { Ok 'Activado (data\mods.cfg).' 'Enabled (data\mods.cfg).' }
    else { Ok 'Ya estaba activado.' 'Already enabled.' }

    # 5. The old KenshiCoop (the name up to v0.53) --------------------------------
    # Only after mods.cfg no longer lists it: if the removal fails, it is already off.
    try {
        $old = Remove-LegacyKenshiCoop $kenshi
        if ($old) {
            Ok "Quitada la versión antigua (KenshiCoop): $old" "Removed the old version (KenshiCoop): $old"
        }
    } catch {
        Warn "No pude borrar la carpeta mods\KenshiCoop (ya está desactivada). Bórrala a mano: $($_.Exception.Message)" `
             "Could not delete the mods\KenshiCoop folder (it is already disabled). Delete it by hand: $($_.Exception.Message)"
    }

    # 6. Leftovers that break things ---------------------------------------------
    $ws = @(Find-WorkshopTokelaCoop $kenshi)
    if ($ws.Count -gt 0) {
        Warn 'También tienes el mod (TokelaCoop o KenshiCoop) desde Steam Workshop; se cargaría dos veces. Date de baja en Workshop:' `
             'You also have the mod (TokelaCoop or KenshiCoop) from Steam Workshop; it would load twice. Unsubscribe from it in the Workshop:'
        foreach ($w in $ws) { Info $w $w }
    }

    Write-Host ''
    Write-Host '==============================================' -ForegroundColor Green
    Write-Host (T '   Listo. TokelaCoop está instalado.' '   Done. TokelaCoop is installed.') -ForegroundColor Green
    Write-Host '==============================================' -ForegroundColor Green
    Info 'Para jugar con un amigo (los dos con TokelaCoop instalado y Steam abierto):' `
         'To play with a friend (both with TokelaCoop installed and Steam running):'
    Info '  1. Abrid Kenshi. En el menú principal debe verse la versión de RE_Kenshi.' `
         '  1. Start Kenshi. The main menu should show the RE_Kenshi version.'
    Info '  2. El que hospeda carga su partida, pulsa F2 y "Invite a Steam friend".' `
         '  2. The host loads a save, presses F2 and clicks "Invite a Steam friend".'
    Info '  3. El amigo acepta la invitación de Steam (con Kenshi abierto, vale el menú).' `
         '  3. The friend accepts the Steam invite (with Kenshi open, the menu is fine).'
    Info 'Instrucciones completas y problemas frecuentes: README.txt' 'Full instructions and troubleshooting: README.txt'
} catch {
    $exitCode = 1
    Write-Host ''
    Write-Host ((T '   ERROR: ' '   ERROR: ') + $_.Exception.Message) -ForegroundColor Red
    Info 'No se ha dejado nada a medias que impida volver a ejecutar el instalador.' `
         'Nothing was left half-done that prevents running the installer again.'
} finally {
    if (-not $NoPause) { Write-Host ''; [void](Read-Host (T 'Pulsa Enter para cerrar' 'Press Enter to close')) }
}
exit $exitCode
