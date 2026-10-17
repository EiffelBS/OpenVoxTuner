# Capture the bottom row with its left title spines at a chosen language and fold state
# (tools/capture_title_bands.ps1)
#
# Synthetic input is blocked on this machine, so UI state can only arrive through the persisted settings file.
# Measured split of what that patching CAN and CANNOT prove, from probes run with this tool:
#   - The four root attributes (advancedExpanded / reverbExpanded / harmonyExpanded / abActiveSlot) DO land: a capture
#     launched after patching shows the row folded exactly as written.
#   - Persisted PARAMETER values do NOT reach the editor at launch: a state carrying scale="3.0" rendered Scale
#     "Chromatic", and a state carrying ui_language="5.0" (Chinese) rendered English labels, while the same file was
#     saved back verbatim on close. So -Lang writes the language into the file and changes nothing on screen: a
#     foreign-language row can only be captured by driving the language menu by hand in the running app, which no
#     automated capture here can do. Locale fit is proven offline in eiffelbs-ui tests/Smoke.cpp instead.
# Each capture therefore proves two things at once: the layout in the patched fold state, and that patching the root
# flags lands the row where the code says it should.
#
# 1132 x 638 is the outer size that lands on the minimum client width (plugin limits 1100 x 600): the bottom row is
# under maximum pressure there, which is exactly where a spine can collide with a knob. A wide size such as 1400 shows
# every frame at its own requested width with no scrolling, which proves a long translated title fits its spine.
#
# Run it as: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\capture_title_bands.ps1 -Lang 1 -Out .\bands.png -Crop .\bands_row.png

param([int]$Lang = 0, [int]$ReverbExpanded = 0, [int]$HarmonyExpanded = 0, [int]$AdvancedExpanded = 0,
      [string]$Out = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\.dbg\bands.png',
      [string]$Crop = '', [int]$Width = 1132, [int]$Height = 638)

$exe = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe'

# A running instance owns the settings file and rewrites it on exit, so patching must happen while nothing is up.
Get-Process -Name OpenVoxTuner -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

& "$PSScriptRoot\set_standalone_language.ps1" -Lang $Lang
& "$PSScriptRoot\set_standalone_fold.ps1" -Attr advancedExpanded -Value $AdvancedExpanded
& "$PSScriptRoot\set_standalone_fold.ps1" -Attr harmonyExpanded -Value $HarmonyExpanded
& "$PSScriptRoot\set_standalone_fold.ps1" -Attr reverbExpanded -Value $ReverbExpanded

Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $exe } | Out-Null
Start-Sleep -Seconds 6

Add-Type -AssemblyName System.Drawing
$src = @'
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public class CapBd {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int w, int hh, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
  public static string Save(IntPtr h, string path) {
    if (h == IntPtr.Zero) return "NO WINDOW HANDLE";
    RECT r; GetWindowRect(h, out r);
    int w = r.right - r.left, hh = r.bottom - r.top;
    if (w < 50 || hh < 50) return "TOO SMALL " + w + "x" + hh;
    Bitmap bmp = new Bitmap(w, hh);
    Graphics g = Graphics.FromImage(bmp);
    IntPtr hdc = g.GetHdc();
    bool ok = PrintWindow(h, hdc, 2);   // PW_RENDERFULLCONTENT: JUCE paints, DWM does not
    g.ReleaseHdc(hdc);
    g.Dispose();
    bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    bmp.Dispose();
    return "saved " + w + "x" + hh + " printwindow=" + ok + " -> " + path;
  }
  public static string Shrink(IntPtr h, int w, int hh) {
    const uint SWP_NOMOVE = 2, SWP_NOZORDER = 4;
    bool ok = SetWindowPos(h, IntPtr.Zero, 0, 0, w, hh, SWP_NOMOVE | SWP_NOZORDER);
    RECT r; GetWindowRect(h, out r);
    return "setwindowpos=" + ok + " now=" + (r.right - r.left) + "x" + (r.bottom - r.top);
  }
}
'@
Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing.dll

$p = Get-Process -Name OpenVoxTuner -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $p) { "NO PROCESS"; exit 1 }
"PID=$($p.Id) title=$($p.MainWindowTitle)"
[CapBd]::Shrink($p.MainWindowHandle, $Width, $Height)
Start-Sleep -Seconds 2
[CapBd]::Save($p.MainWindowHandle, $Out)
if ($Crop -ne '') {
  # A crop cannot be written from inside this script. The child powershell.exe that runs a -File script on this box is
  # confined: System.Drawing constructors and Image::FromFile come back as strings (the first run printed
  # "cropped x200", the width was empty), so FromImage/DrawImage/Save all fail while the identical recipe works when it
  # runs directly in the pwsh harness. Crop after the capture with that proven inline recipe, passing $Crop is inert.
  Write-Output "CROP_SKIPPED (crop the row from $Out inside the pwsh harness; $Crop was not written)"
}
