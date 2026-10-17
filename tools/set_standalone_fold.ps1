# Patch a persisted frame fold state of the standalone (tools/set_standalone_fold.ps1)
#
# The processor writes its fold states as ROOT ATTRIBUTES of the saved state XML:
#   <OpenVoxTuner ... harmonyExpanded="1" reverbExpanded="0">
# and that whole XML lives as JUCE base64 under <VALUE name="filterState" val="N...."> in the settings file.
# Synthetic input is blocked on this machine, so patching this file is the only way to land the editor in a
# chosen fold state: it exercises the restore path (getStateInformation -> setReverbExpanded) exactly as a
# real close-and-reopen does, which no screenshot of a freshly launched app can prove.
#
# Run it as: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\set_standalone_fold.ps1 -Attr reverbExpanded -Value 0
# (the workspace execution policy refuses an unsigned script invoked with & .\script.ps1).

param([string]$Attr = 'reverbExpanded', [int]$Value = 0, [string]$Path = "$env:APPDATA\OpenVoxTuner\OpenVoxTuner.settings")

. "$PSScriptRoot\juce_base64_lib.ps1"

if ($Attr -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') { "BAD_ATTR $Attr"; exit 1 }
if ($Value -ne 0 -and $Value -ne 1) { "VALUE_NOT_BINARY $Value"; exit 1 }

$backup = "$Path.bak_fold"
Copy-Item $Path $backup -Force
"backup=$backup size=$((Get-Item $backup).Length)"

$xml = [IO.File]::ReadAllText($Path)
$m = [regex]::Match($xml, 'name="filterState"\s+val="([^"]*)"')
if (-not $m.Success) { "NO filterState in $Path"; exit 1 }

$bytes = Decode-JuceBase64 $m.Groups[1].Value   # the outer array unrolls: $bytes IS the byte[]
if ($null -eq $bytes) { "DECODE_FAILED"; exit 1 }
$end = $bytes.Length
while ($end -gt 0 -and $bytes[$end - 1] -eq 0) { $end -= 1 }
$text = [Text.Encoding]::UTF8.GetString([byte[]] $bytes[0..($end - 1)])

# The root tag is the FIRST ELEMENT of the document, which comes after the XML prolog. Searching for the first '>'
# lands on the prolog's own '?>' and writes the attribute into the declaration, where JUCE never reads it: skip
# the prolog before looking for the element's closing bracket.
$prologEnd = $text.IndexOf('?>')
$start = if ($prologEnd -ge 0) { $prologEnd + 2 } else { 0 }
$gt = $text.IndexOf('>', $start)
if ($gt -lt 0) { "NO_ROOT_TAG"; exit 1 }
$rootTag = $text.Substring($start, $gt - $start)
"root head: " + $rootTag.Substring(0, [Math]::Min(90, $rootTag.Length))

$attrRe = ($Attr + '="([^"]*)"')
if ([regex]::Match($rootTag, $attrRe).Success) {
    "before: " + ([regex]::Match($rootTag, $attrRe)).Value
    $newRoot = [regex]::Replace($rootTag, $attrRe, ($Attr + '="' + $Value.ToString() + '"'))
} else {
    "before: ABSENT"
    # A self-closing root tag needs the attribute inserted before the slash, not before the '>'.
    if ($rootTag.EndsWith('/')) { $newRoot = $rootTag.Substring(0, $rootTag.Length - 1) + ' ' + $Attr + '="' + $Value.ToString() + '"' }
    else { $newRoot = $rootTag + ' ' + $Attr + '="' + $Value.ToString() + '"' }
}
"after:  " + ([regex]::Match($newRoot, $attrRe)).Value

$newText = $text.Substring(0, $start) + $newRoot + $text.Substring($gt)
$newVal = Encode-JuceBase64 ([Text.Encoding]::UTF8.GetBytes($newText))
# A failed encode returns an empty string; writing it would blank the whole state blob. Guard that.
if ($newVal.Length -lt 100) { "ENCODE_TOO_SHORT ($($newVal.Length)) - refusing to write"; exit 1 }

$xmlNew = $xml.Substring(0, $m.Index) + ($xml.Substring($m.Index) -replace 'name="filterState"\s+val="[^"]*"', ('name="filterState" val="' + $newVal + '"'))
[IO.File]::WriteAllText($Path, $xmlNew, (New-Object System.Text.UTF8Encoding($false)))
"wrote size=$((Get-Item $Path).Length)"
