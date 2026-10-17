# Report the integrity level of a launched process (tools/check_process_integrity.ps1)
#
# Why: a binary whose image lives inside the workspace runs at Low integrity (S-1-16-4096), and a Low-integrity
# process cannot write into %APPDATA%. The JUCE standalone saves its state to %APPDATA%\<App>\<App>.settings on
# clean close, so an instance launched from the build tree never persists anything. This script proves which
# integrity level a given exe runs at, so a persistence test is not run against a process that cannot write.
#
# Usage: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\check_process_integrity.ps1 [exePath]

param([string]$Exe = "C:\Users\User\Documents\trae_projects\OpenVoxTuner\build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe")

Add-Type -Namespace IntProbe -Name Win32 -MemberDefinition @'
[DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(int access, bool inherit, int pid);
[DllImport("advapi32.dll")] public static extern bool OpenProcessToken(IntPtr proc, int access, out IntPtr token);
[DllImport("advapi32.dll")] public static extern bool GetTokenInformation(IntPtr token, int cls, IntPtr buf, int len, out int ret);
[DllImport("advapi32.dll", CharSet = CharSet.Unicode)] public static extern bool ConvertSidToStringSid(IntPtr sid, out string str);
[DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
'@

$exeDir = (Split-Path -Parent $Exe)
$cmd = '"' + $Exe + '"'
$r = Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $cmd; CurrentDirectory = $exeDir }
$pidNew = [int] $r.ProcessId
Start-Sleep -Seconds 4

$h = [IntProbe.Win32]::OpenProcess(0x1000, $false, $pidNew)   # PROCESS_QUERY_LIMITED_INFORMATION
if ($h -eq [IntPtr]::Zero) { "PID $pidNew : cannot open process"; exit 1 }

$tok = [IntPtr]::Zero
if (-not [IntProbe.Win32]::OpenProcessToken($h, 0x0008, [ref] $tok)) { "cannot open token"; exit 1 }

$buf = [Runtime.InteropServices.Marshal]::AllocHGlobal(64)
$ret = 0
# TokenIntegrityLevel = 25; the buffer starts with a pointer to the integrity SID
if (-not [IntProbe.Win32]::GetTokenInformation($tok, 25, $buf, 64, [ref] $ret)) { "no integrity info"; exit 1 }

$sidPtr = [Runtime.InteropServices.Marshal]::ReadIntPtr($buf)
$sidStr = ""
[void] [IntProbe.Win32]::ConvertSidToStringSid($sidPtr, [ref] $sidStr)

$label = switch ($sidStr) {
    "S-1-16-4096"  { "Low (cannot write %APPDATA%)" }
    "S-1-16-8192"  { "Untrusted" }
    "S-1-16-16384" { "Medium (can write %APPDATA%)" }
    "S-1-16-12288" { "Medium Plus" }
    "S-1-16-20480" { "High" }
    default        { "level RID = " + ($sidStr -split '-')[-1] }
}

"PID $pidNew  SID $sidStr  integrity: $label"

[void] [Runtime.InteropServices.Marshal]::FreeHGlobal($buf)
[void] [IntProbe.Win32]::CloseHandle($tok)
[void] [IntProbe.Win32]::CloseHandle($h)

Stop-Process -Id $pidNew -Force
"process stopped"
