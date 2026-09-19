# Regression check for Windows PowerShell 5.1 / Chinese and UTF-8 consoles.
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'vs-discovery.ps1')
$originalConsoleEncoding = [Console]::OutputEncoding
$originalPipelineEncoding = $OutputEncoding
try {
    $baseline = @(Get-LauncherVisualStudioInstallation)
    if ($baseline.Count -ne 1) { throw 'This machine-level test requires one detected latest C++ installation.' }
    foreach ($codePage in @(936, 65001)) {
        [Console]::OutputEncoding = [Text.Encoding]::GetEncoding($codePage)
        $OutputEncoding = [Console]::OutputEncoding
        $actual = @(Get-LauncherVisualStudioInstallation)
        if ($actual.Count -ne $baseline.Count) { throw "Discovery count changed at CP$codePage." }
        foreach ($field in @('installationPath', 'installationVersion', 'displayName', 'description')) {
            if ($actual[0].$field -cne $baseline[0].$field) { throw "UTF-8 field '$field' corrupted at CP$codePage." }
        }
        if ([Console]::OutputEncoding.CodePage -ne $codePage) { throw 'Discovery modified the console encoding.' }
        if (-not (Test-Path -LiteralPath $actual[0].installationPath)) { throw 'Discovered path is invalid.' }
        Write-Output "PASS: VS discovery at CP$codePage; Unicode fields preserved; console encoding unchanged."
    }
} finally {
    [Console]::OutputEncoding = $originalConsoleEncoding
    $OutputEncoding = $originalPipelineEncoding
}
