[CmdletBinding()]
param([string]$Engine,[string]$Left,[string]$Right,[string]$Executable,[ValidatePattern('^[A-Za-z0-9_-]+$')][string]$EvidenceName='p05')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'verify-inputs.ps1') -LoadHelpersOnly
$evidence=Join-Path $guiRoot "work/$EvidenceName"
New-Item -ItemType Directory -Force -Path $evidence|Out-Null
$real=-not [string]::IsNullOrEmpty($Engine)
if($Executable -and -not $real){throw 'Custom executable validation requires a real engine and media.'}
if($real -and (-not $Left -or -not $Right)){throw 'Real engine verification requires Left and Right.'}
function Wait-ControlWindow([int]$Id) {
    $deadline=[DateTime]::UtcNow.AddSeconds(10)
    do{$h=[InputProbe]::FindControlWindow($script:process.Id,$Id);if($h -ne [IntPtr]::Zero){return $h};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline)
    throw "Missing window control $Id"
}
function Logs { Wait-ControlWindow 1301 }
function Run-Folder {
    $text=[InputProbe]::Text([InputProbe]::GetDlgItem((Logs),1300))
    $line=($text -split "`r?`n" | Select-Object -Last 1)
    if(-not $line.StartsWith('日志：')){throw "Missing log path: $text"}
    return $line.Substring(3)
}
function Read-Run([string]$Folder) {
    $file=Join-Path $Folder 'state.bin'
    $stream=[IO.File]::Open($file,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete)
    try {$reader=New-Object IO.BinaryReader $stream;return [pscustomobject]@{phase=$reader.ReadUInt32();pid=$reader.ReadUInt32();exitCode=$reader.ReadUInt32()}}finally{$stream.Dispose()}
}
function Wait-Run([string]$Folder,[int[]]$Phases=@(4,5),[int]$Seconds=20) {
    $deadline=[DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        $state=Read-Run $Folder
        if($state.phase -in $Phases){Start-Sleep -Milliseconds 250;return $state}
        if(-not $script:process.HasExited -and -not [InputProbe]::Responsive($script:window)){throw 'GUI stopped responding while engine ran.'}
        Start-Sleep -Milliseconds 100
    }while([DateTime]::UtcNow -lt $deadline)
    throw "Run did not reach phase $Phases"
}
function Start-Task([int]$Id=1043) {Command $Id;Start-Sleep -Milliseconds 100;return (Run-Folder)}
function Log-Command([int]$Id){[InputProbe]::SendMessage((Logs),0x111,[IntPtr]$Id,[IntPtr]::Zero)|Out-Null}
function Close-Choice([int]$Button) {
    [InputProbe]::PostMessage($script:window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
    $deadline=[DateTime]::UtcNow.AddSeconds(5)
    do {$dialog=[InputProbe]::FindCaption($script:process.Id,'关闭启动器');if($dialog -ne [IntPtr]::Zero){break};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline)
    if($dialog -eq [IntPtr]::Zero){throw 'Close choice dialog absent.'}
    [InputProbe]::PostMessage($dialog,0x466,[IntPtr]$Button,[IntPtr]::Zero)|Out-Null
    Start-Sleep -Milliseconds 100
}
function Signal-Run([string]$Folder,[string]$Kind) {
    $name='Local\VideoCompareGUI-'+(Split-Path -Leaf $Folder)+'-'+$Kind
    $event=[Threading.EventWaitHandle]::OpenExisting($name)
    try {$event.Set()|Out-Null}finally{$event.Dispose()}
}
$results=@()
$configs=if($real){@('Installed')}else{@('Debug','Release','Installed')}
foreach($config in $configs) {
    $work=Join-Path $evidence ('ui-'+$(if($real){'real'}else{$config}))
    New-Item -ItemType Directory -Force -Path $work|Out-Null
    $exe=if($config -eq 'Installed'){Join-Path $guiRoot 'dist/video-compare-gui.exe'}else{Join-Path $guiRoot "build/msvc-x64/bin/$config/video-compare-gui.exe"}
    if($Executable){$exe=(Resolve-Path -LiteralPath $Executable).Path}
    $fixtureConfig=if($config -eq 'Installed'){'Release'}else{$config}
    if(-not $real) {
        foreach($mode in @('normal','fail','wait','delay-flood')){Copy-Item -LiteralPath (Join-Path $guiRoot "build/msvc-x64/bin/$fixtureConfig/launcher-engine-fixture.exe") -Destination (Join-Path $work "fixture-$mode.exe") -Force}
        $enginePath=Join-Path $work 'fixture-normal.exe'
        $leftPath=Join-Path $work 'left.mp4';$rightPath=Join-Path $work 'right.mp4'
        [IO.File]::WriteAllText($leftPath,'path fixture');[IO.File]::WriteAllText($rightPath,'path fixture')
    }else{$enginePath=$Engine;$leftPath=$Left;$rightPath=$Right}
    $script:checks=New-Object 'System.Collections.Generic.List[string]'
    $script:process=Start-Process -FilePath $exe -WorkingDirectory $work -WindowStyle Hidden -PassThru
    $folders=New-Object 'System.Collections.Generic.List[string]'
    try {
        if(-not $process.WaitForInputIdle(8000)){throw 'GUI did not initialize.'}
        $script:window=Wait-Window $process.Id
        if($Executable){
            $portableWork=Join-Path (Split-Path -Parent $exe) 'work'
            Assert ([IO.Path]::GetFullPath([InputProbe]::Text((Control 1003))) -eq [IO.Path]::GetFullPath($portableWork)) 'Unpacked GUI uses its own portable work directory'
            $work=$portableWork
            Assert ([InputProbe]::Text((Control 1001)) -eq '') 'Launcher-only package starts without a machine-specific engine path'
        }else{[InputProbe]::Put((Control 1003),$work)}
        # Exercise the user's actual executable selection button and native picker.
        [InputProbe]::PostMessage($window,0x111,[IntPtr]1002,[IntPtr]::Zero)|Out-Null
        $picker=Wait-Window $process.Id '#32770'
        $deadline=[DateTime]::UtcNow.AddSeconds(8)
        do {$field=[InputProbe]::FileNameEdit($picker);if($field -ne [IntPtr]::Zero){break};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline)
        if($field -eq [IntPtr]::Zero){throw 'Native executable picker filename field missing.'}
        # The shell restores the last filename after creating the edit control.
        Start-Sleep -Milliseconds 800
        [InputProbe]::Put($field,$enginePath)
        [InputProbe]::SendMessage($picker,0x111,[IntPtr]1,[IntPtr]::Zero)|Out-Null
        $deadline=[DateTime]::UtcNow.AddSeconds(8)
        do {Start-Sleep -Milliseconds 100;$selected=[InputProbe]::Text((Control 1001))}while(-not $selected -and [DateTime]::UtcNow -lt $deadline)
        if(-not $selected){$saved=$script:window;try{$script:window=$picker;Save-InputImage 'picker-failure.png'}finally{$script:window=$saved};throw "Picker failed to select $enginePath; filename field: $([InputProbe]::Text($field))"}
        Assert ([IO.Path]::GetFullPath($selected) -eq [IO.Path]::GetFullPath($enginePath)) 'Native path selector chooses the existing executable'
        $folder=Start-Task 1046;$folders.Add($folder)
        $state=Wait-Run $folder
        Assert ($state.exitCode -eq 0 -and $state.phase -eq 4) 'Engine version check succeeds without video input'
        [InputProbe]::Put((Control 1010),$leftPath)
        Edit-Input $rightPath
        $null=Preview
        Assert ([InputProbe]::IsWindowEnabled((Control 1043))) 'Valid inputs and selected engine enable Start'
        $folder=Start-Task;$folders.Add($folder)
        if($Executable){Assert ($folder.StartsWith((Join-Path $work 'runs'),[StringComparison]::OrdinalIgnoreCase)) 'Unpacked helper writes logs inside the portable package'}
        if($real) {
            $state=Wait-Run $folder @(1,4,5)
            Assert ($state.phase -eq 1) 'Real comparison process remains running after startup'
            Start-Sleep -Seconds 3
            Assert ([InputProbe]::Responsive($window)) 'GUI responsive during real video comparison'
            $engineProcess=Get-Process -Id $state.pid
            $engineProcess.Refresh()
            Assert ($engineProcess.MainWindowHandle -ne [IntPtr]::Zero) 'Real engine creates a comparison window'
            $saved=$script:window
            try {$script:window=$engineProcess.MainWindowHandle;Save-InputImage 'real-engine-window.png'}finally{$script:window=$saved}
            Log-Command 1302;$state=Wait-Run $folder
            Assert ($state.exitCode -eq 0) 'Real engine closes normally through GUI stop'
        }else{
            $state=Wait-Run $folder
            Assert ($state.exitCode -eq 0) 'GUI comparison launch receives successful exit'
            Assert ([InputProbe]::Text([InputProbe]::GetDlgItem((Logs),1301)).Contains('stderr')) 'Separate stderr appears in GUI log'
            [InputProbe]::Put((Control 1001),(Join-Path $work 'fixture-fail.exe'))
            $folder=Start-Task;$folders.Add($folder);$state=Wait-Run $folder
            Assert ($state.exitCode -eq 23) 'GUI preserves failed exit code and allows retry'
            [InputProbe]::Put((Control 1001),(Join-Path $work 'fixture-wait.exe'))
            $folder=Start-Task;$folders.Add($folder);$null=Wait-Run $folder @(1)
            Assert (-not [InputProbe]::IsWindowEnabled((Control 1043))) 'Start disabled while task runs'
            Close-Choice 2
            Assert (-not $process.HasExited -and (Read-Run $folder).phase -eq 1) 'Cancel closing preserves GUI and engine'
            Log-Command 1302;$state=Wait-Run $folder
            Assert ($state.exitCode -eq 0) 'GUI normal stop closes the owned engine'
            Copy-Item -LiteralPath (Join-Path $work 'fixture-normal.exe') -Destination (Join-Path $work 'fixture-ignore.exe') -Force
            [InputProbe]::Put((Control 1001),(Join-Path $work 'fixture-ignore.exe'))
            $folder=Start-Task;$folders.Add($folder);$null=Wait-Run $folder @(1)
            Assert (-not [InputProbe]::IsWindowEnabled([InputProbe]::GetDlgItem((Logs),1303))) 'Force unavailable before normal stop timeout'
            Log-Command 1302;$null=Wait-Run $folder @(3)
            Assert ([InputProbe]::IsWindowEnabled([InputProbe]::GetDlgItem((Logs),1303))) 'Stop timeout enables explicit force choice'
            [InputProbe]::PostMessage((Logs),0x111,[IntPtr]1303,[IntPtr]::Zero)|Out-Null
            $deadline=[DateTime]::UtcNow.AddSeconds(5)
            do{$confirm=[InputProbe]::FindCaption($process.Id,'结束比较');if($confirm -ne [IntPtr]::Zero){break};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline)
            if($confirm -eq [IntPtr]::Zero){throw 'Force confirmation absent.'}
            [InputProbe]::PostMessage($confirm,0x111,[IntPtr]6,[IntPtr]::Zero)|Out-Null
            $state=Wait-Run $folder
            Assert ($state.exitCode -eq 3221225786) 'Confirmed GUI force ends the owned unresponsive engine'
            [InputProbe]::Put((Control 1001),(Join-Path $work 'fixture-delay-flood.exe'))
            $folder=Start-Task;$folders.Add($folder);$null=Wait-Run $folder @(1)
            Close-Choice 100
            if(-not $process.WaitForExit(5000)){throw 'GUI failed to exit while retaining task.'}
            $state=Wait-Run $folder @(4,5) 20
            Assert ($state.exitCode -eq 0) 'Keep-running close leaves helper draining flood through engine exit'
            $process.Dispose()
            $script:process=Start-Process -FilePath $exe -WorkingDirectory $work -WindowStyle Hidden -PassThru
            if(-not $process.WaitForInputIdle(8000)){throw 'Second GUI did not initialize.'}
            $script:window=Wait-Window $process.Id
            [InputProbe]::Put((Control 1001),(Join-Path $work 'fixture-wait.exe'))
            [InputProbe]::Put((Control 1003),$work)
            [InputProbe]::Put((Control 1010),$leftPath)
            Edit-Input $rightPath
            $folder=Start-Task;$folders.Add($folder);$null=Wait-Run $folder @(1)
            Close-Choice 101
            if(-not $process.WaitForExit(5000)){throw 'End-and-close failed to close GUI.'}
            $state=Wait-Run $folder
            Assert ($state.exitCode -eq 0) 'End-and-close stops engine normally then exits GUI'
        }
        if($real) {
            $saved=$script:window
            try{$script:window=Logs;Save-InputImage 'real-gui-log.png'}finally{$script:window=$saved}
            [InputProbe]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null
            if(-not $process.WaitForExit(5000)){throw 'GUI failed to exit.'}
        }
        $results += [pscustomobject]@{configuration=$config;realEngine=$real;engine=$enginePath;left=$leftPath;right=$rightPath;status='PASS';checks=$checks.ToArray();runs=$folders.ToArray();sha256=(Get-FileHash $exe).Hash}
    }finally{
        foreach($folder in $folders){try{if((Read-Run $folder).phase -lt 4){Signal-Run $folder 'stop';Start-Sleep -Milliseconds 500;if((Read-Run $folder).phase -lt 4){Signal-Run $folder 'kill'}}}catch{Write-Warning $_}}
        if(-not $process.HasExited){[InputProbe]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(-not $process.WaitForExit(2000)){$process.Kill();$process.WaitForExit()}}
        $process.Dispose()
    }
}
$name=if($real){'real-engine-check.json'}else{'process-gui-check.json'}
$results|ConvertTo-Json -Depth 6|Set-Content (Join-Path $evidence $name) -Encoding UTF8
Write-Output "Evidence: $evidence"
