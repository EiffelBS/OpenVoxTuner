# Self-test for tools/juce_base64_lib.ps1 (run with ExecutionPolicy Bypass)
# powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\test_juce_base64.ps1
#
# Note on the return shape: Decode-JuceBase64 returns ,$bytes, and PowerShell unrolls that outer
# array when assigning, so the caller gets the byte[] itself. Do NOT index with [0].

. "$PSScriptRoot\juce_base64_lib.ps1"

$probe = 'OpenVoxTuner <OpenVoxTuner> PARAM id value'
$bytes = [Text.Encoding]::UTF8.GetBytes($probe)
$enc = Encode-JuceBase64 $bytes
"encoded: $enc"
"expected head: 42.OAWYtY0a3QUctUlbfvySvUlaV8FdTUmakImOf.UPRETSf"

$b = Decode-JuceBase64 $enc
"type: $($b.GetType().Name) length: $($b.Length)"
$back = [Text.Encoding]::UTF8.GetString($b)
"decoded: $back"
"round trip exact: $($back -eq $probe)"

# A real head taken from the live settings file, encoded by JUCE itself.
$known = '42.OAWYtY0a3QUctUlbfvySvUlaV8FdTUmakImOf.UPRETSf'
$k = Decode-JuceBase64 $known
"known decoded: " + [Text.Encoding]::UTF8.GetString($k)
"known re-encoded matches: $((Encode-JuceBase64 $k) -eq $known)"
