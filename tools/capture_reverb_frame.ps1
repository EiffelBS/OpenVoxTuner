# Capture the Reverb frame at the minimum host width, in a chosen fold state
# (tools/capture_reverb_frame.ps1)
#
# Synthetic input is blocked on this machine, so the fold cannot be clicked: the state is written into the
# settings file with tools/set_standalone_fold.ps1 and read back by the editor's restore path. That makes this
# capture a proof of BOTH things at once - the layout at minimum width, and the persistence round trip.
#
# The plugin's resize limits are 1100 x 600, so 1132 x 638 is the outer size that lands on the minimum client
# width: the row of bottom frames is under the most pressure there, which is where a new frame can overflow.
# Width/Height override that: a wide capture (for example 1400) shows every frame at its own requested width with
# no scrolling, which is what proves a header-row label fits rather than being clipped by an ellipsis.
#
# Run it as: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\capture_reverb_frame.ps1 -Folded 0 -Out .\ui_check_reverb_open.png

param([int]$Folded = 1, [string]$Out = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\ui_check_reverb_folded.png',
      [int]$Width = 1132, [int]$Height = 638)

$exe = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe'
$minW = $Width
$minH = $Height

# A running instance owns the settings file and rewrites it on exit, so patching must happen while nothing is up.
Get-Process -Name OpenVoxTuner -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
& "$PSScriptRoot\set_standalone_fold.ps1" -Attr reverbExpanded -Value $Folded

Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $exe } | Out-Null
Start-Sleep -Seconds 6

Add-Type -AssemblyName System.Drawing
$src = @'
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public class CapRv {
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
[CapRv]::Shrink($p.MainWindowHandle, $minW, $minH)
Start-Sleep -Seconds 2
[CapRv]::Save($p.MainWindowHandle, $Out)
