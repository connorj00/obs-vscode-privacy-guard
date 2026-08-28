[CmdletBinding()]
param(
	[string]$Version
)

$ErrorActionPreference = 'Stop'

$pluginRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildSpecPath = Join-Path $pluginRoot 'buildspec.json'
$sourceDll = Join-Path $pluginRoot 'build_x64\RelWithDebInfo\obs-vscode-privacy-guard.dll'
$installerScript = Join-Path $pluginRoot 'installer\windows\obs-vscode-privacy-guard.iss'
$outputDirectory = Join-Path $pluginRoot 'dist'

if (-not $Version) {
	$buildSpec = Get-Content -LiteralPath $buildSpecPath -Raw | ConvertFrom-Json
	$Version = $buildSpec.version
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
	throw "Installer version must use major.minor.patch format. Received: $Version"
}

if (-not (Test-Path -LiteralPath $sourceDll -PathType Leaf)) {
	throw "Built OBS plugin was not found at: $sourceDll"
}

$compilerCandidates = @(
	(Get-Command 'ISCC.exe' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source),
	(Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
	'C:\Program Files (x86)\Inno Setup 6\ISCC.exe',
	'C:\Program Files\Inno Setup 6\ISCC.exe'
) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) }

$compiler = $compilerCandidates | Select-Object -First 1
if (-not $compiler) {
	throw 'Inno Setup 6 was not found. Install it before building the OBS installer.'
}

New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

& $compiler "/DAppVersion=$Version" "/DSourceDll=$sourceDll" $installerScript
if ($LASTEXITCODE -ne 0) {
	throw "Inno Setup failed with exit code $LASTEXITCODE."
}

$installer = Join-Path $outputDirectory "obs-vscode-privacy-guard-setup-$Version.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
	throw "Installer build completed without producing the expected file: $installer"
}

Write-Host "Built OBS installer: $installer"
