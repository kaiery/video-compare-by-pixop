# Exercise the real GUI executable using window messages. No video engine is launched.
[CmdletBinding()]
param([switch]$LoadHelpersOnly)
$ErrorActionPreference = 'Stop'
$guiRoot = Split-Path -Parent $PSScriptRoot
$evidence = Join-Path $guiRoot 'work/p03'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class InputProbe {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left,Top,Right,Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out Rect rect);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd,IntPtr hdc,uint flags);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd,int command);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hwnd,int x,int y,int width,int height,bool repaint);
    public delegate bool Callback(IntPtr hwnd, IntPtr param);
    [DllImport("user32.dll")] static extern bool EnumWindows(Callback callback, IntPtr param);
    [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr hwnd,Callback callback,IntPtr param);
    [DllImport("user32.dll")] static extern IntPtr GetParent(IntPtr hwnd);
    [DllImport("user32.dll")] static extern IntPtr SendMessageTimeout(IntPtr hwnd,uint msg,IntPtr wp,IntPtr lp,uint flags,uint timeout,out IntPtr result);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
    [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] static extern IntPtr SetText(IntPtr hwnd, uint msg, IntPtr wp, string text);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] static extern IntPtr ReadText(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder text);
    [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll")] static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size, uint type, uint protect);
    [DllImport("kernel32.dll")] static extern bool WriteProcessMemory(IntPtr process, IntPtr address, byte[] bytes, UIntPtr size, out UIntPtr written);
    [DllImport("kernel32.dll")] static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size, uint type);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    public static IntPtr Find(uint processId, string name) {
        IntPtr found=IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr unused) {
            uint pid; GetWindowThreadProcessId(hwnd,out pid);
            var cls=new StringBuilder(256); GetClassName(hwnd,cls,256);
            if(pid==processId && cls.ToString()==name) {found=hwnd;return false;} return true;
        },IntPtr.Zero); return found;
    }
    public static void Put(IntPtr hwnd, string text) {SetText(hwnd,12,IntPtr.Zero,text);}
    public static bool Responsive(IntPtr hwnd) {IntPtr result;return SendMessageTimeout(hwnd,0,IntPtr.Zero,IntPtr.Zero,2,1000,out result)!=IntPtr.Zero;}
    public static IntPtr FindControlWindow(uint processId,int id) {
        IntPtr found=IntPtr.Zero;
        EnumWindows(delegate(IntPtr h,IntPtr unused){uint pid;GetWindowThreadProcessId(h,out pid);if(pid==processId && GetDlgItem(h,id)!=IntPtr.Zero){found=h;return false;}return true;},IntPtr.Zero);return found;
    }
    public static IntPtr FindCaption(uint processId,string caption) {
        IntPtr found=IntPtr.Zero;
        EnumWindows(delegate(IntPtr h,IntPtr unused){uint pid;GetWindowThreadProcessId(h,out pid);if(pid==processId && Text(h)==caption){found=h;return false;}return true;},IntPtr.Zero);return found;
    }
    public static IntPtr FileNameEdit(IntPtr dialog) {
        IntPtr found=IntPtr.Zero;
        EnumChildWindows(dialog,delegate(IntPtr h,IntPtr unused){
            var c=new StringBuilder(64);var p=new StringBuilder(64);GetClassName(h,c,64);GetClassName(GetParent(h),p,64);
            if(c.ToString()=="Edit"&&p.ToString()=="ComboBox"){found=h;return false;}return true;
        },IntPtr.Zero);return found;
    }
    public static string Text(IntPtr hwnd) {var b=new StringBuilder(1048576);ReadText(hwnd,13,(IntPtr)b.Capacity,b);return b.ToString();}
    // LVM_SETITEMSTATE is above WM_USER and therefore needs memory in the target process.
    public static void State(IntPtr list,int row,uint state,uint mask) {
        uint pid;GetWindowThreadProcessId(list,out pid);IntPtr p=OpenProcess(0x28,false,pid);
        if(p==IntPtr.Zero)throw new Exception("Cannot open test process.");
        IntPtr mem=IntPtr.Zero;
        try {
            byte[] data=new byte[88];Array.Copy(BitConverter.GetBytes(state),0,data,12,4);Array.Copy(BitConverter.GetBytes(mask),0,data,16,4);
            mem=VirtualAllocEx(p,IntPtr.Zero,(UIntPtr)data.Length,0x3000,4);UIntPtr written;
            if(mem==IntPtr.Zero || !WriteProcessMemory(p,mem,data,(UIntPtr)data.Length,out written))throw new Exception("Cannot write LVITEM.");
            SendMessage(list,0x102b,(IntPtr)row,mem);
        } finally {if(mem!=IntPtr.Zero)VirtualFreeEx(p,mem,UIntPtr.Zero,0x8000);CloseHandle(p);}
    }
}
'@
function Wait-Window([int]$ProcessId,[string]$Class='VideoCompareGUI.MainWindow') {
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do {
        $h=[InputProbe]::Find($ProcessId,$Class)
        if($h -ne [IntPtr]::Zero){return $h}
        Start-Sleep -Milliseconds 50
    } while([DateTime]::UtcNow -lt $deadline)
    throw "Missing window: $Class"
}
function Control([int]$Id) { [InputProbe]::GetDlgItem($script:window,$Id) }
function Command([int]$Id) {
    [InputProbe]::SendMessage($script:window,0x111,[IntPtr]$Id,(Control $Id)) | Out-Null
}
function Select-Combo([IntPtr]$Parent,[int]$Id,[int]$Value) {
    $h=[InputProbe]::GetDlgItem($Parent,$Id)
    [InputProbe]::SendMessage($h,0x14e,[IntPtr]$Value,[IntPtr]::Zero) | Out-Null
    [InputProbe]::SendMessage($Parent,0x111,[IntPtr]($Id+65536),$h) | Out-Null
}
function Preview { Command 1041; [InputProbe]::Text((Control 1040)) }
function Assert([bool]$Pass,[string]$Name) {
    if(-not $Pass){throw "FAIL: $Name`n$([InputProbe]::Text((Control 1040)))"}
    $script:checks.Add($Name); Write-Output "PASS: $Name"
}
function Edit-Input([string]$Path,[int]$Kind=0,[int]$Mode=0,[string]$Filter='', [switch]$Existing,[switch]$Cancel) {
    $id=if($Existing){1023}else{1022}
    [InputProbe]::PostMessage($script:window,0x111,[IntPtr]$id,[IntPtr]::Zero) | Out-Null
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do {$dialog=[InputProbe]::FindControlWindow($script:process.Id,1101);if($dialog -ne [IntPtr]::Zero){break};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline)
    if($dialog -eq [IntPtr]::Zero){throw 'Input editor did not open.'}
    [InputProbe]::Put([InputProbe]::GetDlgItem($dialog,1101),$Path)
    Select-Combo $dialog 1103 $Kind
    Select-Combo $dialog 1104 $Mode
    [InputProbe]::Put([InputProbe]::GetDlgItem($dialog,1105),$Filter)
    $button=if($Cancel){2}else{1}
    [InputProbe]::SendMessage($dialog,0x111,[IntPtr]$button,[IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 80
    if([InputProbe]::FindControlWindow($script:process.Id,1101) -ne [IntPtr]::Zero){throw 'Edit dialog rejected test input.'}
}
function Select-Row([int]$Row) {
    [InputProbe]::State((Control 1020),-1,0,3)
    [InputProbe]::State((Control 1020),$Row,3,3)
}
function Save-InputImage([string]$Name) {
    [InputProbe]::ShowWindow($script:window,4)|Out-Null
    $rect=New-Object InputProbe+Rect
    [InputProbe]::GetWindowRect($script:window,[ref]$rect)|Out-Null
    $bitmap=New-Object Drawing.Bitmap ($rect.Right-$rect.Left),($rect.Bottom-$rect.Top)
    $graphics=[Drawing.Graphics]::FromImage($bitmap); $dc=$graphics.GetHdc()
    try {
        if(-not [InputProbe]::PrintWindow($script:window,$dc,2)){throw 'Cannot capture GUI.'}
    } finally {$graphics.ReleaseHdc($dc);$graphics.Dispose()}
    try {$bitmap.Save((Join-Path $evidence $Name),[Drawing.Imaging.ImageFormat]::Png)}finally{$bitmap.Dispose()}
}
if($LoadHelpersOnly){return}
$results=@()
foreach($config in @('Debug','Release','Installed')) {
    $exe=if($config -eq 'Installed'){Join-Path $guiRoot 'dist/video-compare-gui.exe'}else{Join-Path $guiRoot "build/msvc-x64/bin/$config/video-compare-gui.exe"}
    $script:checks=New-Object 'System.Collections.Generic.List[string]'
    $script:process=Start-Process -FilePath $exe -WorkingDirectory $evidence -WindowStyle Hidden -PassThru
    try {
        if(-not $process.WaitForInputIdle(8000)){throw 'GUI did not become ready.'}
        $script:window=Wait-Window $process.Id
        foreach($id in @(1001,1003,1010,1012,1020,1021,1022,1023,1024,1025,1026,1027,1030,1031,1032,1033,1034,1035,1036,1037,1038,1039,1040,1041,1042,1043,1044)) {
            if((Control $id) -eq [IntPtr]::Zero){throw "Missing P03 control $id"}
        }
        Assert (-not [InputProbe]::IsWindowEnabled((Control 1043))) 'Empty session cannot launch the engine'
        Assert (-not [InputProbe]::IsWindowEnabled((Control 1042))) 'Empty session cannot be copied'
        [InputProbe]::Put((Control 1001),(Join-Path $evidence 'engine.exe'))
        [InputProbe]::Put((Control 1010),(Join-Path $evidence '参考 & (原片)#.mp4'))
        Edit-Input 'first.mp4' -Mode 1 -Filter 'hflip'
        $p=Preview
        Assert ($p.Contains('first.mp4::filters=hflip') -and $p.Contains('参考 & (原片)#.mp4')) '1+1 Unicode and filter preview'
        Edit-Input 'https://example.invalid/second.mp4' -Kind 2
        Edit-Input 'frames/%04d.png' -Kind 1
        $p=Preview
        Assert ($p.Contains('first.mp4::filters=hflip') -and $p.Contains('https://example.invalid/second.mp4') -and $p.Contains('%04d.png')) '1+3 mixed input types'
        if($config -eq 'Installed'){Save-InputImage 'one-plus-three.png'}
        Edit-Input 'cancelled.mp4' -Cancel
        Assert ((Preview) -notlike '*cancelled.mp4*') 'Cancel does not commit an input'
        Select-Row 0; Command 1027
        $p=Preview
        Assert ($p.IndexOf('https://example.invalid/second.mp4') -lt $p.IndexOf('first.mp4::filters=hflip')) 'Move down preserves per-input filter'
        Command 1026
        Assert ((Preview).IndexOf('first.mp4::filters=hflip') -lt (Preview).IndexOf('https://example.invalid/second.mp4')) 'Move up restores order'
        Command 1024
        Assert ([regex]::Matches((Preview),'first\.mp4::filters=hflip').Count -eq 2) 'Duplicate retains independent settings'
        Edit-Input 'duplicate.mp4' -Mode 2 -Filter 'vflip' -Existing
        $p=Preview
        Assert ($p.Contains('first.mp4::filters=hflip') -and $p.Contains('duplicate.mp4::filters=vflip')) 'Editing duplicate leaves original unchanged'
        [InputProbe]::State((Control 1020),3,0x1000,0xf000)
        Assert (-not (Preview).Contains('duplicate.mp4')) 'Disabled input is omitted'
        [InputProbe]::State((Control 1020),3,0x2000,0xf000)
        Assert ((Preview).Contains('duplicate.mp4::filters=vflip')) 'Re-enable restores input and filter'
        Select-Combo $window 1030 1; Select-Combo $window 1034 2
        Select-Combo $window 1031 2; [InputProbe]::Put((Control 1033),'')
        [InputProbe]::Put((Control 1039),'x25/24+0.1')
        foreach($id in @(1035,1036,1037,1038)){[InputProbe]::SendMessage((Control $id),0xf1,[IntPtr]1,[IntPtr]::Zero)|Out-Null}
        $p=Preview
        foreach($value in @('--mode','hstack','--auto-loop-mode','pp','1280x','x25/24+0.1','--subtraction-mode','--high-dpi','--10-bpc','--fullscreen')) {
            Assert ($p.Contains($value)) "Common option: $value"
        }
        [InputProbe]::Put((Control 1032),'bad')
        $null=Preview; Assert (-not [InputProbe]::IsWindowEnabled((Control 1042))) 'Invalid window size blocks copy'
        Select-Combo $window 1031 1
        $p=Preview; Assert ($p.Contains('--window-fit-display') -and -not $p.Contains('--window-size')) 'Fit and custom size are mutually exclusive'
        Select-Row 1; Edit-Input '' -Kind 4 -Mode 3 -Existing
        Assert ((Preview).Contains('参考 & (原片)#.mp4::filters=')) 'Reference input and clear-filter mode'
        Select-Combo $window 1030 2; Select-Combo $window 1034 1
        Assert ((Preview).Contains('vstack') -and (Preview).Contains('"on"')) 'Vertical layout and forward loop'
        Select-Combo $window 1031 2
        [InputProbe]::Put((Control 1032),''); [InputProbe]::Put((Control 1033),'720')
        Assert ((Preview).Contains('x720')) 'Height-only custom size'
        Select-Combo $window 1031 0; [InputProbe]::Put((Control 1039),'')
        foreach($id in @(1035,1036,1037,1038)){[InputProbe]::SendMessage((Control $id),0xf1,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null}
        $p=Preview
        Assert (-not $p.Contains('--window-') -and -not $p.Contains('--time-shift') -and -not $p.Contains('--fullscreen') -and -not $p.Contains('--high-dpi') -and -not $p.Contains('--subtraction-mode') -and -not $p.Contains('--10-bpc')) 'Reset optional controls removes flags'
        [InputProbe]::State((Control 1020),-1,2,2); Command 1025
        $null=Preview
        Assert ([InputProbe]::SendMessage((Control 1020),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 0) 'Multi-select removal deletes all selected inputs'
        Assert (-not [InputProbe]::IsWindowEnabled((Control 1042))) 'No enabled right input blocks copy'
        [InputProbe]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
        if(-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0){throw 'GUI failed to exit cleanly.'}
        $results += [pscustomobject]@{configuration=$config;status='PASS';checks=$checks.ToArray();sha256=(Get-FileHash $exe).Hash}
    } finally {
        if(-not $process.HasExited){$process.CloseMainWindow()|Out-Null;if(-not $process.WaitForExit(2000)){$process.Kill();$process.WaitForExit()}}
        $process.Dispose()
    }
}
$results | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $evidence 'input-check.json') -Encoding UTF8
Write-Output "Evidence: $evidence"
