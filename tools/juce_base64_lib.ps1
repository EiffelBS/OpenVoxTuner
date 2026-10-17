# JUCE base64 codec reproduced in PowerShell (tools/juce_base64_lib.ps1)
#
# Source of truth: C:\JUCE\modules\juce_core\memory\juce_MemoryBlock.cpp
#   L283-305 getBitRange, L307-328 setBitRange,
#   L361 base64EncodingTable ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+",
#   L366-383 toBase64Encoding ("N." + data, 6-bit groups taken LSB-first),
#   L385-392 base64DecodingTable, L394-425 fromBase64Encoding.
# JUCE base64 is NOT standard base64: index 0 of the alphabet is '.', and the string carries
# its own byte count before the dot, so [Convert]::FromBase64String cannot read it.

$alpha = ".ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"

# base64DecodingTable copied verbatim from juce_MemoryBlock.cpp L385-392 (80 entries, index = char - 43)
$decTable = @(63, 0, 0, 0, 0, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 0, 0, 0, 0, 0, 0, 0,
             1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
             0, 0, 0, 0, 0, 0, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52)

$full = [int64] 4294967295

function U32([int64]$x) { return ($x -band $full) }

# PowerShell has no uint32 type, so ~x is written as -(x+1) masked to 32 bits.
function Not32([int64]$x) { return (U32 (- ($x + 1))) }

function Get-BitRange([byte[]]$bytes, [int]$start, [int]$numBits)
{
    $res = 0; $byte = $start -shr 3; $off = $start -band 7; $soFar = 0
    while ($numBits -gt 0 -and $byte -lt $bytes.Length)
    {
        $take = [Math]::Min($numBits, 8 - $off)
        $mask = (255 -shr (8 - $take)) -shl $off          # (0xff >> (8-take)) << off
        $res  = $res -bor (((($bytes[$byte] -band $mask) -shr $off) -shl $soFar))
        $soFar += $take; $numBits -= $take; $byte += 1; $off = 0
    }
    return $res
}

function Set-BitRange([byte[]]$bytes, [int]$start, [int]$numBits, [int]$val)
{
    $byte = $start -shr 3                                  # JUCE: bitRangeStart >> 3 (floor), never rounding
    $off  = $start -band 7
    # mask = ~(((uint32)0xffffffff << (32-numBits)) >> (32-numBits)), every step in uint32
    $mask = Not32 ((U32 ($full -shl (32 - $numBits))) -shr (32 - $numBits))

    while ($numBits -gt 0 -and $byte -lt $bytes.Length)
    {
        $take     = [Math]::Min($numBits, 8 - $off)
        $hi       = U32 ((U32 ($full -shr $off)) -shl $off)
        $tempMask = U32 ((U32 ($mask -shl $off)) -bor (Not32 $hi))
        $tempBits = U32 ($val -shl $off)
        $bytes[$byte] = [byte] ((($bytes[$byte] -band $tempMask) -bor $tempBits) -band 0xFF)

        $byte   += 1
        $numBits -= $take
        $val    = $val -shr $take
        $mask    = U32 ($mask -shr $take)
        $off     = 0
    }
}

function Encode-JuceBase64([byte[]]$bytes)
{
    $size = $bytes.Length
    $numChars = [int] [Math]::Floor(((($size -shl 3) + 5) / 6))   # JUCE integer division (truncate, not round)
    $out = New-Object char[] $numChars
    for ($i = 0; $i -lt $numChars; $i += 1) { $out[$i] = $alpha[(Get-BitRange $bytes ($i * 6) 6)] }
    return "$size.$(-join $out)"
}

function Decode-JuceBase64([string]$s)
{
    $dot = $s.IndexOf('.')
    if ($dot -lt 0) { return $null }
    $n = [int] $s.Substring(0, $dot)
    $bytes = New-Object byte[] $n
    $pos = 0
    for ($i = $dot + 1; $i -lt $s.Length; $i += 1)
    {
        $c = [int] [char] $s[$i] - 43
        if ($c -ge 0 -and $c -lt $decTable.Length)
        {
            Set-BitRange $bytes $pos 6 $decTable[$c]       # JUCE: setBitRange (pos, 6, base64DecodingTable[c])
            $pos += 6
        }
    }
    return ,$bytes
}
