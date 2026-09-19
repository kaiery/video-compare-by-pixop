[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$guiRoot=Split-Path -Parent $PSScriptRoot
$root=Join-Path $guiRoot 'work/p07'
Add-Type -AssemblyName System.Drawing
$results=@()
foreach($id in @('RUN-008','RUN-011')){
 $case=Join-Path $root "final-interaction/$id-keys"
 $gui=[IO.File]::ReadAllText((Join-Path $case 'gui/observation.txt'))
 $cli=[IO.File]::ReadAllText((Join-Path $case 'cli/observation.txt'))
 $commandPattern='(?m)^Command line:.*$'
 $sameCommand=[regex]::Match($gui,$commandPattern).Value -ceq [regex]::Match($cli,$commandPattern).Value
 if(-not [regex]::IsMatch($gui,$commandPattern) -or -not [regex]::IsMatch($cli,$commandPattern)){throw 'Missing actual engine command line.'}
 $frames=@()
 foreach($file in (Get-ChildItem -LiteralPath (Join-Path $case 'gui') -Filter *.png | Where-Object Name -notmatch 'osd')){
  $other=Join-Path $case ('cli/'+$file.Name)
  $a=[Drawing.Bitmap]::new($file.FullName)
  try{$aSize="$($a.Width)x$($a.Height)"}finally{$a.Dispose()}
  $b=[Drawing.Bitmap]::new($other)
  try{$bSize="$($b.Width)x$($b.Height)"}finally{$b.Dispose()}
  $frames += [pscustomobject]@{file=$file.Name;guiSize=$aSize;cliSize=$bSize;equal=((Get-FileHash -LiteralPath $file.FullName).Hash -eq (Get-FileHash -LiteralPath $other).Hash)}
 }
 $results += [pscustomobject]@{
  id=$id;identicalActualCommandLine=$sameCommand
  guiDisplayStates=@([regex]::Matches($gui,'(?m)^Display state:.*$') | ForEach-Object Value)
  cliDisplayStates=@([regex]::Matches($cli,'(?m)^Display state:.*$') | ForEach-Object Value)
  guiMetrics=@([regex]::Matches($gui,'(?m)^Metrics:.*$') | ForEach-Object Value)
  cliMetrics=@([regex]::Matches($cli,'(?m)^Metrics:.*$') | ForEach-Object Value)
  frames=$frames
  conclusion='Different observed interaction states; this historical FAIL is not sufficient to attribute a defect to GUI launch. Manual equivalent-state replay remains required.'
 }
}
$results | ConvertTo-Json -Depth 8 | Out-File (Join-Path $root 'failure-audit.json') -Encoding UTF8
$results | Select-Object id,identicalActualCommandLine,@{n='GUI display states';e={$_.guiDisplayStates.Count}},@{n='CLI display states';e={$_.cliDisplayStates.Count}}
