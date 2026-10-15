# Launch the freshly linked standalone and capture its window.
$exe = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\build\OpenVoxTuner_artefacts\Release\Standalone\OpenVoxTuner.exe'
$out = 'C:\Users\User\Documents\trae_projects\OpenVoxTuner\ui_check_deesser.png'

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
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
  public static string Save(IntPtr h, string path) {
    if (h == IntPtr.Zero) return "NO WINDOW HANDLE";
    RECT r; GetWindowRect(h, out r);
    int w = r.right - r.left, hh = r.bottom - r.top;
    if (w < 50 || hh < 50) return "TOO SMALL " + w + "x" + hh;
    Bitmap bmp = new Bitmap(w, hh);
    Graphics g = Graphics.FromImage(bmp);
    IntPtr hdc = g.GetHdc();
    bool ok = PrintWindow(h, hdc, 2);
    g.ReleaseHdc(hdc);
    g.Dispose();
    bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    bmp.Dispose();
    return "saved " + w + "x" + hh + " printwindow=" + ok + " -> " + path;
  }
}
'@
Add-Type -TypeDefinition $src -ReferencedAssemblies System.Drawing.dll

$p = Get-Process -Name OpenVoxTuner -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $p) { "NO PROCESS"; exit 1 }
"PID=$($p.Id) title=$($p.MainWindowTitle)"
[Cap]::Save($p.MainWindowHandle, $out)
