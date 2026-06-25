Add-Type -AssemblyName System.Drawing
$outDir = "C:\Users\User\Documents\trae_projects\VST3\assets"
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir }

$width = 512
$height = 512
$bmp = New-Object System.Drawing.Bitmap($width, $height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

# Transparent background
$g.Clear([System.Drawing.Color]::Transparent)

# Dark circle background for the app icon
$bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 16, 17, 21))
$g.FillEllipse($bgBrush, 16, 16, 480, 480)

# Accent Circle (Light Blue)
$penBlue = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 26, 154, 240), 40)
$g.DrawEllipse($penBlue, 80, 80, 352, 352)

# White Pitch Curve
$penWhite = New-Object System.Drawing.Pen([System.Drawing.Color]::White, 35)
$penWhite.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$penWhite.EndCap = [System.Drawing.Drawing2D.LineCap]::Round

$p1 = New-Object System.Drawing.PointF(80, 384)
$p2 = New-Object System.Drawing.PointF(200, 384)
$p3 = New-Object System.Drawing.PointF(312, 128)
$p4 = New-Object System.Drawing.PointF(432, 128)
$g.DrawBezier($penWhite, $p1, $p2, $p3, $p4)

$outPath = Join-Path $outDir "icon.png"
$bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose()
$bmp.Dispose()
Write-Host "Icon generated successfully at $outPath" -ForegroundColor Green
