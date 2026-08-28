[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$obsProcess = Get-Process obs64 -ErrorAction SilentlyContinue
if ($obsProcess) {
	throw 'OBS Studio must be closed before installing the plugin.'
}

$source = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\build_x64\RelWithDebInfo\obs-vscode-privacy-guard.dll'))
$targetDirectory = 'C:\Program Files\obs-studio\obs-plugins\64bit'
$target = Join-Path $targetDirectory 'obs-vscode-privacy-guard.dll'

if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
	throw "Built plugin was not found at: $source"
}

if (-not (Test-Path -LiteralPath $targetDirectory -PathType Container)) {
	throw "OBS plugin directory was not found at: $targetDirectory"
}

Copy-Item -LiteralPath $source -Destination $target -Force

$sourceHash = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
$targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
if ($sourceHash -ne $targetHash) {
	throw 'Installed plugin hash does not match the build artifact.'
}

Write-Host "Installed and verified OBS plugin: $target"

