# Decode the JUCE standalone settings file (tools/decode_standalone_state.ps1)
#
# The settings file stores base64 of processor->getStateInformation() under <VALUE name="filterState" val="N....">.
# JUCE base64 is NOT standard, so [Convert]::FromBase64String fails; tools/juce_base64_lib.ps1 reproduces
# MemoryBlock::to/fromBase64Encoding exactly (see that file for the source line numbers).
#
# Usage: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\decode_standalone_state.ps1 [path]

param([string]$Path = "$env:APPDATA\OpenVoxTuner\OpenVoxTuner.settings")

. "$PSScriptRoot\juce_base64_lib.ps1"

$xmlText = [IO.File]::ReadAllText($Path)
$m = [regex]::Match($xmlText, 'name="filterState"\s+val="([^"]*)"')
if (-not $m.Success) { "NO filterState in $Path"; exit 1 }

$bytes = Decode-JuceBase64 $m.Groups[1].Value   # the outer array unrolls: $bytes IS the byte[]
"declaredLen=$($bytes.Length)"

$end = $bytes.Length
while ($end -gt 0 -and $bytes[$end - 1] -eq 0) { $end -= 1 }
$text = [Text.Encoding]::UTF8.GetString([byte[]] $bytes[0..($end - 1)])
"textLen=$text.Length"
"text head: " + $text.Substring(0, [Math]::Min(90, $text.Length))

foreach ($key in @('advancedExpanded', 'harmonyExpanded', 'abActiveSlot'))
{
    $mm = [regex]::Match($text, ($key + '="([^"]*)"'))
    if ($mm.Success) { "FOUND $key=$($mm.Groups[1].Value)" } else { "ABSENT $key" }
}

# The window geometry keys live in the same file next to filterState.
foreach ($key in @('windowX', 'windowY', 'shouldMuteInput'))
{
    $mm = [regex]::Match($xmlText, ($key + '"\s+val="([^"]*)"'))
    if ($mm.Success) { "FOUND $key=$($mm.Groups[1].Value)" } else { "ABSENT $key" }
}
