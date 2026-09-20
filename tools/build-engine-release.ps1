param([string]$DependencyRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) '.local-build'))
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $root
$compiler=Join-Path $DependencyRoot 'llvm-mingw-20260908-ucrt-x86_64/bin/clang++.exe'
$ff=Join-Path $DependencyRoot 'ffmpeg-9.0.1-full_build-shared'
$sdl=Join-Path $DependencyRoot 'SDL2-2.32.10/x86_64-w64-mingw32'
$ttf=Join-Path $DependencyRoot 'SDL2_ttf-2.24.0/x86_64-w64-mingw32'
$bin=Join-Path $root '.local-build/release/bin'
$obj=Join-Path $root '.local-build/release/obj'
New-Item -ItemType Directory -Force -Path $bin,$obj | Out-Null
$flags=@('-std=c++14','-O2','-D__STDC_CONSTANT_MACROS','-Wno-deprecated-declarations','-Isrc','-Ithird_party',"-I$ff/include","-I$sdl/include","-I$sdl/include/SDL2","-I$ttf/include","-I$ttf/include/SDL2")
$libs=@("-L$ff/lib","-L$sdl/lib","-L$ttf/lib",'-lavformat','-lavcodec','-lavfilter','-lavutil','-lswscale','-lswresample','-lSDL2_ttf','-lSDL2')
$headerTime=(Get-ChildItem src,third_party -Recurse -Filter '*.h' | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).LastWriteTimeUtc
$objects=@()
foreach($source in Get-ChildItem src/*.cpp){
 $object=Join-Path $obj ($source.BaseName+'.o')
 if(!(Test-Path $object) -or (Get-Item $object).LastWriteTimeUtc -lt $source.LastWriteTimeUtc -or (Get-Item $object).LastWriteTimeUtc -lt $headerTime){
  & $compiler @flags -c $source.FullName -o $object
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"}
 }
 $objects+=$object
}
& $compiler @objects @libs -o (Join-Path $bin 'video-compare.exe')
if($LASTEXITCODE -ne 0){throw 'Engine link failed'}
$appObjects=@($objects | Where-Object { [IO.Path]::GetFileName($_) -ne 'main.o' })
& $compiler @flags tests/integration_video_compare.cpp @appObjects @libs -o (Join-Path $bin 'integration_video_compare.exe')
if($LASTEXITCODE -ne 0){throw 'Integration build failed'}
& $compiler @flags tests/integration_frame_step.cpp @appObjects @libs -o (Join-Path $bin 'integration_frame_step.exe')
if($LASTEXITCODE -ne 0){throw 'Frame-step integration build failed'}
foreach($name in @('test_playback_navigation','test_playback_seek','test_playback_timing','test_frame_step')){
 if(Test-Path "tests/$name.cpp"){
  & $compiler @flags "tests/$name.cpp" @libs -o (Join-Path $bin "$name.exe")
  if($LASTEXITCODE -ne 0){throw "Test build failed: $name"}
 }
}
Get-ChildItem "$ff/bin/*.dll","$sdl/bin/*.dll","$ttf/bin/*.dll",(Join-Path $DependencyRoot 'llvm-mingw-20260908-ucrt-x86_64/bin/*.dll') | Copy-Item -Destination $bin -Force
'Release build passed: engine, integration host, navigation tests'

