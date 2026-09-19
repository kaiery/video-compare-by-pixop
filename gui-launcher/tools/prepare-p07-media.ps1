[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$FFmpeg,[string]$AssetName='assets-lossless')
$ErrorActionPreference='Stop'
if($AssetName -notmatch '^[a-zA-Z0-9_-]+$'){throw 'AssetName must be a simple directory name.'}
$assets=Join-Path (Split-Path -Parent $PSScriptRoot) "work\p07\$AssetName"
New-Item -ItemType Directory -Path $assets -Force | Out-Null
function Media([string[]]$Arguments) {
    & $FFmpeg -hide_banner -loglevel error -y @Arguments
    if($LASTEXITCODE -ne 0){throw "FFmpeg failed: $LASTEXITCODE"}
}
Media @('-f','lavfi','-i','testsrc2=size=160x96:rate=24','-frames:v','1',(Join-Path $assets 'left.png'))
Media @('-f','lavfi','-i','testsrc2=size=192x128:rate=30','-vf','hflip,eq=brightness=0.03','-frames:v','1',(Join-Path $assets 'right.png'))
foreach($name in @('left','right')) {
    $rate=if($name -eq 'left'){'24'}else{'30'}
    Media @('-loop','1','-framerate',$rate,'-i',(Join-Path $assets "$name.png"),'-t','120','-c:v','libx264','-preset','ultrafast','-crf','0','-pix_fmt','yuv420p','-colorspace','bt709','-color_primaries','bt709','-color_trc','bt709',(Join-Path $assets "$name.mp4"))
    # H.264 lossless profile is not supported by many hardware decoders.
    Media @('-loop','1','-framerate',$rate,'-i',(Join-Path $assets "$name.png"),'-t','4','-c:v','libx264','-preset','ultrafast','-pix_fmt','yuv420p',(Join-Path $assets "cuda-$name.mp4"))
}
Media @('-i',(Join-Path $assets 'right.png'),'-frames:v','1','-c:v','libaom-av1','-cpu-used','8','-still-picture','1',(Join-Path $assets 'right-av1.mkv'))
Media @('-loop','1','-i',(Join-Path $assets 'left.png'),'-t','2','-vf','format=yuv420p10le','-c:v','libx265','-preset','ultrafast','-x265-params','log-level=error:pools=1','-color_primaries','bt2020','-color_trc','smpte2084','-colorspace','bt2020nc',(Join-Path $assets 'pq-tagged.mkv'))
Media @('-i',(Join-Path $assets 'left.png'),'-frames:v','1','-pix_fmt','rgb24','-f','rawvideo',(Join-Path $assets 'left.rgb'))
# Synthetic PQ metadata exercises conversion paths, not calibrated HDR luminance.
& $FFmpeg -version | Out-File (Join-Path $assets 'ffmpeg-version.txt') -Encoding utf8
$hashes=@(Get-ChildItem -LiteralPath $assets -File | Where-Object Extension -in @('.mp4','.png','.mkv','.rgb') | Get-FileHash -Algorithm SHA256)
$hashes | Export-Csv (Join-Path $assets 'sha256.csv') -NoTypeInformation -Encoding UTF8
Write-Output "P07 fixtures: $assets"
