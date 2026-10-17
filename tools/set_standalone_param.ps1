# Patch one persisted parameter inside the standalone's state blob (tools/set_standalone_param.ps1)
#
# Same JUCE base64 machinery as tools/set_standalone_language.ps1, keyed by PARAM id instead of the language:
# this is how a capture series lands a control at a known value with synthetic input blocked on this machine.
# The value is written in the same shape JUCE emits ("value=\"N.N\""), so it survives the app's own re-save.
#
# Run it as: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\set_standalone_param.ps1 -Id scale -Value 0

param([string]$Id = 'scale', [string]$Value = '0.0', [string]$Path = "$env:APPDATA\OpenVoxTuner\OpenVoxTuner.settings")

. "$PSScriptRoot\juce_base64_lib.ps1"

$backup = "$Path.bak_param"
Copy-Item $Path $backup -Force
"backup=$backup size=$((Get-Item $backup).Length)"

$xml = [IO.File]::ReadAllText($Path)
$m = [regex]::Match($xml, 'name="filterState"\s+val="([^"]*)"')
if (-not $m.Success) { "NO filterState in $Path"; exit 1 }

$bytes = Decode-JuceBase64 $m.Groups[1].Value
if ($null -eq $bytes) { "DECODE_FAILED"; exit 1 }
$end = $bytes.Length
while ($end -gt 0 -and $bytes[$end - 1] -eq 0) { $end -= 1 }
$text = [Text.Encoding]::UTF8.GetString([byte[]] $bytes[0..($end - 1)])

# Only ids that exist as a PARAM entry can be patched; an AB_A attribute with the same name is not the target.
$pattern = 'id="' + $Id + '" value="[^"]*"'
$hit = ([regex]::Match($text, $pattern)).Value
if ($hit -eq '') { "NO PARAM id=$Id in the state blob"; exit 1 }
"before: $hit"
$newText = $text -replace $pattern, ('id="' + $Id + '" value="' + $Value + '"')
"after:  " + ([regex]::Match($newText, $pattern)).Value

$newVal = Encode-JuceBase64 ([Text.Encoding]::UTF8.GetBytes($newText))
# A failed encode returns an empty string; writing it would blank the whole state blob. Guard that.
if ($newVal.Length -lt 100) { "ENCODE_TOO_SHORT ($($newVal.Length)) - refusing to write"; exit 1 }

$xmlNew = $xml.Substring(0, $m.Index) + ($xml.Substring($m.Index) -replace 'name="filterState"\s+val="[^"]*"', ('name="filterState" val="' + $newVal + '"'))
[IO.File]::WriteAllText($Path, $xmlNew, (New-Object System.Text.UTF8Encoding($false)))
"wrote size=$((Get-Item $Path).Length)"
