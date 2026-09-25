# ModText.psm1 - change display text inside a Kenshi FCS .mod (file type 16)
# without the OpenConstructionSet toolchain (tools/MultiplayerStartGen needs a
# .NET 9 SDK and a local Kenshi install).
#
# Layout (little-endian; a "string" is an int32 byte length + UTF-8 bytes):
#   int32 fileType (16), int32 version, string author, string description,
#   string dependencies, string references, int32 lastId, int32 itemCount,
#   then itemCount records. Each record starts with an int32 holding the
#   record's TOTAL byte length (that field included), then int32 type,
#   int32 id, string name, string stringId, and its fields.
# There is no offsets table and no checksum, so a string can change length as
# long as its own prefix and its record's total length are rewritten.
#
# StringIds are save identifiers: Edit-ModFile refuses to change any of them.
Set-StrictMode -Version Latest

$script:Utf8 = New-Object System.Text.UTF8Encoding($false)

function Read-ModString([byte[]]$Bytes, [ref]$Pos) {
    $len = [BitConverter]::ToInt32($Bytes, $Pos.Value)
    if ($len -lt 0 -or ($Pos.Value + 4 + $len) -gt $Bytes.Length) { throw "bad string length $len at $($Pos.Value)" }
    $s = $script:Utf8.GetString($Bytes, $Pos.Value + 4, $len)
    $Pos.Value += 4 + $len
    return $s
}

function Get-ModInfo {
    <#
    .SYNOPSIS
      Parse a .mod: header fields plus each record's type, id, name and StringId.
      Throws when the records do not add up to the file length.
    #>
    param([string]$Path)
    $b = [IO.File]::ReadAllBytes($Path)
    return (Get-ModInfoFromBytes $b)
}

function Get-ModInfoFromBytes([byte[]]$Bytes) {
    $b = $Bytes
    $type = [BitConverter]::ToInt32($b, 0)
    if ($type -ne 16) { throw "unsupported .mod file type $type (expected 16)" }
    $p = 8
    $info = [ordered]@{ FileType = $type; Version = [BitConverter]::ToInt32($b, 4) }
    $info.Author       = Read-ModString $b ([ref]$p)
    $info.Description  = Read-ModString $b ([ref]$p)
    $info.Dependencies = Read-ModString $b ([ref]$p)
    $info.References   = Read-ModString $b ([ref]$p)
    $info.LastId       = [BitConverter]::ToInt32($b, $p)
    $info.ItemCount    = [BitConverter]::ToInt32($b, $p + 4)
    $info.RecordsAt    = $p + 8
    $p += 8
    $recs = @()
    for ($i = 0; $i -lt $info.ItemCount; $i++) {
        $len = [BitConverter]::ToInt32($b, $p)
        if ($len -lt 16 -or ($p + $len) -gt $b.Length) { throw "record $i at ${p}: bad length $len" }
        $q = $p + 12
        $name = Read-ModString $b ([ref]$q)
        $sid  = Read-ModString $b ([ref]$q)
        $recs += [pscustomobject]@{ Offset = $p; Length = $len; Type = [BitConverter]::ToInt32($b, $p + 4)
                                    Id = [BitConverter]::ToInt32($b, $p + 8); Name = $name; StringId = $sid }
        $p += $len
    }
    if ($p -ne $b.Length) { throw "records end at $p but the file is $($b.Length) bytes" }
    $info.Records = $recs
    return [pscustomobject]$info
}

# Every length-prefixed printable string in $Bytes[$From..$To) that contains $Text:
# @{ Offset (of the prefix); Length; Value }. Printable text never holds a small
# int32 (its bytes are >= 0x20), so walking back from a hit finds its real prefix.
function Find-StringsIn([byte[]]$Bytes, [int]$From, [int]$To, [string]$Text) {
    $pat = $script:Utf8.GetBytes($Text)
    $found = @{}
    for ($i = $From + 4; $i -le $To - $pat.Length; $i++) {
        $hit = $true
        for ($j = 0; $j -lt $pat.Length; $j++) { if ($Bytes[$i + $j] -ne $pat[$j]) { $hit = $false; break } }
        if (-not $hit) { continue }
        for ($s = $i - 4; $s -ge $From; $s--) {
            $L = [BitConverter]::ToInt32($Bytes, $s)
            if ($L -le 0 -or $L -gt 65536) { continue }
            if (($s + 4 + $L) -lt ($i + $pat.Length) -or ($s + 4 + $L) -gt $To) { continue }
            $v = $script:Utf8.GetString($Bytes, $s + 4, $L)
            if ($v -match '[\x00-\x08\x0B\x0C\x0E-\x1F]' -or $script:Utf8.GetByteCount($v) -ne $L) { continue }
            $found[$s] = [pscustomobject]@{ Offset = $s; Length = $L; Value = $v }
            break
        }
    }
    return @($found.Values | Sort-Object Offset)
}

# The display strings of the record at $Offset: its name and the VALUE of each
# string field - never a field key, a reference category, a file path or an id,
# which the game looks things up by. Walks the record's field blocks (bool,
# float, int, vec3, vec4, string, file, references, instances) and throws unless
# the walk ends exactly at the record's end (an unknown layout is not edited).
# Returns a hashtable of the allowed string offsets (their length prefix).
function Get-DisplayStrings([byte[]]$Bytes, [int]$Offset, [int]$Length) {
    $allowed = @{}
    $q = $Offset + 12                                   # length, type, id
    $allowed[$q] = $true; [void](Read-ModString $Bytes ([ref]$q))   # name
    [void](Read-ModString $Bytes ([ref]$q))             # StringId
    $q += 4                                              # change type
    foreach ($size in 1, 4, 4, 12, 16) {                 # bool, float, int, vec3, vec4
        $n = [BitConverter]::ToInt32($Bytes, $q); $q += 4
        for ($i = 0; $i -lt $n; $i++) { [void](Read-ModString $Bytes ([ref]$q)); $q += $size }
    }
    $n = [BitConverter]::ToInt32($Bytes, $q); $q += 4    # string fields: key, value
    for ($i = 0; $i -lt $n; $i++) {
        [void](Read-ModString $Bytes ([ref]$q))
        $allowed[$q] = $true; [void](Read-ModString $Bytes ([ref]$q))
    }
    $n = [BitConverter]::ToInt32($Bytes, $q); $q += 4    # file fields: key, path
    for ($i = 0; $i -lt $n; $i++) { [void](Read-ModString $Bytes ([ref]$q)); [void](Read-ModString $Bytes ([ref]$q)) }
    $n = [BitConverter]::ToInt32($Bytes, $q); $q += 4    # reference categories
    for ($i = 0; $i -lt $n; $i++) {
        [void](Read-ModString $Bytes ([ref]$q))
        $m = [BitConverter]::ToInt32($Bytes, $q); $q += 4
        for ($j = 0; $j -lt $m; $j++) { [void](Read-ModString $Bytes ([ref]$q)); $q += 12 }
    }
    $n = [BitConverter]::ToInt32($Bytes, $q); $q += 4    # instances
    for ($i = 0; $i -lt $n; $i++) {
        [void](Read-ModString $Bytes ([ref]$q)); [void](Read-ModString $Bytes ([ref]$q)); $q += 28
        $k = [BitConverter]::ToInt32($Bytes, $q); $q += 4
        for ($j = 0; $j -lt $k; $j++) { [void](Read-ModString $Bytes ([ref]$q)) }
    }
    if ($q -ne $Offset + $Length) { throw "record at ${Offset}: unexpected layout (walk ended at $q, record ends at $($Offset + $Length))" }
    return $allowed
}

# Rewrite the strings of $Bytes[$From..$To) that contain a key of $Replace; with
# $Allowed, only strings whose length prefix is at one of those offsets.
# Returns @{ Bytes = rewritten slice; Changed = list; Hits = keys matched }.
function Edit-Slice([byte[]]$Bytes, [int]$From, [int]$To, $Replace, $Allowed = $null) {
    $targets = @{}
    $hits = @{}
    foreach ($k in $Replace.Keys) {
        foreach ($h in @(Find-StringsIn $Bytes $From $To $k)) {
            if ($null -ne $Allowed -and -not $Allowed.ContainsKey($h.Offset)) { continue }
            $targets[$h.Offset] = $h; $hits[$k] = $true
        }
    }
    $out = New-Object System.IO.MemoryStream
    $pos = $From
    $changed = @()
    foreach ($t in @($targets.Values | Sort-Object Offset)) {
        if ($t.Offset -lt $pos) { throw "overlapping strings at $($t.Offset)" }
        $out.Write($Bytes, $pos, $t.Offset - $pos)
        $nv = $t.Value
        foreach ($k in $Replace.Keys) { $nv = $nv.Replace([string]$k, [string]$Replace[$k]) }
        $nb = $script:Utf8.GetBytes($nv)
        $out.Write([BitConverter]::GetBytes([int32]$nb.Length), 0, 4)
        $out.Write($nb, 0, $nb.Length)
        $pos = $t.Offset + 4 + $t.Length
        if ($nv -ne $t.Value) { $changed += [pscustomobject]@{ Old = $t.Value; New = $nv } }
    }
    $out.Write($Bytes, $pos, $To - $pos)
    return [pscustomobject]@{ Bytes = $out.ToArray(); Changed = $changed; Hits = @($hits.Keys) }
}

function Edit-ModFile {
    <#
    .SYNOPSIS
      Replace display text in a .mod: every DISPLAY string containing a key of
      $Replace (plain text, applied in order) - the header description, each
      record's name and the values of its string fields (Get-DisplayStrings);
      field keys, reference categories, file paths and ids are never touched.
      -Description sets the whole header description instead. Record lengths are
      recomputed. Throws if a key matches nothing, if a record has a layout it
      cannot walk, or if any StringId or id reference would change.
    #>
    param([Parameter(Mandatory = $true)][string]$Path,
          [System.Collections.Specialized.OrderedDictionary]$Replace = $null,
          [string]$Description = $null,
          [string]$OutPath = "")
    if (-not $OutPath) { $OutPath = $Path }
    if ($null -eq $Replace) { $Replace = [ordered]@{} }
    $b = [IO.File]::ReadAllBytes($Path)
    $before = Get-ModInfoFromBytes $b
    $p = 8
    [void](Read-ModString $b ([ref]$p))   # author
    $descAt = $p
    [void](Read-ModString $b ([ref]$p))   # description
    $descEnd = $p

    $out = New-Object System.IO.MemoryStream
    $out.Write($b, 0, $descAt)                        # type, version, author
    $allHits = @{}
    $changed = @()
    if ($Description) {
        $db = $script:Utf8.GetBytes($Description)
        $out.Write([BitConverter]::GetBytes([int32]$db.Length), 0, 4)
        $out.Write($db, 0, $db.Length)
        if ($before.Description -ne $Description) {
            $changed += [pscustomobject]@{ Old = $before.Description; New = $Description }
        }
    } else {
        $r = Edit-Slice $b $descAt $descEnd $Replace
        $out.Write($r.Bytes, 0, $r.Bytes.Length)
        foreach ($k in $r.Hits) { $allHits[$k] = $true }
        $changed += $r.Changed
    }
    $out.Write($b, $descEnd, $before.RecordsAt - $descEnd)   # deps, refs, lastId, count
    foreach ($rec in $before.Records) {
        $allowed = Get-DisplayStrings $b $rec.Offset $rec.Length
        $r = Edit-Slice $b ($rec.Offset + 4) ($rec.Offset + $rec.Length) $Replace $allowed
        $out.Write([BitConverter]::GetBytes([int32]($r.Bytes.Length + 4)), 0, 4)
        $out.Write($r.Bytes, 0, $r.Bytes.Length)
        foreach ($k in $r.Hits) { $allHits[$k] = $true }
        $changed += $r.Changed
    }
    foreach ($k in $Replace.Keys) { if (-not $allHits.ContainsKey([string]$k)) { throw "text not found in ${Path}: '$k'" } }

    $nb = $out.ToArray()
    $after = Get-ModInfoFromBytes $nb
    # Save identifiers must survive byte-for-byte.
    if ($after.ItemCount -ne $before.ItemCount) { throw "record count changed" }
    for ($i = 0; $i -lt $before.ItemCount; $i++) {
        $x = $before.Records[$i]; $y = $after.Records[$i]
        if ($x.StringId -ne $y.StringId -or $x.Type -ne $y.Type -or $x.Id -ne $y.Id) {
            throw "record $i identity changed: '$($x.StringId)' -> '$($y.StringId)'"
        }
    }
    # ...and so must every reference to them inside the records.
    $idsOf = {
        param([byte[]]$x)
        $s = [Text.Encoding]::GetEncoding(28591).GetString($x)   # bytes 1:1
        return ((@([regex]::Matches($s, '\d+-[A-Za-z0-9_+-]+\.(mod|base)') | ForEach-Object { $_.Value }) |
                 Sort-Object) -join '|')
    }
    if ((& $idsOf $b) -ne (& $idsOf $nb)) { throw "a StringId reference changed" }
    [IO.File]::WriteAllBytes($OutPath, $nb)
    return $changed
}

Export-ModuleMember -Function Get-ModInfo, Edit-ModFile
