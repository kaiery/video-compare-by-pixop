# P05 fail-fast entry point. No old EXE checks after a failed build/test.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$powershellExe=Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
$steps=@(
    @{Script='verify-vs-discovery.ps1';Arguments=@()},
    @{Script='verify-coverage.ps1';Arguments=@()},
    @{Script='verify-settings-coverage.ps1';Arguments=@()},
    @{Script='build.ps1';Arguments=@('-Configuration','All','-Test','-Install')},
    @{Script='verify-inputs.ps1';Arguments=@()},
    @{Script='verify-settings.ps1';Arguments=@()},
    @{Script='verify-process-gui.ps1';Arguments=@()},
    @{Script='verify-window.ps1';Arguments=@('-EvidenceName','p05-window')}
)
foreach($step in $steps) {
    Write-Output "Checking: $($step.Script)"
    $arguments=$step.Arguments
    & $powershellExe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot $step.Script) @arguments
    if($LASTEXITCODE -ne 0){throw "P05 stopped at $($step.Script), exit $LASTEXITCODE. Later steps were not run."}
}
Write-Output 'PASS: P05 source/UI mappings, builds, parameter tests, GUI settings and input regressions.'
