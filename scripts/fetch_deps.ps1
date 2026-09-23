<#
.SYNOPSIS
  Fetch the plugin's third-party dependencies into third_party/ (idempotent).

.DESCRIPTION
  - KenshiLib_Examples_deps (Git LFS): precompiled KenshiLib.lib / OgreMain / MyGUI
    import libs + Boost 1.60 headers. Pinned to b566d74 (KenshiLib 0.4.0), whose
    KenshiLib.lib exports every KenshiLib symbol the shipped v0.51 DLL imports.
  - KenshiLib headers are replaced with the KenshiLib source repo at b0d7665
    (2026-08-09): the headers bundled with the deps repo do not compile together
    (BuildingDesignation defined twice, CraftingItem incomplete - fixed upstream
    in 0e0ded7 / 6f9168d). A forwarding kenshi/CombatClass.h is added because the
    plugin includes the pre-0.4 path (the class moved to kenshi/combat/).
  - ENet v1.3.18 + the patches in third_party/enet/patches.

  Needs git and git-lfs on PATH.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\fetch_deps.ps1
#>
[CmdletBinding()]
param(
    [string]$DepsRef = "b566d74",
    [string]$HeadersRef = "b0d7665",
    [string]$EnetTag = "v1.3.18"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$tp = Join-Path $repo "third_party"
$deps = Join-Path $tp "KenshiLib_deps"
$enet = Join-Path $tp "enet\enet"

function Git { & git @args; if ($LASTEXITCODE -ne 0) { throw "git $args failed ($LASTEXITCODE)" } }

Write-Host "=== KenshiLib_Examples_deps @ $DepsRef ==="
Git lfs install --skip-repo | Out-Null
if (!(Test-Path (Join-Path $deps ".git"))) {
    $env:GIT_LFS_SKIP_SMUDGE = "1"
    Git clone -q https://github.com/BFrizzleFoShizzle/KenshiLib_Examples_deps $deps
    Remove-Item Env:\GIT_LFS_SKIP_SMUDGE
}
Git -C $deps checkout -q -f $DepsRef
Git -C $deps lfs pull
$boost = Join-Path $deps "boost_1_60_0"
if (!(Test-Path (Join-Path $boost "boost"))) {
    Write-Host "extracting boost"
    & tar -xf (Join-Path $boost "boost.zip") --directory $boost
    if ($LASTEXITCODE -ne 0) { throw "boost extract failed" }
}

Write-Host "=== KenshiLib headers @ $HeadersRef ==="
$src = Join-Path $tp "KenshiLib_src"
if (!(Test-Path (Join-Path $src ".git"))) {
    $env:GIT_LFS_SKIP_SMUDGE = "1"
    Git clone -q --filter=blob:none --no-checkout https://github.com/BFrizzleFoShizzle/KenshiLib.git $src
    Remove-Item Env:\GIT_LFS_SKIP_SMUDGE
}
Git -C $src -c filter.lfs.smudge= -c filter.lfs.required=false checkout -q -f $HeadersRef -- Include
$inc = Join-Path $deps "KenshiLib\Include"
Remove-Item -Recurse -Force $inc
Copy-Item -Recurse (Join-Path $src "Include") $inc
@(
    '#pragma once'
    '// Added by scripts/fetch_deps.ps1: the plugin includes the pre-0.4 path.'
    '#include <kenshi/combat/CombatClass.h>'
) | Set-Content -Encoding ASCII (Join-Path $inc "kenshi\CombatClass.h")

Write-Host "=== ENet $EnetTag ==="
if (!(Test-Path (Join-Path $enet ".git"))) {
    Git clone -q --branch $EnetTag --depth 1 https://github.com/lsalzman/enet $enet
    Push-Location $repo
    try {
        Git apply third_party/enet/patches/0001-enet-c89-for-loops.patch
        Git apply third_party/enet/patches/0002-enet-socket-hooks.patch
    } finally { Pop-Location }
} else {
    Write-Host "  already present (patches assumed applied)"
}

Write-Host ""
Write-Host "deps ready. Env for the vcxproj (build_plugin.cmd sets INCLUDE/LIB itself):"
Write-Host "  KENSHILIB_DIR=$deps\KenshiLib"
Write-Host "  BOOST_INCLUDE_PATH=$boost"
