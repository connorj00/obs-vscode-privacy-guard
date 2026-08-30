[CmdletBinding()]
param(
	[Parameter(Mandatory)]
	[string]$BaseCommit,

	[switch]$AllowVersionChanges
)

$ErrorActionPreference = 'Stop'

# Read the released versions from the current develop branch tip.
$previousBuildSpec = git show "${BaseCommit}:obs-plugin/buildspec.json" | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) {
	throw "Could not read the OBS version at base commit $BaseCommit."
}

$previousPackage = git show "${BaseCommit}:vscode-extension/package.json" | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) {
	throw "Could not read the VS Code version at base commit $BaseCommit."
}

$currentBuildSpec = Get-Content -LiteralPath 'obs-plugin/buildspec.json' -Raw | ConvertFrom-Json
$currentPackage = Get-Content -LiteralPath 'vscode-extension/package.json' -Raw | ConvertFrom-Json
$currentLock = Get-Content -LiteralPath 'vscode-extension/package-lock.json' -Raw | ConvertFrom-Json -AsHashtable

if ($currentLock.version -ne $currentPackage.version -or $currentLock.packages[''].version -ne $currentPackage.version) {
	throw 'The VS Code package.json and package-lock.json versions must match.'
}

if ($AllowVersionChanges) {
	Write-Host 'Version changes are allowed while synchronizing main back into develop.'
	return
}

if ($currentBuildSpec.version -ne $previousBuildSpec.version) {
	throw 'Contributor PRs into develop must not change the OBS plugin version. Versions are selected in release branches.'
}

if ($currentPackage.version -ne $previousPackage.version) {
	throw 'Contributor PRs into develop must not change the VS Code extension version. Versions are selected in release branches.'
}

Write-Host 'Component versions are unchanged, as required for develop.'
