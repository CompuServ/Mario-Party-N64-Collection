param([int[]]$Ids = @(0xBA, 0xC4, 0xCD, 0xCE, 0xCF, 0xD0, 0xDA, 0xDB))
# Dumps MP2 (USA) messages from the user's ROM as hex + printable text, for analysis (nothing is written).
$rom = [IO.File]::ReadAllBytes("$PSScriptRoot\..\roms\marioparty2.us.z64")
function U16($o) { ([int]$rom[$o] -shl 8) -bor [int]$rom[$o + 1] }
function U32($o) { ([uint32]$rom[$o] -shl 24) -bor ([uint32]$rom[$o + 1] -shl 16) -bor ([uint32]$rom[$o + 2] -shl 8) -bor [uint32]$rom[$o + 3] }
$upper = U16 0x1D22A; $lower = U16 0x1D232
$table = ($upper -shl 16) -bor $lower; if ($lower -band 0x8000) { $table -= 0x10000 }
"table at 0x{0:X}, {1} strings" -f $table, (U32 $table)
foreach ($id in $Ids) {
    $e = $table + (U32 ($table + 4 * (1 + $id))); $n = U16 $e
    $bytes = $rom[($e + 2)..($e + 1 + $n)]
    $hex = ($bytes | ForEach-Object { '{0:X2}' -f $_ }) -join ' '
    $txt = -join ($bytes | ForEach-Object { if ($_ -ge 0x20 -and $_ -lt 0x7F) { [char]$_ } else { '{' + ('{0:X2}' -f $_) + '}' } })
    "`n== 0x{0:X} ({1} bytes)`n{2}`n{3}" -f $id, $n, $txt, $hex
}

