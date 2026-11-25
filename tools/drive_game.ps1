<#
Test helper: starts a recomp, presses keys into its window and saves screenshots, so a handoff or a boot redirect can
be checked without anyone at the controller. Keys are the recomps' default keyboard bindings (Enter = Start,
Space = A, LShift = B, Q = Z, E = L, R = R, WASD = stick).

  drive_game.ps1 -Exe <path> -Steps "wait:15","key:Enter","wait:2","shot:title.png",... -OutDir <folder> [-Keep]

Steps: wait:<seconds>, key:<Enter|Space|LShift|E|R|Q|W|A|S|D|Up|Down|Left|Right>[:<hold seconds>],
keys:<key>+<key>:<hold seconds> (held together), shot:<file.png>. The game is closed at the end unless -Keep.
#>
param(
    [Parameter(Mandatory)] [string]$Exe,
    [Parameter(Mandatory)] [string[]]$Steps,
    [Parameter(Mandatory)] [string]$OutDir,
    [switch]$Keep,
    # The folder to start it in (one with assets\): a recomp's root for its build\ exe; the exe's own folder otherwise.
    [string]$WorkDir = "",
    [switch]$Attach
)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GameWin {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  public struct RECT { public int L, T, R, B; }
}
"@
[GameWin]::SetProcessDPIAware() | Out-Null
$vk = @{ Enter = 0x0D; Space = 0x20; LShift = 0xA0; E = 0x45; R = 0x52; Q = 0x51; W = 0x57; A = 0x41; S = 0x53; D = 0x44;
         Up = 0x26; Down = 0x28; Left = 0x25; Right = 0x27 }
New-Item -ItemType Directory $OutDir -Force | Out-Null
$exePath = (Resolve-Path $Exe).Path
if ($WorkDir -eq "") { $WorkDir = Split-Path $exePath }
# -Attach: carry on with a copy of that exe already running (started by this script with -Keep) instead of a new one.
$running = if ($Attach) { Get-Process | Where-Object { $_.Path -eq $exePath } | Select-Object -First 1 } else { $null }
if ($running) {
    $proc = $running
} else {
    $proc = Start-Process $exePath -ArgumentList "--boot" -WorkingDirectory $WorkDir -PassThru `
        -RedirectStandardError (Join-Path $OutDir "stderr.txt") -RedirectStandardOutput (Join-Path $OutDir "stdout.txt")
}

function Get-Handle { $proc.Refresh(); return $proc.MainWindowHandle }
function Send-Keys([string[]]$names, [double]$hold) {
    $h = Get-Handle; [GameWin]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 100
    foreach ($n in $names) { $c = [byte]$vk[$n]; [GameWin]::keybd_event($c, [byte][GameWin]::MapVirtualKey($c, 0), 0, [UIntPtr]::Zero) }
    Start-Sleep -Milliseconds ([int]($hold * 1000))
    foreach ($n in $names) { $c = [byte]$vk[$n]; [GameWin]::keybd_event($c, [byte][GameWin]::MapVirtualKey($c, 0), 2, [UIntPtr]::Zero) }
}
function Save-Shot([string]$name) {
    $h = Get-Handle; $r = New-Object GameWin+RECT; [GameWin]::GetWindowRect($h, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $ht = $r.B - $r.T
    if ($w -le 0) { "no window for $name"; return }
    $bmp = New-Object System.Drawing.Bitmap $w, $ht; $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc(); [GameWin]::PrintWindow($h, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc)
    $small = New-Object System.Drawing.Bitmap $bmp, ([int]($w / 2)), ([int]($ht / 2))
    $small.Save((Join-Path $OutDir $name)); "shot $name"
}
foreach ($step in $Steps) {
    $parts = $step.Split(':')
    if ($proc.HasExited) { "game exited before step $step (code $($proc.ExitCode))"; break }
    switch ($parts[0]) {
        'wait' { Start-Sleep -Milliseconds ([int]([double]$parts[1] * 1000)) }
        'key' { $hold = if ($parts.Count -gt 2) { [double]$parts[2] } else { 0.15 }; Send-Keys @($parts[1]) $hold }
        'keys' { Send-Keys ($parts[1].Split('+')) ([double]$parts[2]) }
        'shot' { Save-Shot $parts[1] }
    }
}
$proc.Refresh()
"running at end: $(-not $proc.HasExited)"
if (-not $Keep -and -not $proc.HasExited) { Stop-Process -Id $proc.Id -Confirm:$false }
