# P01 smoke check. Only launches and closes the exact executables listed below.
[CmdletBinding()]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$EvidenceName = 'p01',
    [ValidateSet('Debug', 'Release', 'Installed')][string[]]$Configurations = @('Debug', 'Release', 'Installed')
)
$ErrorActionPreference = 'Stop'
$guiRoot = Split-Path -Parent $PSScriptRoot
$evidence = Join-Path $guiRoot "work/$EvidenceName"
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class LauncherWindowProbe {
    public delegate bool Callback(IntPtr hwnd, IntPtr param);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool EnumWindows(Callback callback, IntPtr param);
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr hwnd, Callback callback, IntPtr param);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hwnd, int x, int y, int width, int height, bool repaint);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wparam, IntPtr lparam);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool AreDpiAwarenessContextsEqual(IntPtr a, IntPtr b);
    public static IntPtr Find(uint processId, string className) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr unused) {
            uint pid; GetWindowThreadProcessId(hwnd, out pid);
            var text = new StringBuilder(256); GetClassName(hwnd, text, text.Capacity);
            if (pid == processId && text.ToString() == className) { found = hwnd; return false; }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static int ChildCount(IntPtr hwnd) {
        int count = 0;
        EnumChildWindows(hwnd, delegate(IntPtr child, IntPtr unused) { count++; return true; }, IntPtr.Zero);
        return count;
    }
}
'@

function Wait-Window([int]$ProcessId, [string]$ClassName) {
    $deadline = [DateTime]::UtcNow.AddSeconds(8)
    do {
        $handle = [LauncherWindowProbe]::Find($ProcessId, $ClassName)
        if ($handle -ne [IntPtr]::Zero) { return $handle }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "Window $ClassName did not appear for PID $ProcessId."
}

function Save-WindowImage([IntPtr]$Handle, [string]$Path) {
    $rect = New-Object LauncherWindowProbe+Rect
    if (-not [LauncherWindowProbe]::GetWindowRect($Handle, [ref]$rect)) { throw 'Cannot get window bounds.' }
    $bitmap = New-Object Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $dc = $graphics.GetHdc()
    try {
        if (-not [LauncherWindowProbe]::PrintWindow($Handle, $dc, 2)) { throw 'PrintWindow failed.' }
    } finally { $graphics.ReleaseHdc($dc); $graphics.Dispose() }
    try { $bitmap.Save($Path, [Drawing.Imaging.ImageFormat]::Png) } finally { $bitmap.Dispose() }
}

$results = @()
foreach ($config in $Configurations) {
    $exe = if ($config -eq 'Installed') { Join-Path $guiRoot 'dist/video-compare-gui.exe' } else {
        Join-Path $guiRoot "build/msvc-x64/bin/$config/video-compare-gui.exe"
    }
    if (-not (Test-Path -LiteralPath $exe)) { throw "Build first: $exe" }
    $bytes = [IO.File]::ReadAllBytes($exe)
    $pe = [BitConverter]::ToInt32($bytes, 0x3c)
    $machine = [BitConverter]::ToUInt16($bytes, $pe + 4)
    $subsystem = [BitConverter]::ToUInt16($bytes, $pe + 24 + 68)
    if ($machine -ne 0x8664 -or $subsystem -ne 2) { throw "$config is not an x64 Windows GUI binary." }
    $process = Start-Process -FilePath $exe -WorkingDirectory $evidence -WindowStyle Hidden -PassThru
    try {
        if (-not $process.WaitForInputIdle(8000)) { throw 'GUI did not become ready.' }
        $window = Wait-Window $process.Id 'VideoCompareGUI.MainWindow'
        # Show only the tested GUI without taking foreground focus, for visual evidence.
        [LauncherWindowProbe]::ShowWindow($window, 4) | Out-Null
        $title = New-Object Text.StringBuilder 256
        [LauncherWindowProbe]::GetWindowText($window, $title, $title.Capacity) | Out-Null
        if ($title.ToString() -notlike 'Video Compare GUI*') { throw 'Incorrect window title.' }
        $children = [LauncherWindowProbe]::ChildCount($window)
        if ($children -lt 4) { throw "Expected at least 4 child controls, found $children." }
        $dpi = [LauncherWindowProbe]::GetDpiForWindow($window)
        $perMonitorV2 = [LauncherWindowProbe]::AreDpiAwarenessContextsEqual(
            [LauncherWindowProbe]::GetWindowDpiAwarenessContext($window), [IntPtr](-4))
        if (-not $perMonitorV2) { throw 'PerMonitorV2 manifest did not take effect.' }
        Start-Sleep -Milliseconds 250
        Save-WindowImage $window (Join-Path $evidence "$config-window.png")
        [LauncherWindowProbe]::MoveWindow($window, 80, 80, 956, 600, $true) | Out-Null
        Start-Sleep -Milliseconds 150
        Save-WindowImage $window (Join-Path $evidence "$config-resized.png")
        [LauncherWindowProbe]::PostMessage($window, 0x115, [IntPtr]7, [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 150
        Save-WindowImage $window (Join-Path $evidence "$config-scrolled.png")
        [LauncherWindowProbe]::PostMessage($window, 0x111, [IntPtr]40002, [IntPtr]::Zero) | Out-Null
        $about = Wait-Window $process.Id '#32770'
        [LauncherWindowProbe]::PostMessage($about, 0x111, [IntPtr]1, [IntPtr]::Zero) | Out-Null
        Start-Sleep -Milliseconds 150
        if ($config -eq 'Debug') {
            [LauncherWindowProbe]::PostMessage($window, 0x111, [IntPtr]40001, [IntPtr]::Zero) | Out-Null
        } else {
            [LauncherWindowProbe]::PostMessage($window, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        }
        if (-not $process.WaitForExit(5000)) { throw 'GUI failed to close normally.' }
        if ($process.ExitCode -ne 0) { throw "GUI exit code: $($process.ExitCode)" }
        $results += [pscustomobject]@{
            configuration=$config; executable=$exe; title=$title.ToString(); dpi=$dpi;
            perMonitorV2=$perMonitorV2; childControls=$children; aboutDialog='PASS';
            resize='PASS'; exitCode=$process.ExitCode; machine='x64'; subsystem='Windows GUI';
            sha256=(Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash; status='PASS'
        }
    } finally {
        if (-not $process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if (-not $process.WaitForExit(2000)) { $process.Kill(); $process.WaitForExit() }
        }
        $process.Dispose()
    }
}
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $evidence 'window-check.json') -Encoding UTF8
$results | Format-Table configuration,status,dpi,perMonitorV2,exitCode
Write-Output "Evidence: $evidence"
