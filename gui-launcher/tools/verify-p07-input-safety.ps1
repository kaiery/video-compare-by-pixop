[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$guiRoot=Split-Path -Parent $PSScriptRoot
$source=[IO.File]::ReadAllText((Join-Path $guiRoot 'tests/engine_acceptance.cpp'))
# Regression guard for the editor-closing incident. No focus stealing or global injection.
$forbidden='\b(SendInput|keybd_event|mouse_event|SetForegroundWindow|AttachThreadInput|SetCursorPos|SwitchDesktop|SetKeyboardState)\s*\('
if($source -match $forbidden){throw 'P07 must not inject global input or change input focus/desktop.'}
if($source -match 'HWND_BROADCAST'){throw 'P07 must not broadcast input.'}
if($source -notmatch 'owner!=input_owner'){throw 'Owned-process keyboard target validation is missing.'}
if($source -notmatch 'manual-input-required; global input disabled'){throw 'Manual-only case guard is missing.'}
Write-Output 'PASS: global input/focus APIs absent; owned-process target check and manual-case guard present.'
