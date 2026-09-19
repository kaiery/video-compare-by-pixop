[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'All')][string]$Configuration = 'All',
    [switch]$Install,
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
$guiRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'vs-discovery.ps1')
$instances = @(Get-LauncherVisualStudioInstallation)
if (-not $instances -or -not $instances[0].installationPath) { throw 'Install the Visual Studio x64/x86 C++ build tools.' }
$vsPath = $instances[0].installationPath
$vsMajor = ([version]$instances[0].installationVersion).Major
$cmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Install the Visual Studio C++ CMake tools.' }

$capabilities = (& $cmake -E capabilities) | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw 'Cannot query CMake generators.' }
$generator = @($capabilities.generators | Where-Object { $_.name -like "Visual Studio $vsMajor *" })[0].name
if (-not $generator) { throw "This CMake does not support Visual Studio $vsMajor." }
if ($Install -and $Configuration -eq 'Debug') { throw '-Install requires Release or All.' }
$oldLocation = Get-Location
try {
    Set-Location -LiteralPath $guiRoot
    $configureArguments = @('--preset', 'windows-x64', '-G', $generator, "-DCMAKE_GENERATOR_INSTANCE=$vsPath")
    if ($Test) { $configureArguments += '-DBUILD_TESTING=ON' }
    & $cmake @configureArguments
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    $configurations = if ($Configuration -eq 'All') { @('Debug', 'Release') } else { @($Configuration) }
    foreach ($config in $configurations) {
        & $cmake --build --preset $config.ToLowerInvariant()
        if ($LASTEXITCODE -ne 0) { throw "$config build failed." }
        if ($Test) {
            $ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
            $reportDirectory = Join-Path $guiRoot 'work/p02'
            New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
            & $ctest --test-dir (Join-Path $guiRoot 'build/msvc-x64') -C $config --output-on-failure --no-tests=error --output-junit (Join-Path $reportDirectory "$config-tests.xml")
            $testExitCode = $LASTEXITCODE
            $lastTestLog = Join-Path $guiRoot 'build/msvc-x64/Testing/Temporary/LastTest.log'
            if (Test-Path -LiteralPath $lastTestLog) {
                Copy-Item -LiteralPath $lastTestLog -Destination (Join-Path $reportDirectory "$config-cases.log")
            }
            if ($testExitCode -ne 0) { throw "$config tests failed. See the CTest report for the failing group." }
        }
    }
    if ($Install) {
        & $cmake --install (Join-Path $guiRoot 'build/msvc-x64') --config Release --prefix (Join-Path $guiRoot 'dist')
        if ($LASTEXITCODE -ne 0) { throw 'Release installation failed.' }
    }
} finally {
    Set-Location -LiteralPath $oldLocation.Path
}
