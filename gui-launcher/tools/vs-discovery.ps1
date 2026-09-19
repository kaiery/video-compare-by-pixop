# vswhere -utf8 writes UTF-8 bytes regardless of the console code page.
# Read its redirected stream explicitly: Windows PowerShell native pipelines
# can otherwise decode it as CP936 and corrupt even JSON string terminators.
function Get-LauncherVisualStudioInstallation {
    [CmdletBinding()]
    param()

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer/vswhere was not found.' }
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $vswhere
    $startInfo.Arguments = '-latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json -utf8'
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.StandardOutputEncoding = New-Object System.Text.UTF8Encoding($false, $true)
    $startInfo.StandardErrorEncoding = New-Object System.Text.UTF8Encoding($false, $true)
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) { throw 'Could not start Visual Studio discovery.' }
        # Drain both pipes concurrently, including error cases.
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit(15000)) {
            $process.Kill()
            $process.WaitForExit()
            throw 'Visual Studio discovery timed out.'
        }
        $json = $stdout.GetAwaiter().GetResult()
        $errorText = $stderr.GetAwaiter().GetResult()
        if ($process.ExitCode -ne 0) { throw "Visual Studio discovery failed ($($process.ExitCode)): $errorText" }
        $json | ConvertFrom-Json -ErrorAction Stop
    } finally {
        $process.Dispose()
    }
}
