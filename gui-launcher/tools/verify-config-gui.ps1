[CmdletBinding()]
param([string]$Engine,[string]$Configuration='All',[string]$Left,[string]$Right)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'verify-settings.ps1') -LoadHelpersOnly
$evidence=Join-Path $guiRoot 'work/p06'
New-Item -ItemType Directory -Force -Path $evidence|Out-Null
function File-Command([int]$Id,[string]$Path){
 [InputProbe]::PostMessage($window,0x111,[IntPtr]$Id,[IntPtr]::Zero)|Out-Null
 $deadline=[DateTime]::UtcNow.AddSeconds(8)
 do{$picker=[InputProbe]::FindCaption($process.Id,$(if($Id -in 40003,40006){'保存配置'}else{'打开配置'}));if($picker -ne [IntPtr]::Zero){$field=[InputProbe]::FileNameEdit($picker);if($field -ne [IntPtr]::Zero){break}};Start-Sleep -Milliseconds 50}while([DateTime]::UtcNow -lt $deadline)
 if($field -eq [IntPtr]::Zero){throw 'Configuration picker did not open.'}
 Start-Sleep -Milliseconds 800
 [InputProbe]::Put($field,$Path)
 [InputProbe]::PostMessage($picker,0x111,[IntPtr]1,[IntPtr]::Zero)|Out-Null
 $deadline=[DateTime]::UtcNow.AddSeconds(8)
 do{Start-Sleep -Milliseconds 100;$alive=[InputProbe]::FindCaption($process.Id,$(if($Id -in 40003,40006){'保存配置'}else{'打开配置'}))}while($alive -ne [IntPtr]::Zero -and [DateTime]::UtcNow -lt $deadline)
 if($alive -ne [IntPtr]::Zero){throw 'Configuration picker did not close.'}
 Start-Sleep -Milliseconds 250
}
function Open-Settings {[InputProbe]::PostMessage($window,0x111,[IntPtr]1045,[IntPtr]::Zero)|Out-Null;return (Wait-Settings)}
function Wait-Query([IntPtr]$Dialog){
 Start-Sleep -Milliseconds 300
 $deadline=[DateTime]::UtcNow.AddSeconds(15)
 do{$text=[InputProbe]::Text((Setting-Control $Dialog 1202));if($text.Contains('退出码=') -or $text.Contains('创建比较进程失败') -or $text.Contains('运行助手发生错误')){return $text};if(-not [InputProbe]::Responsive($Dialog)){throw 'Query UI unresponsive'};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow -lt $deadline)
 throw "Query did not finish: $text"
}
$real=-not [string]::IsNullOrEmpty($Engine)
$configs=if($real){@('Installed')}elseif($Configuration -eq 'All'){@('Debug','Release','Installed')}else{@($Configuration)}
$results=@()
foreach($config in $configs){
 $run=Join-Path $evidence ($config+'-'+[Guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Path $run|Out-Null
 $exe=if($config -eq 'Installed'){Join-Path $guiRoot 'dist/video-compare-gui.exe'}else{Join-Path $guiRoot "build/msvc-x64/bin/$config/video-compare-gui.exe"}
 $fixtureConfig=if($config -eq 'Installed'){'Release'}else{$config}
 $enginePath=if($real){$Engine}else{Join-Path $guiRoot "build/msvc-x64/bin/$fixtureConfig/launcher-engine-fixture.exe"}
 $script:checks=New-Object 'System.Collections.Generic.List[string]'
 $script:process=Start-Process -FilePath $exe -WorkingDirectory $run -WindowStyle Hidden -PassThru
 try{
  if(-not $process.WaitForInputIdle(8000)){throw 'GUI not initialized'}
  $script:window=Wait-Window $process.Id
  [InputProbe]::Put((Control 1001),$enginePath);[InputProbe]::Put((Control 1003),$run)
  $leftPath=if($Left){$Left}else{Join-Path $run '参考.mp4'};$rightPath=if($Right){$Right}else{Join-Path $run '右侧.mp4'}
  [InputProbe]::Put((Control 1010),$leftPath);Edit-Input $rightPath -Mode 1 -Filter 'hflip'
  Select-Combo $window 1030 2;Select-Combo $window 1034 2
  $before=Preview
  $file=Join-Path $run '会话.vcgui';File-Command 40003 $file
  Assert (Test-Path -LiteralPath $file) 'Native Save writes Unicode session file'
  [InputProbe]::Put((Control 1010),'changed.mp4');Select-Combo $window 1030 0;Select-Combo $window 1034 0
  File-Command 40004 $file
  Assert ((Preview) -eq $before) 'Native Open restores main controls, input overrides and exact argv'
  $bad=Join-Path $run 'broken.vcgui';[IO.File]::WriteAllText($bad,'damaged')
  File-Command 40004 $bad
  Assert ([InputProbe]::Text((Control 1044)).Contains('配置操作失败')) 'Corrupt session visibly rejected'
  Assert ((Preview) -eq $before) 'Corrupt session leaves current session unchanged'
  $opt=Join-Path $run '导出.opt';File-Command 40006 $opt
  Assert (Test-Path -LiteralPath $opt) 'Native export creates engine .opt'
  Select-Combo $window 1030 0;Select-Combo $window 1034 0
  File-Command 40005 $opt
  Assert ((Preview) -eq $before) 'Opt import restores active configuration and per-video overrides'
  Command 40007
  Assert ([InputProbe]::Text((Control 1040)).Contains('合并顺序')) 'Configuration provenance and effective preview visible'
  [InputProbe]::Put((Control 1010),'')
  $dialog=Open-Settings;Settings-Page $dialog 8
  foreach($number in @(1,2,3,35,46,50,54,58)){
   $id=2000+($number-1)*10
   if($number -gt 3){[InputProbe]::Put((Setting-Control $dialog ($id+1)),'')}
   Settings-Command $dialog ($id+2);$output=Wait-Query $dialog
   Assert ($output.Contains('退出码=0')) "Query CLI-$number succeeds without videos (empty search when applicable)"
   if($number -eq 1){Assert ($output.Contains('帮助文本能力核对')) 'Help reports baseline capability differences'}
   $output|Set-Content (Join-Path $run "query-$number.txt") -Encoding UTF8
   if($number -gt 3){
    $terms=@{35='scale';46='http';50='matroska';54='h264';58='cuda'}
    foreach($term in @($terms[$number],'p06_no_such_capability_79d3')){
     [InputProbe]::Put((Setting-Control $dialog ($id+1)),$term)
     Settings-Command $dialog ($id+2);$filtered=Wait-Query $dialog
     Assert ($filtered.Contains('退出码=0')) "Query CLI-$number search '$term' completes"
     $filtered|Set-Content (Join-Path $run "query-$number-$term.txt") -Encoding UTF8
    }
   }
  }
  Snapshot $dialog $(if($real){'real-query.png'}else{"$config-query.png"})
  Settings-Command $dialog 2
  if(-not $real){
   $waiting=Join-Path $run 'fixture-wait.exe';Copy-Item -LiteralPath $enginePath -Destination $waiting
   [InputProbe]::Put((Control 1001),$waiting);[InputProbe]::Put((Control 1010),$leftPath)
   Command 1043;Start-Sleep -Milliseconds 700
   $log=[InputProbe]::FindControlWindow($process.Id,1301)
   $status=[InputProbe]::Text([InputProbe]::GetDlgItem($log,1300))
   if($status -notmatch 'PID=(\d+)'){throw 'Comparison PID absent'}
   $comparisonPid=[int]$Matches[1]
   $dialog=Open-Settings;Settings-Page $dialog 8
   Settings-Command $dialog 2012;$output=Wait-Query $dialog
   Assert ($output.Contains('退出码=0') -and $null -ne (Get-Process -Id $comparisonPid -ErrorAction SilentlyContinue)) 'Version query runs independently during active comparison'
   Settings-Command $dialog 2002;Start-Sleep -Milliseconds 500;Settings-Command $dialog 2003
   $output=Wait-Query $dialog;Assert ($output.Contains('退出码=3221225786')) 'Cancel ends only the independent query'
   Settings-Command $dialog 2002;$output=Wait-Query $dialog
   Assert ($output.Contains('检查超时')) 'Query times out automatically while GUI remains responsive'
   Assert ($null -ne (Get-Process -Id $comparisonPid -ErrorAction SilentlyContinue)) 'Query cancellation and timeout preserve running comparison'
   Settings-Command $dialog 2
   [InputProbe]::SendMessage($log,0x111,[IntPtr]1302,[IntPtr]::Zero)|Out-Null
   $deadline=[DateTime]::UtcNow.AddSeconds(5)
   do{Start-Sleep -Milliseconds 100;$alive=Get-Process -Id $comparisonPid -ErrorAction SilentlyContinue}while($null -ne $alive -and [DateTime]::UtcNow -lt $deadline)
   Assert ($null -eq $alive) 'Comparison still stops normally after independent queries'
  }
  if($real -and $Left -and $Right){
   [InputProbe]::Put((Control 1010),$leftPath);Select-Combo $window 1034 0
   $external=Join-Path $run '中文兼容配置.opt';[IO.File]::WriteAllText($external,'--frame-buffer-size 60',(New-Object Text.UTF8Encoding $false))
   $dialog=Open-Settings;Settings-Page $dialog 7
   [InputProbe]::Put((Setting-Control $dialog 3101),$external);Settings-Command $dialog 3102;Settings-Command $dialog 1
   Command 1043;Start-Sleep -Seconds 3
   $log=[InputProbe]::FindControlWindow($process.Id,1301);$status=[InputProbe]::Text([InputProbe]::GetDlgItem($log,1300))
   if($status -notmatch 'PID=(\d+)'){throw 'Real comparison PID absent'}
   $engineProcess=Get-Process -Id ([int]$Matches[1]);$engineProcess.Refresh()
   Assert ($engineProcess.MainWindowHandle -ne [IntPtr]::Zero) 'Real engine loads imported settings and Unicode external opt through staging'
   Snapshot $engineProcess.MainWindowHandle 'real-config-engine.png'
   [InputProbe]::SendMessage($log,0x111,[IntPtr]1302,[IntPtr]::Zero)|Out-Null
   if(-not $engineProcess.WaitForExit(5000)){throw 'Real config comparison failed to stop'}
   Start-Sleep -Milliseconds 400
   Assert ([InputProbe]::Text([InputProbe]::GetDlgItem($log,1300)).Contains('退出码=0')) 'Real configuration comparison exits normally'
   Snapshot $log 'real-config-log.png'
   # Verify that the exported file is accepted by the engine's own two-pass parser,
   # including the trailing bootstrap --options-file arguments.
   $asciiOpt=Join-Path $run 'export-engine.opt';Copy-Item -LiteralPath $opt -Destination $asciiOpt
   $info=New-Object Diagnostics.ProcessStartInfo
   $info.FileName=$Engine;$info.Arguments='--no-auto-options-file --options-file "'+$asciiOpt+'"';$info.WorkingDirectory=$run
   $info.UseShellExecute=$false;$info.CreateNoWindow=$true;$info.RedirectStandardOutput=$true;$info.RedirectStandardError=$true
   $native=New-Object Diagnostics.Process;$native.StartInfo=$info
   try{
    if(-not $native.Start()){throw 'Cannot start exported-opt engine test'}
    $stdout=$native.StandardOutput.ReadToEndAsync();$stderr=$native.StandardError.ReadToEndAsync()
    Start-Sleep -Seconds 3;$native.Refresh()
    Assert (-not $native.HasExited -and $native.MainWindowHandle -ne [IntPtr]::Zero) 'Engine directly accepts exported .opt without stray bootstrap inputs'
    Snapshot $native.MainWindowHandle 'real-export-engine.png'
    $null=$native.CloseMainWindow();if(-not $native.WaitForExit(5000)){throw 'Exported-opt engine did not stop'}
    Assert ($native.ExitCode -eq 0) 'Direct exported-opt comparison exits normally'
    $stdout.Result|Set-Content (Join-Path $run 'export-stdout.txt') -Encoding UTF8
    $stderr.Result|Set-Content (Join-Path $run 'export-stderr.txt') -Encoding UTF8
   }finally{if(-not $native.HasExited){$native.Kill();$native.WaitForExit()};$native.Dispose()}
  }
  $results += [pscustomobject]@{configuration=$config;realEngine=$real;engine=$enginePath;status='PASS';checks=$checks.ToArray();evidence=$run;sha256=(Get-FileHash $exe).Hash}
 }finally{
  if(-not $process.HasExited){[InputProbe]::PostMessage($window,0x10,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null;if(-not $process.WaitForExit(3000)){$process.Kill();$process.WaitForExit()}}
  $process.Dispose()
 }
}
$name=if($real){'real-config-query.json'}else{'config-query.json'}
$results|ConvertTo-Json -Depth 5|Set-Content (Join-Path $evidence $name) -Encoding UTF8
Write-Output "Evidence: $evidence"
