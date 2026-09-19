[CmdletBinding()]
param(
 [Parameter(Mandatory=$true)][string]$Archive,
 [Parameter(Mandatory=$true)][string]$Engine,
 [Parameter(Mandatory=$true)][string]$Left,
 [Parameter(Mandatory=$true)][string]$Right
)
$ErrorActionPreference='Stop'
$guiRoot=Split-Path -Parent $PSScriptRoot
$archivePath=(Resolve-Path -LiteralPath $Archive).Path
$expected=([IO.File]::ReadAllText("$archivePath.sha256") -split '\s+')[0]
if((Get-FileHash -LiteralPath $archivePath).Hash -ne $expected){throw 'Archive SHA256 mismatch.'}
$name='p08-unpacked-'+[Guid]::NewGuid().ToString('N')
$destination=Join-Path $guiRoot "work/$name"
New-Item -ItemType Directory -Path $destination | Out-Null
Expand-Archive -LiteralPath $archivePath -DestinationPath $destination
$roots=@(Get-ChildItem -LiteralPath $destination -Directory)
if($roots.Count -ne 1){throw 'Package must contain exactly one top-level directory.'}
$root=$roots[0].FullName
$manifest=Get-Content -LiteralPath (Join-Path $root 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
$expectedFiles=@($manifest.files | ForEach-Object path)+@('manifest.json')
$actualFiles=@(Get-ChildItem -LiteralPath $root -File -Recurse | ForEach-Object {$_.FullName.Substring($root.Length+1).Replace('\','/')})
if(Compare-Object ($expectedFiles|Sort-Object) ($actualFiles|Sort-Object)){throw 'Package content does not match manifest.'}
foreach($file in $manifest.files){
 if($file.path -match '(^/|\.\.|:|\\)'){throw 'Invalid manifest relative path.'}
 $path=Join-Path $root $file.path
 if((Get-Item -LiteralPath $path).Length -ne $file.bytes -or (Get-FileHash -LiteralPath $path).Hash -ne $file.sha256){throw "File verification failed: $($file.path)"}
}
if(@($actualFiles | Where-Object {$_ -match '(?i)(^|/)(build|work|tests|src)/|\.(pdb|mp4|mkv|dll|vcgui|opt)$'}).Count){throw 'Unexpected development/media/session files in runtime package.'}
$ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
& $ps -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'verify-process-gui.ps1') -Executable (Join-Path $root 'video-compare-gui.exe') -EvidenceName $name -Engine $Engine -Left $Left -Right $Right
if($LASTEXITCODE -ne 0){throw "Unpacked GUI runtime check failed: $destination"}
$observed=Get-Content -LiteralPath (Join-Path $destination 'real-engine-check.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if(@($observed | Where-Object status -ne PASS).Count){throw 'Runtime report is not PASS.'}
[pscustomobject]@{status='PASS';archive=$archivePath;sha256=$expected;unpacked=$root;fileCount=$actualFiles.Count;runtimeEvidence=(Join-Path $destination 'real-engine-check.json');checkedUtc=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Out-File (Join-Path $guiRoot 'work/p08/package-verification.json') -Encoding UTF8
Write-Output "PASS: archive/manifest, clean extraction, engine selection, media launch, logs and normal stop. Evidence: $destination"
