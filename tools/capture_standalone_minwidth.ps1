# Launch the freshly linked standalone, capture it at its opening size, then force the
# minimum width and capture again. The minimum width is where the bottom band is under
# the most pressure: every frame states the width its content asks for, so this is the
# only size at which a row can overflow and scroll.
#
# Run it as: powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\capture_standalone_minwidth.ps1
# (the workspace execution policy refuses an unsigned script invoked with & .\script.ps1).
$exe  = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe'
$outA = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\ui_check_room_default.png'
$outB = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\ui_check_room_min.png'

# The plugin's resize limits are 1100 x 600, and the standalone window adds its own
# frame, so 1132 x 638 is the outer size that lands on the minimum client width.
$minW = 1132
$minH = 638

$existing = Get-Process -Name OpenVoxTuner -ErrorAction SilentlyContinue
if ($null -eq $existing) {
    Invoke-CimMethod -ClassName Win32_Process -MethodName Create -Arguments @{ CommandLine = $exe } | Out-Null
    Start-Sleep -Seconds 6
}

Add-Type -AssemblyName System.Drawing
$src = @'
using System;
using System.Runtime.InteropServices;
using System.Drawing;
public class Cap {
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
[Cap]::Save($p.MainWindowHandle, $outA)
[Cap]::Shrink($p.MainWindowHandle, $minW, $minH)
Start-Sleep -Seconds 2
[Cap]::Save($p.MainWindowHandle, $outB)
