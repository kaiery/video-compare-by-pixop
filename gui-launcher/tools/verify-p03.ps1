# Fail fast: never accept an old executable after a failed build.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$powershellExe = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$steps = @(
    @{ Script = 'verify-vs-discovery.ps1'; Arguments = @() },
    @{ Script = 'verify-coverage.ps1'; Arguments = @() },
    @{ Script = 'build.ps1'; Arguments = @('-Configuration','All','-Test','-Install') },
    @{ Script = 'verify-inputs.ps1'; Arguments = @() },
    @{ Script = 'verify-window.ps1'; Arguments = @('-EvidenceName','p03-window') }
)
foreach($step in $steps) {
    Write-Output "Checking: $($step.Script)"
    $path = Join-Path $PSScriptRoot $step.Script
    $arguments = $step.Arguments
    & $powershellExe -NoProfile -ExecutionPolicy Bypass -File $path @arguments
    if($LASTEXITCODE -ne 0){throw "P03 stopped at $($step.Script), exit $LASTEXITCODE. Later steps were not run."}
}
Write-Output 'PASS: P03 builds, core/native tests, real GUI controls and window checks.'
