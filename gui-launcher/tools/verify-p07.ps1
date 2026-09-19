[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$Engine,
 [Parameter(Mandatory=$true)][string]$FFmpeg,
 [string]$CaseIds,
 [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
$guiRoot=Split-Path -Parent $PSScriptRoot
$ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
function Check([string]$Script,[string[]]$Arguments=@()) {
 & $ps -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot $Script) @Arguments
 if($LASTEXITCODE -ne 0){throw "P07 stopped at $Script ($LASTEXITCODE)."}
}
Check 'verify-coverage.ps1'
Check 'verify-p07-input-safety.ps1'
Check 'verify-settings-coverage.ps1'
if(-not $SkipBuild){Check 'build.ps1' @('-Configuration','All','-Test','-Install')}
Check 'prepare-p07-media.ps1' @('-FFmpeg',$FFmpeg)
$destination=Join-Path $guiRoot ('work\p07\run-'+[Guid]::NewGuid().ToString('N'))
$arguments=@($Engine,(Join-Path $guiRoot 'work\p07\assets-lossless'),$destination)
if($CaseIds){$arguments+=$CaseIds}
& (Join-Path $guiRoot 'build\msvc-x64\bin\Release\launcher-engine-acceptance.exe') @arguments
if($LASTEXITCODE -ne 0){throw "P07 real-engine checks failed. Inspect $destination\results.tsv and per-case logs."}
Write-Output "Engine subcases completed: $destination"
Write-Output 'This is not a full P07 sign-off. Review conditional hardware and interactive cases in P07-ACCEPTANCE.md.'
