[CmdletBinding()]
param([switch]$SkipBuild,[switch]$ReplaceExisting)
$ErrorActionPreference='Stop'
$guiRoot=Split-Path -Parent $PSScriptRoot
$ps=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
if(-not $SkipBuild){
 & $ps -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build.ps1') -Configuration All -Test
 if($LASTEXITCODE -ne 0){throw 'Package stopped: build or tests failed.'}
}
$version=[regex]::Match([IO.File]::ReadAllText((Join-Path $guiRoot 'CMakeLists.txt')),'project\(VideoCompareGUI VERSION ([0-9.]+)').Groups[1].Value
if(-not $version){throw 'Cannot determine project version.'}
$name="video-compare-gui-$version"
$release=Join-Path $guiRoot 'releases'
New-Item -ItemType Directory -Force -Path $release | Out-Null
$zip=Join-Path $release "$name-win-x64.zip"
$sourceZip=Join-Path $release "$name-source.zip"
if(-not $ReplaceExisting -and ((Test-Path -LiteralPath $zip) -or (Test-Path -LiteralPath $sourceZip))){throw 'Release files already exist; use -ReplaceExisting only when deliberately rebuilding these generated archives.'}
$stage=Join-Path $guiRoot ('work/p08/package-'+[Guid]::NewGuid().ToString('N'))
$runtime=Join-Path $stage "$name-win-x64"
$source=Join-Path $stage 'source/gui-launcher'
New-Item -ItemType Directory -Force -Path $runtime,$source,(Join-Path $runtime 'engine') | Out-Null
. (Join-Path $PSScriptRoot 'vs-discovery.ps1')
$vs=@(Get-LauncherVisualStudioInstallation)[0].installationPath
$dumpbin=Get-ChildItem -LiteralPath (Join-Path $vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | ForEach-Object {Join-Path $_.FullName 'bin/Hostx64/x64/dumpbin.exe'} | Where-Object {Test-Path -LiteralPath $_} | Select-Object -First 1
if(-not $dumpbin){throw 'dumpbin is required to inspect release imports.'}
$imports=@{}
foreach($exe in @('video-compare-gui.exe','video-compare-runner.exe')){
 $path=Join-Path $guiRoot "build/msvc-x64/bin/Release/$exe"
 if(-not (Test-Path -LiteralPath $path)){throw "Missing compiled Release executable: $exe"}
 $headers=(& $dumpbin /headers $path | Out-String)
 if($LASTEXITCODE -ne 0 -or $headers -notmatch '8664 machine'){throw "Not a verified x64 PE: $exe"}
 $deps=(& $dumpbin /dependents $path | Out-String)
 if($LASTEXITCODE -ne 0){throw "Cannot inspect imports: $exe"}
 $imports[$exe]=@([regex]::Matches($deps,'(?im)^\s+([a-z0-9_.-]+\.dll)\s*$') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
 if(-not $imports[$exe].Count -or $imports[$exe] -match '^(vcruntime|msvcp|avcodec|avformat|SDL)'){throw "Unexpected standalone runtime dependency: $exe"}
 [IO.File]::WriteAllText((Join-Path $stage "$exe-imports.txt"),$deps)
 Copy-Item -LiteralPath $path -Destination (Join-Path $runtime $exe)
}
$fileVersion=(Get-Item -LiteralPath (Join-Path $runtime 'video-compare-gui.exe')).VersionInfo.FileVersion
if($fileVersion -ne "$version.0"){throw 'GUI file version does not match CMake project version.'}
foreach($file in @('README.txt','KNOWN-ISSUES.md','DEPENDENCIES.md','SOURCE.md')){Copy-Item -LiteralPath (Join-Path $guiRoot "release/$file") -Destination (Join-Path $runtime $file)}
Copy-Item -LiteralPath (Join-Path $guiRoot 'release/engine-README.txt') -Destination (Join-Path $runtime 'engine/README.txt')
[IO.File]::WriteAllText((Join-Path $runtime 'portable.mode'),"Video Compare GUI portable data root: this directory.`r`n",[Text.UTF8Encoding]::new($false))
$license=Join-Path $guiRoot 'LICENSE.txt'
if(-not (Test-Path -LiteralPath $license)){$license=Join-Path (Split-Path -Parent $guiRoot) 'LICENSE.md'}
Copy-Item -LiteralPath $license -Destination (Join-Path $runtime 'LICENSE.txt')
$files=@(Get-ChildItem -LiteralPath $runtime -File -Recurse | ForEach-Object {[pscustomobject]@{path=$_.FullName.Substring($runtime.Length+1).Replace('\','/');bytes=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}} | Sort-Object path)
$manifest=[ordered]@{product='Video Compare GUI';version=$version;architecture='x64';configuration='Release';engineBundled=$false;portable=$true;acceptance='P07 limited; see KNOWN-ISSUES.md';createdUtc=[DateTime]::UtcNow.ToString('o');imports=$imports;files=$files}
$manifest | ConvertTo-Json -Depth 6 | Out-File (Join-Path $runtime 'manifest.json') -Encoding UTF8
# Explicit source roots exclude build output, local sessions, test evidence and media.
foreach($dir in @('src','resources','tests','tools','release')){Copy-Item -LiteralPath (Join-Path $guiRoot $dir) -Destination $source -Recurse}
foreach($file in Get-ChildItem -LiteralPath $guiRoot -File | Where-Object { $_.Extension -eq '.md' -or $_.Name -in @('CMakeLists.txt','CMakePresets.json','.gitignore') }){Copy-Item -LiteralPath $file.FullName -Destination $source}
Copy-Item -LiteralPath $license -Destination (Join-Path $source 'LICENSE.txt')
Compress-Archive -LiteralPath $runtime -DestinationPath $zip -CompressionLevel Optimal -Force:$ReplaceExisting
Compress-Archive -LiteralPath (Join-Path $stage 'source/gui-launcher') -DestinationPath $sourceZip -CompressionLevel Optimal -Force:$ReplaceExisting
foreach($archive in @($zip,$sourceZip)){
 $hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
 [IO.File]::WriteAllText("$archive.sha256", "$hash  $([IO.Path]::GetFileName($archive))`n",[Text.UTF8Encoding]::new($false))
}
[pscustomobject]@{runtime=$zip;source=$sourceZip;stage=$stage;version=$version} | ConvertTo-Json | Out-File (Join-Path $guiRoot 'work/p08/package.json') -Encoding UTF8
Write-Output "Runtime: $zip"
Write-Output "Source: $sourceZip"
