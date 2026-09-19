# One fail-fast entry point: never run the window check after a failed build.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$powershellExe = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$steps = @(
    @{ Name = 'Visual Studio encoding regression'; Script = 'verify-vs-discovery.ps1'; Arguments = @() },
    @{ Name = 'Source and schema coverage'; Script = 'verify-coverage.ps1'; Arguments = @() },
    @{ Name = 'Debug/Release build, tests and install'; Script = 'build.ps1'; Arguments = @('-Configuration', 'All', '-Test', '-Install') },
    @{ Name = 'Built executable window checks'; Script = 'verify-window.ps1'; Arguments = @('-EvidenceName', 'p02-window') }
)
foreach ($step in $steps) {
    Write-Output "Checking: $($step.Name)"
    $scriptPath = Join-Path $PSScriptRoot $step.Script
    $stepArguments = $step.Arguments
    & $powershellExe -NoProfile -ExecutionPolicy Bypass -File $scriptPath @stepArguments
    if ($LASTEXITCODE -ne 0) {
        throw "P02 verification stopped at '$($step.Name)' (exit $LASTEXITCODE). Later steps were not run."
    }
}
Write-Output 'PASS: P02 coverage, builds, core tests and window checks completed in sequence.'
