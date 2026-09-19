# Read-only documentation coverage check. Does not run or modify the engine.
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$guiRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Split-Path -Parent $guiRoot
$sourcePath = Join-Path $projectRoot 'src/main.cpp'
$tablePath = Join-Path $guiRoot 'COVERAGE.md'
$source = [IO.File]::ReadAllText($sourcePath)
$table = [IO.File]::ReadAllText($tablePath)

$start = $source.IndexOf('argagg::parser argparser{', [StringComparison]::Ordinal)
$end = $source.IndexOf('argagg::parser_results initial_args', [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Cannot locate CLI definitions; manually review source layout.' }
$block = $source.Substring($start, $end - $start)
$definitions = [regex]::Matches($block, '\{"([^"]+)",\s*\{([^}]+)\}')
$rows = [regex]::Matches($table, '(?m)^\| (CLI-\d{3}) \| `([^`]+)` \| ([^|]+) \|')

function Assert-SameSet($Expected, $Actual, [string]$Label) {
    $expectedList = @($Expected | Sort-Object -Unique)
    $actualList = @($Actual | Sort-Object -Unique)
    $difference = @(Compare-Object -CaseSensitive -ReferenceObject $expectedList -DifferenceObject $actualList)
    if ($difference.Count -ne 0) {
        $details = ($difference | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }) -join '; '
        throw "${Label} mismatch (<= missing from table; => not in source): $details"
    }
}

if ($definitions.Count -eq 0 -or $rows.Count -eq 0) { throw 'Empty CLI source or coverage table.' }
$sourceKeys = @($definitions | ForEach-Object { $_.Groups[1].Value })
$tableKeys = @($rows | ForEach-Object { $_.Groups[2].Value })
Assert-SameSet $sourceKeys $tableKeys 'CLI keys'
if ($rows.Count -ne @($tableKeys | Sort-Object -Unique).Count) { throw 'Duplicate CLI keys in coverage table.' }
$ids = @($rows | ForEach-Object { $_.Groups[1].Value })
if ($ids.Count -ne @($ids | Sort-Object -Unique).Count) { throw 'Duplicate CLI IDs.' }

foreach ($definition in $definitions) {
    $key = $definition.Groups[1].Value
    $row = @($rows | Where-Object { $_.Groups[2].Value -ceq $key })[0]
    $expectedFlags = @([regex]::Matches($definition.Groups[2].Value, '"(-[^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    $actualFlags = @([regex]::Matches($row.Groups[3].Value, '`(-[^`]+)`') | ForEach-Object { $_.Groups[1].Value })
    Assert-SameSet $expectedFlags $actualFlags "CLI aliases for $key"
}

$overrideStart = $source.IndexOf('void apply_right_video_spec(', [StringComparison]::Ordinal)
$overrideEnd = $source.IndexOf('int main(', [StringComparison]::Ordinal)
if ($overrideStart -lt 0 -or $overrideEnd -le $overrideStart) { throw 'Cannot locate right-video override function.' }
$overrideBlock = $source.Substring($overrideStart, $overrideEnd - $overrideStart)
$overrideKeys = @([regex]::Matches($overrideBlock, 'get_param\("([^"]+)"\)') | ForEach-Object { $_.Groups[1].Value })
$overrideRows = [regex]::Matches($table, '(?m)^\| (RV-\d{3}) \| `([^`]+)` \|')
$tableOverrideKeys = @($overrideRows | ForEach-Object { $_.Groups[2].Value })
if ($overrideKeys.Count -eq 0 -or $overrideRows.Count -eq 0) { throw 'Empty right-video source or coverage table.' }
Assert-SameSet $overrideKeys $tableOverrideKeys 'Right-video keys'
if ($overrideRows.Count -ne @($tableOverrideKeys | Sort-Object -Unique).Count) { throw 'Duplicate right-video keys.' }
$overrideIds = @($overrideRows | ForEach-Object { $_.Groups[1].Value })
if ($overrideIds.Count -ne @($overrideIds | Sort-Object -Unique).Count) { throw 'Duplicate right-video IDs.' }

Write-Output "PASS: $($definitions.Count) CLI options with all aliases; $($overrideKeys.Count) right-video override keys."

# Independently compare the P02 schema and test fixtures to source and documentation.
$catalog = [IO.File]::ReadAllText((Join-Path $guiRoot 'src/core/catalog.cpp'))
$catalogRows = [regex]::Matches($catalog, '\{"(CLI-\d{3})", "([^"]+)", L"([^"]+)", L"([^"]*)", S::(\w+), "([^"]+)", R::(\w+)')
$catalogKeys = @($catalogRows | ForEach-Object { $_.Groups[2].Value })
Assert-SameSet $sourceKeys $catalogKeys 'Implementation CLI keys'
if ($catalogRows.Count -ne $definitions.Count) { throw 'Implementation CLI row count mismatch.' }
foreach ($row in $catalogRows) {
    $key = $row.Groups[2].Value
    $documented = @($rows | Where-Object { $_.Groups[2].Value -ceq $key })[0]
    if ($row.Groups[1].Value -cne $documented.Groups[1].Value) { throw "Stable ID mismatch: $key" }
    $expectedFlags = @([regex]::Matches($documented.Groups[3].Value, '`(-[^`]+)`') | ForEach-Object { $_.Groups[1].Value })
    $actualFlags = @($row.Groups[3].Value, $row.Groups[4].Value | Where-Object { $_ })
    Assert-SameSet $expectedFlags $actualFlags "Implementation aliases for $key"
    $index = 0
    while ($definitions[$index].Groups[1].Value -cne $key) { $index++ }
    $partEnd = if ($index + 1 -lt $definitions.Count) { $definitions[$index + 1].Index } else { $block.Length }
    $part = $block.Substring($definitions[$index].Index, $partEnd - $definitions[$index].Index)
    $arities = [regex]::Matches($part, ',\s*([01])\s*\}')
    if ($arities.Count -eq 0) { throw "Cannot determine arity: $key" }
    $sourceTakesValue = $arities[$arities.Count - 1].Groups[1].Value -eq '1'
    $catalogTakesValue = $row.Groups[7].Value -ne 'Flag'
    if ($sourceTakesValue -ne $catalogTakesValue) { throw "Value arity mismatch: $key" }
}
$catalogOverrides = [regex]::Matches($catalog, '\{"(RV-\d{3})", "([^"]+)", R::')
Assert-SameSet $overrideKeys @($catalogOverrides | ForEach-Object { $_.Groups[2].Value }) 'Implementation override keys'
if ($catalogOverrides.Count -ne $overrideRows.Count) { throw 'Implementation override count mismatch.' }
foreach ($row in $catalogOverrides) {
    $documented = @($overrideRows | Where-Object { $_.Groups[2].Value -ceq $row.Groups[2].Value })[0]
    if ($documented.Groups[1].Value -cne $row.Groups[1].Value) { throw 'Override stable ID mismatch.' }
}
$cliFixture = [IO.File]::ReadAllText((Join-Path $guiRoot 'tests/cli_cases.inc'))
$rvFixture = [IO.File]::ReadAllText((Join-Path $guiRoot 'tests/override_cases.inc'))
$cliFixtureIds = @([regex]::Matches($cliFixture, '\{"(CLI-\d{3})"') | ForEach-Object { $_.Groups[1].Value })
$rvFixtureIds = @([regex]::Matches($rvFixture, '\{"(RV-\d{3})"') | ForEach-Object { $_.Groups[1].Value })
Assert-SameSet $ids $cliFixtureIds 'CLI test fixture IDs'
Assert-SameSet $overrideIds $rvFixtureIds 'Override test fixture IDs'
if ($cliFixtureIds.Count -ne $ids.Count -or $rvFixtureIds.Count -ne $overrideIds.Count) { throw 'Duplicate fixture IDs.' }
Write-Output 'PASS: implementation schema, value arities, stable IDs and independent fixtures match the source/table.'
Write-Output 'Static coverage only; run the core tests for behavior. Video-engine runtime acceptance is separate.'
