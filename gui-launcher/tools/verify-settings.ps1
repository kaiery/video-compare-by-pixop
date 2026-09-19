# Real EXE integration and screenshots. Exhaustive values are tested by ui.settings.
[CmdletBinding()]
param([switch]$LoadHelpersOnly)
$ErrorActionPreference='Stop'
$settingsHelpersOnly=$LoadHelpersOnly
. (Join-Path $PSScriptRoot 'verify-inputs.ps1') -LoadHelpersOnly
$evidence=Join-Path $guiRoot 'work/p04'
New-Item -ItemType Directory -Force -Path $evidence|Out-Null
function Setting-Control([IntPtr]$Dialog,[int]$Id) {
    $h=[InputProbe]::GetDlgItem($Dialog,$Id)
    if($h -eq [IntPtr]::Zero){$h=[InputProbe]::GetDlgItem([InputProbe]::GetDlgItem($Dialog,1201),$Id)}
    if($h -eq [IntPtr]::Zero){throw "Missing settings control $Id"}
    return $h
}
function Wait-Settings {
    $deadline=[DateTime]::UtcNow.AddSeconds(8)
    do {
        $h=[InputProbe]::FindControlWindow($script:process.Id,1201)
        if($h -ne [IntPtr]::Zero -and [InputProbe]::GetDlgItem($h,1201) -ne [IntPtr]::Zero -and [InputProbe]::IsWindowVisible($h)){return $h}
        Start-Sleep -Milliseconds 50
    }while([DateTime]::UtcNow -lt $deadline)
    throw 'Settings dialog did not become ready.'
}
function Settings-Command([IntPtr]$Dialog,[int]$Id) {
    [InputProbe]::SendMessage($Dialog,0x111,[IntPtr]$Id,[IntPtr]::Zero)|Out-Null
}
function Set-Setting([IntPtr]$Dialog,[int]$Number,[string]$Value,[int]$Offset=0) {
    $id=2000+($Number-1)*10+$Offset
    $h=Setting-Control $Dialog $id
    [InputProbe]::SendMessage($h,0xf1,[IntPtr]1,[IntPtr]::Zero)|Out-Null
    Settings-Command $Dialog $id
    [InputProbe]::Put((Setting-Control $Dialog ($id+1)),$Value)
}
function Settings-Page([IntPtr]$Dialog,[int]$Page) {
    $h=Setting-Control $Dialog 1200
    [InputProbe]::SendMessage($h,0x186,[IntPtr]$Page,[IntPtr]::Zero)|Out-Null
    [InputProbe]::SendMessage($Dialog,0x111,[IntPtr](1200+65536),$h)|Out-Null
}
function Snapshot([IntPtr]$Dialog,[string]$Name) {
    $saved=$script:window
    try{$script:window=$Dialog;Save-InputImage $Name}finally{$script:window=$saved}
}
function Set-Override([IntPtr]$Dialog,[int]$Number,[int]$Mode,[string]$Value) {
    $id=4000+($Number-1)*10
    $h=Setting-Control $Dialog $id
    [InputProbe]::SendMessage($h,0x14e,[IntPtr]$Mode,[IntPtr]::Zero)|Out-Null
    [InputProbe]::SendMessage($Dialog,0x111,[IntPtr]($id+65536),$h)|Out-Null
    [InputProbe]::Put((Setting-Control $Dialog ($id+1)),$Value)
}
if($settingsHelpersOnly){return}
$results=@()
foreach($config in @('Debug','Release','Installed')) {
    $exe=if($config -eq 'Installed'){Join-Path $guiRoot 'dist/video-compare-gui.exe'}else{Join-Path $guiRoot "build/msvc-x64/bin/$config/video-compare-gui.exe"}
    $script:checks=New-Object 'System.Collections.Generic.List[string]'
    $script:process=Start-Process -FilePath $exe -WorkingDirectory $evidence -WindowStyle Hidden -PassThru
    try {
        if(-not $process.WaitForInputIdle(8000)){throw 'GUI not ready.'}
        $script:window=Wait-Window $process.Id
        [InputProbe]::Put((Control 1001),(Join-Path $evidence 'engine.exe'))
        [InputProbe]::Put((Control 1010),(Join-Path $evidence 'left.mp4'))
        Edit-Input 'right.mp4'
        [InputProbe]::PostMessage($window,0x111,[IntPtr]1045,[IntPtr]::Zero)|Out-Null
        $dialog=Wait-Settings
        Assert ([InputProbe]::SendMessage((Setting-Control $dialog 1200),0x18b,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -eq 9) 'Nine settings categories'
        $mainNumbers=@(6,7,10,13,15,16,17,20,22)
        foreach($number in 1..61) {
            if($number -in $mainNumbers){continue}
            $id=if($number -eq 5){3100}else{2000+($number-1)*10}
            $null=Setting-Control $dialog $id
        }
        Assert $true 'All 52 non-main CLI options have controls'
        Set-Setting $dialog 21 '150'
        Set-Setting $dialog 24 'bt2020nc'
        Set-Setting $dialog 24 'bt709' 2
        Set-Setting $dialog 32 'scale=640:-2'
        Set-Setting $dialog 34 '__,format=gray'
        if($config -eq 'Installed') {
            foreach($page in 0..8){Settings-Page $dialog $page;Snapshot $dialog "category-$page.png"}
            Settings-Page $dialog 2
            [InputProbe]::SendMessage((Setting-Control $dialog 1201),0x115,[IntPtr]7,[IntPtr]::Zero)|Out-Null
            Snapshot $dialog 'color-scrolled.png'
            [InputProbe]::MoveWindow($dialog,50,50,960,640,$true)|Out-Null
            Snapshot $dialog 'settings-resized.png'
        }
        Settings-Command $dialog 1
        $p=Preview
        Assert ($p.Contains('150') -and $p.Contains('bt709:bt2020nc') -and $p.Contains('scale=640:-2') -and $p.Contains('__,format=gray')) 'Advanced values reach main-session preview with separate scopes'
        [InputProbe]::PostMessage($window,0x111,[IntPtr]1045,[IntPtr]::Zero)|Out-Null
        $dialog=Wait-Settings
        Assert ([InputProbe]::Text((Setting-Control $dialog 2311)) -eq 'scale=640:-2') 'Reopened dialog retains common filter'
        Set-Setting $dialog 32 'cancelled-filter'
        Settings-Command $dialog 2
        Assert (-not (Preview).Contains('cancelled-filter')) 'Cancel leaves main session unchanged'
        Select-Row 0
        [InputProbe]::PostMessage($window,0x111,[IntPtr]1023,[IntPtr]::Zero)|Out-Null
        $inputDialog=Wait-Window $process.Id '#32770'
        [InputProbe]::PostMessage($inputDialog,0x111,[IntPtr]1107,[IntPtr]::Zero)|Out-Null
        $dialog=Wait-Settings
        foreach($number in 1..11) {
            $id=4000+($number-1)*10
            $expected=if($number -eq 1){4}else{3}
            if([InputProbe]::SendMessage((Setting-Control $dialog $id),0x146,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne $expected){throw "Wrong override modes: RV-$number"}
        }
        Assert $true 'All 11 right-video fields; append only for filters'
        Set-Override $dialog 1 2 'hflip'
        Set-Override $dialog 6 1 'h264'
        Set-Override $dialog 10 1 '850'
        Settings-Command $dialog 1203
        if($config -eq 'Installed') {
            Snapshot $dialog 'overrides-top.png'
            [InputProbe]::SendMessage((Setting-Control $dialog 1201),0x115,[IntPtr]7,[IntPtr]::Zero)|Out-Null
            Snapshot $dialog 'overrides-bottom.png'
        }
        Settings-Command $dialog 1
        Assert ([InputProbe]::Text([InputProbe]::GetDlgItem($inputDialog,1105)) -eq 'hflip') 'Nested editor synchronizes inline filter'
        Settings-Command $inputDialog 1
        $p=Preview
        Assert ($p.Contains('::filters=__,hflip') -and $p.Contains('::decoder=h264') -and $p.Contains('::peak-nits=850')) 'Nested 11-field editor commits into right-video spec'
        Command 1024;Command 1026
        $p=Preview
        Assert ([regex]::Matches($p,'::peak-nits=850').Count -eq 2 -and [regex]::Matches($p,'::decoder=h264').Count -eq 2) 'Duplicate and reorder retain all independent settings'
        Assert (-not [InputProbe]::IsWindowEnabled((Control 1043))) 'Missing engine keeps Start disabled'
        [InputProbe]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
        if(-not $process.WaitForExit(5000) -or $process.ExitCode -ne 0){throw 'GUI did not close normally.'}
        $results += [pscustomobject]@{configuration=$config;status='PASS';checks=$checks.ToArray();sha256=(Get-FileHash $exe).Hash}
    } finally {
        if(-not $process.HasExited){$process.CloseMainWindow()|Out-Null;if(-not $process.WaitForExit(2000)){$process.Kill();$process.WaitForExit()}}
        $process.Dispose()
    }
}
$results|ConvertTo-Json -Depth 5|Set-Content (Join-Path $evidence 'settings-check.json') -Encoding UTF8
Write-Output "Evidence: $evidence"
