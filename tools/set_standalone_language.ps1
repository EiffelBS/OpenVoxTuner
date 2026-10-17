# Patch the persisted UI language of the standalone (tools/set_standalone_language.ps1)
#
# The settings file keeps processor state as JUCE base64 under <VALUE name="filterState" val="N....">, and the
# language is a parameter inside that blob: <PARAM id="ui_language" value="0.0"/> (English..Chinese = 0..5).
# Synthetic input is blocked on this machine, so this file patch is the only way to land the UI in a given locale.
#
# Run it as: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\set_standalone_language.ps1 -Lang 1
# (the workspace execution policy refuses an unsigned script invoked with & .\script.ps1).

param([int]$Lang = 0, [string]$Path = "$env:APPDATA\OpenVoxTuner\OpenVoxTuner.settings")

. "$PSScriptRoot\juce_base64_lib.ps1"

if ($Lang -lt 0 -or $Lang -gt 5) { "LANG_OUT_OF_RANGE $Lang"; exit 1 }
$backup = "$Path.bak_lang"
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

$hit = ([regex]::Match($text, 'id="ui_language" value="[^"]*"')).Value
if ($hit -eq '') { "NO ui_language PARAM in the state blob"; exit 1 }
"before: $hit"
$newText = $text -replace 'id="ui_language" value="[^"]*"', ('id="ui_language" value="' + $Lang.ToString() + '.0"')
"after:  " + ([regex]::Match($newText, 'id="ui_language" value="[^"]*"')).Value

$newVal = Encode-JuceBase64 ([Text.Encoding]::UTF8.GetBytes($newText))
# A failed encode returns an empty string; writing it would blank the whole state blob. Guard that.
if ($newVal.Length -lt 100) { "ENCODE_TOO_SHORT ($($newVal.Length)) - refusing to write"; exit 1 }

$xmlNew = $xml.Substring(0, $m.Index) + ($xml.Substring($m.Index) -replace 'name="filterState"\s+val="[^"]*"', ('name="filterState" val="' + $newVal + '"'))
[IO.File]::WriteAllText($Path, $xmlNew, (New-Object System.Text.UTF8Encoding($false)))
"wrote size=$((Get-Item $Path).Length)"
