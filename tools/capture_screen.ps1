param([string]$OutDir, [int]$Count = 40, [int]$IntervalMs = 400, [string]$Prefix = "s")
# Saves the primary screen (quarter size) every IntervalMs, Count times, then a contact sheet of them.
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class Dpi { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); }
"@
[Dpi]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory $OutDir -Force | Out-Null
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$w = [int]($b.Width / 4); $h = [int]($b.Height / 4)
$files = @()
for ($i = 0; $i -lt $Count; $i++) {
    $full = New-Object Drawing.Bitmap $b.Width, $b.Height
    $g = [Drawing.Graphics]::FromImage($full); $g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size); $g.Dispose()
    $small = New-Object Drawing.Bitmap $full, $w, $h
    $f = Join-Path $OutDir ("{0}{1:D2}.png" -f $Prefix, $i); $small.Save($f); $files += $f
    $full.Dispose(); $small.Dispose()
    Start-Sleep -Milliseconds $IntervalMs
}
$cols = 8; $rows = [math]::Ceiling($Count / $cols)
$sheet = New-Object Drawing.Bitmap ($w * $cols), ($h * $rows)
$g = [Drawing.Graphics]::FromImage($sheet)
for ($i = 0; $i -lt $files.Count; $i++) {
    $img = [Drawing.Image]::FromFile($files[$i])
    $g.DrawImage($img, ($i % $cols) * $w, [math]::Floor($i / $cols) * $h, $w, $h)
    $g.DrawString("$i", (New-Object Drawing.Font "Arial", 14), [Drawing.Brushes]::Red, ($i % $cols) * $w + 3, [math]::Floor($i / $cols) * $h + 3)
    $img.Dispose()
}
$sheet.Save((Join-Path $OutDir "$Prefix-sheet.png"))
"screen $($b.Width)x$($b.Height), sheet saved"
