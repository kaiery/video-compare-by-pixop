[CmdletBinding()]
param([string[]]$ResultDirectories=@('matrix','retest','interaction','focus-retest','scan-retest','final-interaction','targeted-probe','resume-safe','resume-visual','resume-visual-hstack'))
$ErrorActionPreference='Stop'
$guiRoot=Split-Path -Parent $PSScriptRoot
$all=@{}
foreach($directory in $ResultDirectories){
 if($directory -notmatch '^[a-zA-Z0-9_-]+$'){throw 'Use simple evidence directory names.'}
 $file=Join-Path $guiRoot "work/p07/$directory/results.tsv"
 if(-not (Test-Path -LiteralPath $file)){throw "Missing result file: $file"}
 foreach($row in (Import-Csv -LiteralPath $file -Delimiter "`t")){
  $row | Add-Member evidence "work/p07/$directory/$($row.id)-$($row.variant.Replace(':','-'))" -Force
  $all["$($row.id)/$($row.variant)"]=$row
 }
}
$queryIds=@('CLI-001','CLI-002','CLI-003','CLI-035','CLI-046','CLI-050','CLI-054','CLI-058')
$prior=@{
 'IN-002'='P03-ACCEPTANCE.md';'IN-007'='P02-ACCEPTANCE.md';'IN-009'='P06-ACCEPTANCE.md';
 'IN-010'='P06-ACCEPTANCE.md';'IN-011'='P03-ACCEPTANCE.md';'IN-012'='P06-ACCEPTANCE.md';
 'GUI-001'='P05-ACCEPTANCE.md';'GUI-002'='P06-ACCEPTANCE.md';'GUI-003'='P05-ACCEPTANCE.md';
 'GUI-004'='P05-ACCEPTANCE.md';'GUI-005'='P06-ACCEPTANCE.md';'GUI-006'='P05-ACCEPTANCE.md';
 'GUI-007'='P06-ACCEPTANCE.md';'GUI-009'='P05-ACCEPTANCE.md';'GUI-010'='P06-ACCEPTANCE.md'
}
$notes=@{
 'CLI-010'='10-bpc texture path passed; SDL window is RGB888. Physical 10-bit output is not verified.';
 'CLI-014'='Display 0 passed; SDL reports display indices 0..0. Display 1 is unavailable in this session.';
 'CLI-028'='Synthetic PQ-tagged input; conversion-path equivalence only, not HDR photometric validation.';
 'GUI-008'='96-DPI window evidence exists; actual 150%/200% DPI and full keyboard traversal remain unverified.';
 'GUI-011'='Debug/Release build and original-file boundary checked; clean-directory distribution belongs to P08.';
 'RUN-011'='Interactive crop/copy/undo combinations require observed selection and output verification.';
 'RUN-014'='Mouse slider, seek, right-drag and modifier behavior require observed input/output verification.'
}
$table=[IO.File]::ReadAllText((Join-Path $guiRoot 'COVERAGE.md'))
$ids=@([regex]::Matches($table,'(?m)^\| ((?:CLI|RV|IN|RUN|GUI)-\d{3}) \|') | ForEach-Object {$_.Groups[1].Value} | Sort-Object -Unique)
if($ids.Count -ne 111 -or @($ids | Sort-Object -Unique).Count -ne 111){throw 'Coverage baseline changed; review report generation.'}
$rows=@();$lines=@('# P07 results register','','Generated from raw evidence; status is scoped to the stated observation. See [P07-ACCEPTANCE.md](P07-ACCEPTANCE.md) for the sign-off boundary.','','Source option, GUI control, parameter scope and acceptance requirement: [COVERAGE.md](COVERAGE.md), [P04-CONTROLS.md](P04-CONTROLS.md).','','| ID | Status | Actual observation / remaining work | Evidence |','|---|---|---|---|')
foreach($id in $ids){
 $sub=@($all.Values | Where-Object id -eq $id | Sort-Object variant)
 $status='TODO';$note='Not executed.';$evidence=''
 if($sub.Count){
  $status=if(@($sub | Where-Object status -eq 'FAIL').Count){'FAIL'}elseif(@($sub | Where-Object status -eq 'BLOCKED').Count){'BLOCKED'}else{'PASS'}
  $note="$($sub.Count) engine subcases: exit, frame PNG bytes, window dimensions/count compared. Parameter/default/error tests: P02/P04."
  $evidence=($sub | ForEach-Object {"[$($_.variant)]($($_.evidence)/observation-index.md)"}) -join ', '
  # Each case contains independent GUI and CLI observations.
  foreach($item in $sub){
   $index=Join-Path $guiRoot "$($item.evidence)/observation-index.md"
   $links=@('[Session](session.vcgui)')
   foreach($side in @('gui','cli')){if(Test-Path -LiteralPath (Join-Path $guiRoot "$($item.evidence)/$side/observation.txt")){$links+="[$side]($side/observation.txt)"}}
   [IO.File]::WriteAllText($index,(($links -join ' | ')+"`nStatus: $($item.status). Diagnostic/condition: $($item.condition)`n"),[Text.UTF8Encoding]::new($false))
  }
 }
 if($id -in $queryIds){$status='PASS';$note='Real-engine independent GUI query verified in P06; inherited evidence, not a new P07 execution.';$evidence='[query results](work/p06/real-config-query.json)'}
 if($prior.ContainsKey($id)){$status='PASS';$note='Inherited stage evidence plus current core regression; see linked checks for exact scope.';$evidence="[prior acceptance]($($prior[$id]))"}
 if($id -eq 'IN-008'){$status='PASS';$note='Ordered files / automatic loading / CLI overrides: CLI-005 and CLI-060; P06 source inspection and boolean semantics.';$evidence='[P06](P06-ACCEPTANCE.md), CLI-005/060 above'}
 $visualVerified=$false
 if($id -in @('RUN-001','RUN-006') -and $sub.Count -eq 1 -and $status -eq 'PASS'){
  $count=if($id -eq 'RUN-001'){14}else{8}
  $visualVerified=$true
  foreach($side in @('gui','cli')){
   $path=Join-Path $guiRoot "$($sub[0].evidence)/$side/observation.txt"
   if(-not (Test-Path -LiteralPath $path) -or [IO.File]::ReadAllText($path) -notmatch "action_verified=1 captured_states=$count(?:\r|\n)"){$visualVerified=$false}
  }
  if($visualVerified){$note="Observed intermediate states and GUI/CLI rendered screenshots match ($count compared states); visual change/restore assertions passed. RUN-001 dynamic HUD text excluded from cross-process bytes; its toggle/restore is still checked."}
 }
 if($id -like 'RUN-*' -and $id -notin @('RUN-012','RUN-015') -and -not $visualVerified){
  if($status -eq 'PASS'){$status='TODO'}
  $note='Keyboard smoke is not complete functional sign-off. Review all modifiers, intermediate state, mouse/selection and clipboard effects; final PNG equality alone is insufficient. '+$note
 }
 if($id -in @('RUN-002','RUN-004') -and $status -eq 'TODO'){$status='BLOCKED';$note='Manual verification required for playback speed/loop timing or wheel/drag/modifier effects. Safe key-sequence smoke passed, but full interaction is not signed off. '+$note}
 if($id -in @('CLI-010','CLI-014','GUI-008') -and $status -ne 'FAIL'){$status='BLOCKED'}
 if($id -eq 'GUI-011'){$status='N/A';$note='P07 independent builds and original-file boundary passed; clean-directory distribution is deferred to P08, not represented as already verified.'}
 if($notes.ContainsKey($id)){$note=$notes[$id]+' '+$note}
 if($id -eq 'GUI-011'){
  $packageReport=Join-Path $guiRoot 'work/p08/package-verification.json'
  if(Test-Path -LiteralPath $packageReport){
   $package=Get-Content -LiteralPath $packageReport -Raw -Encoding UTF8 | ConvertFrom-Json
   if($package.status -eq 'PASS' -and (Test-Path -LiteralPath $package.archive) -and (Get-FileHash -LiteralPath $package.archive).Hash -eq $package.sha256){$status='PASS';$note='P08: x64 builds, archive/manifest checks and freshly unpacked GUI with real engine/media passed; portable work/log paths and normal stop verified.';$evidence='[P08 acceptance](P08-ACCEPTANCE.md); [package verification](work/p08/package-verification.json)'}
  }
 }
 if($id -in @('RUN-013','RUN-014','RUN-015') -and $ResultDirectories -contains 'final-interaction'){
  $sourceRound=if($sub.Count){($sub[0].evidence -split '/')[2]}else{''}
  if([array]::IndexOf($ResultDirectories,$sourceRound) -le [array]::IndexOf($ResultDirectories,'final-interaction')){$status='TODO';$note='Interrupted round has no completion record; no newer completed subcase is selected. '+$note}
 }
 if($id -in @('RUN-008','RUN-011') -and $status -eq 'BLOCKED'){
  $status='FAIL';$note='Historical final-interaction failure remains OPEN. Latest automated attempt is BLOCKED (manual input required), not a successful retest. Failure audit shows identical launch command but different observed interaction states. '+$note
  $evidence+='; [failure audit](work/p07/failure-audit.json); [historical GUI](work/p07/final-interaction/'+$id+'-keys/gui/observation.txt); [historical CLI](work/p07/final-interaction/'+$id+'-keys/cli/observation.txt)'
 }
 if($id -in @('RUN-008','RUN-013') -and $sub.Count -eq 1 -and $sub[0].condition -like 'manual-input-required*' -and (Test-Path -LiteralPath (Join-Path $guiRoot 'P07-USER-VERIFICATION.md'))){
  $status='BLOCKED'
  $note=if($id -eq 'RUN-008'){'User log (PID 70072) confirms pixels, PSNR/SSIM/VMAF and Display state through GUI. Previous missing-Display-state failure point is resolved; visible FPS remains unverified, so full row is not PASS.'}else{'User log (PID 70072) confirms Saved/Restored saved/Restored startup size (480x320). Actual resize-and-restore visual observation was not reported; log subcases pass, full row remains unverified.'}
  $evidence+='; [user-provided log evidence](P07-USER-VERIFICATION.md)'
 }
 $rows += [pscustomobject]@{id=$id;status=$status;observation=$note;evidence=$evidence;engine='20260828-reykjavik'}
 $lines += "| $id | $status | $note | $evidence |"
}
[IO.File]::WriteAllLines((Join-Path $guiRoot 'P07-RESULTS.md'),$lines,[Text.UTF8Encoding]::new($false))
$rows | ConvertTo-Json -Depth 5 | Out-File (Join-Path $guiRoot 'work/p07/coverage-results.json') -Encoding UTF8
$all.Values | Sort-Object id,variant | Export-Csv (Join-Path $guiRoot 'work/p07/merged-subcases.csv') -NoTypeInformation -Encoding UTF8
$rows | Group-Object status | Select-Object Name,Count
