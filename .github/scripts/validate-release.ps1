[CmdletBinding()]
param(
	[Parameter(Mandatory)]
	[string]$BaseCommit,

	[string]$SourceBranch,

	[string]$SourceRepository,

	[string]$ExpectedRepository,

	[switch]$WriteGitHubOutputs
)

$ErrorActionPreference = 'Stop'

if ($SourceBranch) {
	if ($SourceBranch -notmatch '^(release|hotfix)/.+$') {
		throw "Pull requests into main must come from a release/* or hotfix/* branch. Received: $SourceBranch"
	}

	if (-not $SourceRepository -or -not $ExpectedRepository) {
		throw 'Repository identity is required when validating a pull request into main.'
	}

	if ($SourceRepository -ne $ExpectedRepository) {
		throw "Pull requests into main must come from an internal branch in $ExpectedRepository. Received: $SourceRepository"
	}
}

# Read a version file from the base commit without modifying the worktree.
function Get-JsonAtCommit {
	param(
		[Parameter(Mandatory)]
		[string]$Commit,

		[Parameter(Mandatory)]
		[string]$Path
	)

	$json = git show "${Commit}:$Path"
	if ($LASTEXITCODE -ne 0) {
		throw "Could not read $Path at commit $Commit."
	}

	return $json | ConvertFrom-Json
}

# Enforce the numeric major.minor.patch format shared by both packaging systems.
function Assert-VersionFormat {
	param(
		[Parameter(Mandatory)]
		[string]$Component,

		[Parameter(Mandatory)]
		[string]$Version
	)

	$versionPattern = '^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$'
	if ($Version -notmatch $versionPattern) {
		throw "$Component version must use major.minor.patch format. Received: $Version"
	}
}

function Assert-VersionIncremented {
	param(
		[Parameter(Mandatory)]
		[string]$Component,

		[Parameter(Mandatory)]
		[string]$PreviousVersion,

		[Parameter(Mandatory)]
		[string]$CurrentVersion
	)

	Assert-VersionFormat -Component "$Component existing" -Version $PreviousVersion
	Assert-VersionFormat -Component $Component -Version $CurrentVersion

	if ([System.Version]$CurrentVersion -le [System.Version]$PreviousVersion) {
		throw "$Component changes require a version newer than $PreviousVersion. Received: $CurrentVersion"
	}
}

# A missing version tag identifies a component that has never been released.
function Test-VersionTagExists {
	param(
		[Parameter(Mandatory)]
		[string]$Tag
	)

	$matchingTag = git tag --list $Tag
	if ($LASTEXITCODE -ne 0) {
		throw "Could not determine whether Git tag $Tag exists."
	}

	return $null -ne $matchingTag
}

# Compare the exact base and candidate commits to identify affected components.
$changedFiles = @(git diff --name-only $BaseCommit HEAD)
if ($LASTEXITCODE -ne 0) {
	throw "Could not compare the pull request with base commit $BaseCommit."
}

$obsChanged = $null -ne ($changedFiles | Where-Object { $_ -like 'obs-plugin/*' } | Select-Object -First 1)
$vscodeChanged = $null -ne ($changedFiles | Where-Object { $_ -like 'vscode-extension/*' } | Select-Object -First 1)
$currentBuildSpec = Get-Content -LiteralPath 'obs-plugin/buildspec.json' -Raw | ConvertFrom-Json
$currentPackage = Get-Content -LiteralPath 'vscode-extension/package.json' -Raw | ConvertFrom-Json
$currentLock = Get-Content -LiteralPath 'vscode-extension/package-lock.json' -Raw | ConvertFrom-Json -AsHashtable

if ($obsChanged) {
	$previousBuildSpec = Get-JsonAtCommit -Commit $BaseCommit -Path 'obs-plugin/buildspec.json'
	Assert-VersionIncremented `
		-Component 'OBS plugin' `
		-PreviousVersion $previousBuildSpec.version `
		-CurrentVersion $currentBuildSpec.version
}

if ($vscodeChanged) {
	$previousPackage = Get-JsonAtCommit -Commit $BaseCommit -Path 'vscode-extension/package.json'
	Assert-VersionIncremented `
		-Component 'VS Code extension' `
		-PreviousVersion $previousPackage.version `
		-CurrentVersion $currentPackage.version
}

$obsReleaseRequired = $obsChanged
$vscodeReleaseRequired = $vscodeChanged

# Post-merge runs also bootstrap any current version that has no release tag.
if ($WriteGitHubOutputs) {
	Assert-VersionFormat -Component 'OBS plugin' -Version $currentBuildSpec.version
	Assert-VersionFormat -Component 'VS Code extension' -Version $currentPackage.version

	if (-not (Test-VersionTagExists -Tag "obs-v$($currentBuildSpec.version)")) {
		$obsReleaseRequired = $true
		Write-Host "OBS plugin v$($currentBuildSpec.version) has no release tag and will be built."
	}

	if (-not (Test-VersionTagExists -Tag "vscode-v$($currentPackage.version)")) {
		$vscodeReleaseRequired = $true
		Write-Host "VS Code extension v$($currentPackage.version) has no release tag and will be built."
	}
}

if ($vscodeReleaseRequired) {
	if ($currentLock.version -ne $currentPackage.version -or $currentLock.packages[''].version -ne $currentPackage.version) {
		throw 'The VS Code package.json and package-lock.json versions must match.'
	}
}

if (-not $obsReleaseRequired -and -not $vscodeReleaseRequired) {
	Write-Host 'No component release is required for these changes.'
	if ($WriteGitHubOutputs) {
		'obs=false' >> $env:GITHUB_OUTPUT
		'vscode=false' >> $env:GITHUB_OUTPUT
	}

	return
}

if ($WriteGitHubOutputs) {
	"obs=$($obsReleaseRequired.ToString().ToLowerInvariant())" >> $env:GITHUB_OUTPUT
	"vscode=$($vscodeReleaseRequired.ToString().ToLowerInvariant())" >> $env:GITHUB_OUTPUT
}

Write-Host 'All required component releases have valid versions.'
