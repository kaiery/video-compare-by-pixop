param([string]$DependencyRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) '.local-build'))
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root '.local-build/release/bin'
$llvm = Join-Path $DependencyRoot 'llvm-mingw-20260908-ucrt-x86_64'
$ff = Join-Path $DependencyRoot 'ffmpeg-9.0.1-full_build-shared'
$exe = Join-Path $bin 'video-compare.exe'
$versionLine = & $exe --version
if ($LASTEXITCODE -ne 0) { throw 'Engine version check failed' }
$version = ($versionLine | Select-Object -First 1) -replace '^video-compare ', ''
if ($version -notmatch '^[a-zA-Z0-9.-]+$') { throw 'Invalid version string' }
$name = "video-compare-$version-windows-x64"
$dist = Join-Path $root 'dist'
$package = Join-Path $dist $name
if (Test-Path $package) { throw "Release already exists: $package (use a new version or archive the old output)" }
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item -LiteralPath $exe -Destination $package
& "$llvm/bin/llvm-strip.exe" "$package/video-compare.exe"
if ($LASTEXITCODE -ne 0) { throw 'Strip failed' }
# Follow PE imports recursively. Only Windows system DLLs may be external.
$pending = [Collections.Generic.Queue[string]]::new()
$pending.Enqueue('video-compare.exe')
$seen = @{}
while ($pending.Count) {
    $file = $pending.Dequeue()
    $imports = & "$llvm/bin/llvm-objdump.exe" -p (Join-Path $package $file)
    if ($LASTEXITCODE -ne 0) { throw "Cannot inspect imports: $file" }
    foreach ($line in $imports) {
        if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
        $dll = $Matches[1]
        if ($seen.ContainsKey($dll)) { continue }
        $seen[$dll] = $true
        $source = Join-Path $bin $dll
        if (Test-Path $source) {
            Copy-Item -LiteralPath $source -Destination $package
            $pending.Enqueue($dll)
        } elseif ($dll -notmatch '^(api-ms-|ext-ms-)' -and !(Test-Path (Join-Path "$env:WINDIR/System32" $dll))) {
            throw "Unresolved DLL: $dll"
        }
    }
}
Copy-Item -LiteralPath "$root/README.md","$root/LICENSE.md" -Destination $package
Copy-Item -LiteralPath "$root/licenses" -Destination $package -Recurse
New-Item -ItemType Directory "$package/docs" | Out-Null
Copy-Item -LiteralPath "$root/docs/frame-stepping.md","$root/docs/engine-release.md" -Destination "$package/docs"
Copy-Item -LiteralPath "$root/docs/engine-release.md" -Destination "$package/START-HERE.md"
Copy-Item -LiteralPath "$llvm/LICENSE.TXT" -Destination "$package/licenses/LICENSE-LLVM.txt"
Copy-Item -LiteralPath "$ff/README.txt" -Destination "$package/licenses/FFMPEG-BUILD.txt"
$ffReadme = Get-Content "$ff/README.txt" -Raw
$revision = [regex]::Match($ffReadme, 'https://github.com/FFmpeg/FFmpeg/commit/([a-f0-9]+)').Groups[1].Value
if (!$revision) { throw 'Missing FFmpeg source revision' }
@"
Source and build information for the bundled FFmpeg libraries
Gyan release version: 9.0.1-full_build-www.gyan.dev
FFmpeg upstream commit: https://github.com/FFmpeg/FFmpeg/commit/$revision
FFmpeg upstream archive: https://github.com/FFmpeg/FFmpeg/archive/$revision.tar.gz
Bundled shared-build archive: ffmpeg-9.0.1-full_build-shared.7z
Archive download URL: https://github.com/GyanD/codexffmpeg/releases/download/9.0.1/ffmpeg-9.0.1-full_build-shared.7z
Build scripts: https://github.com/GyanD/media-autobuild_suite
Full build configuration: FFMPEG-BUILD.txt
This manifest is not the complete corresponding source of all bundled libraries.
"@ | Set-Content "$package/licenses/FFMPEG-SOURCE.txt" -Encoding UTF8
$ffArchive = Join-Path $DependencyRoot 'ffmpeg-9.0.1-full_build-shared.7z'
if (!(Test-Path $ffArchive)) { $ffArchive = Join-Path $DependencyRoot 'ffmpeg.7z' }
if (Test-Path $ffArchive) {
    "Archive SHA-256: $((Get-FileHash $ffArchive -Algorithm SHA256).Hash)" | Add-Content "$package/licenses/FFMPEG-SOURCE.txt"
}
@"
Version: $version
Built UTC: $([DateTime]::UtcNow.ToString('o'))
Compiler: LLVM-MinGW 20260908 UCRT x86_64
C++ flags: -std=c++14 -O2 (stripped executable)
Dependencies: FFmpeg 9.0.1, SDL 2.32.10, SDL_ttf 2.24.0
Runtime: libc++.dll and libunwind.dll (LLVM license in licenses/)
Source: $name-source.zip (working-tree snapshot, includes uncommitted changes)
"@ | Set-Content "$package/BUILD-INFO.txt" -Encoding UTF8
# Deliberately restrict the child process search path to the package and Windows.
$savedPath = $env:PATH
try {
    $env:PATH = "$env:WINDIR/System32;$env:WINDIR"
    & "$package/video-compare.exe" --version
    if ($LASTEXITCODE -ne 0) { throw 'Standalone version check failed' }
    & "$package/video-compare.exe" --help | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Standalone help check failed' }
} finally { $env:PATH = $savedPath }
Push-Location $root
try {
    $files = @(git -c core.quotepath=false ls-files --cached --others --exclude-standard)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot list source snapshot' }
    $sourceDir = Join-Path $dist "$name-source"
    if (Test-Path $sourceDir) { throw 'Source snapshot already exists' }
    New-Item -ItemType Directory $sourceDir | Out-Null
    foreach ($file in $files) {
        if (!(Test-Path -LiteralPath $file -PathType Leaf)) { continue }
        $destination = Join-Path $sourceDir $file
        New-Item -ItemType Directory -Force (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath $file -Destination $destination
    }
    Compress-Archive -LiteralPath $sourceDir -DestinationPath "$dist/$name-source.zip"
} finally { Pop-Location }
Get-ChildItem $package -Recurse -File | ForEach-Object {
    $relative = $_.FullName.Substring($package.Length + 1).Replace('\','/')
    "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash)  $relative"
} | Set-Content "$dist/$name-files.sha256" -Encoding ASCII
Copy-Item "$dist/$name-files.sha256" "$package/SHA256SUMS.txt"
Compress-Archive -LiteralPath $package -DestinationPath "$dist/$name.zip"
$verifyDir = Join-Path $root ('.local-build/release/unpacked-' + [Guid]::NewGuid().ToString('N'))
Expand-Archive -LiteralPath "$dist/$name.zip" -DestinationPath $verifyDir
$unpacked = Join-Path $verifyDir $name
foreach ($line in Get-Content "$unpacked/SHA256SUMS.txt") {
    $parts = $line -split '  ', 2
    if ((Get-FileHash -LiteralPath (Join-Path $unpacked $parts[1]) -Algorithm SHA256).Hash -ne $parts[0]) {
        throw "Extracted file checksum mismatch: $($parts[1])"
    }
}
try {
    $env:PATH = "$env:WINDIR/System32;$env:WINDIR"
    & "$unpacked/video-compare.exe" --version
    if ($LASTEXITCODE -ne 0) { throw 'Extracted release failed to start' }
} finally { $env:PATH = $savedPath }
Get-FileHash "$dist/$name.zip","$dist/$name-source.zip" -Algorithm SHA256 | ForEach-Object {
    "$($_.Hash)  $([IO.Path]::GetFileName($_.Path))"
} | Set-Content "$dist/$name.sha256" -Encoding ASCII
"Release ready: $dist/$name.zip"
