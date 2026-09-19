[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$guiRoot=Split-Path -Parent $PSScriptRoot
$schema=[IO.File]::ReadAllText((Join-Path $guiRoot 'src/core/catalog.cpp'))
$ui=[IO.File]::ReadAllText((Join-Path $guiRoot 'src/ui/settings_catalog.inc'))
$mapping=[IO.File]::ReadAllText((Join-Path $guiRoot 'P04-CONTROLS.md'))
$ids=@([regex]::Matches($schema,'\{"(CLI-\d{3})"')|ForEach-Object{$_.Groups[1].Value})
$controls=@([regex]::Matches($ui,'(?m)^\{"(CLI-\d{3})", Page::(\w+), ([^,]+),')|ForEach-Object{
    [pscustomobject]@{id=$_.Groups[1].Value;page=$_.Groups[2].Value;control=$_.Groups[3].Value}
})
$rows=@([regex]::Matches($mapping,'(?m)^\| (CLI-\d{3}) \|')|ForEach-Object{$_.Groups[1].Value})
function Same-Ids($Expected,$Actual,[string]$Name) {
    if($Expected.Count -ne $Actual.Count -or @($Actual|Sort-Object -Unique).Count -ne $Actual.Count -or @(Compare-Object $Expected $Actual).Count){throw "Coverage mismatch: $Name"}
}
Same-Ids $ids @($controls.id) 'schema -> UI catalog'
Same-Ids $ids $rows 'schema -> control/acceptance table'
$main=@($controls|Where-Object{$_.page -eq 'Main'})
if($main.Count -ne 9 -or @($main|Where-Object{$_.control -notlike 'IDC_*'}).Count){throw 'Expected nine existing main-window bindings.'}
if(@($controls|Where-Object{$_.page -ne 'Main' -and $_.control -ne '0'}).Count){throw 'Unexpected advanced binding.'}
$rv=@([regex]::Matches($schema,'\{"(RV-\d{3})"')|ForEach-Object{$_.Groups[1].Value})
$rvRows=@([regex]::Matches($mapping,'(?m)^\| (RV-\d{3}) \|')|ForEach-Object{$_.Groups[1].Value})
Same-Ids $rv $rvRows 'override schema -> control/acceptance table'
if($ids.Count -ne 61 -or $rv.Count -ne 11){throw 'Source baseline changed; update scope and acceptance explicitly.'}
Write-Output 'PASS: 61 CLI UI mappings (9 main, 52 categorized) and 11 per-video controls documented.'
Write-Output 'Static coverage only. Run ui.settings-* and verify-settings.ps1 for behavior.'
