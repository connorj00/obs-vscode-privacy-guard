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

# Documentation and CI metadata inside a component are not product changes.
function Get-ProductChanges {
	param([string]$Commit)

	$files = if ($Commit -match '^0+$') {
		@(git ls-tree -r --name-only HEAD)
	} else {
		@(git diff --name-only --no-renames $Commit HEAD)
	}
	if ($LASTEXITCODE -ne 0) {
		throw "Could not compare product files with commit $Commit."
	}

	return $files | Where-Object {
		$_ -match '^(obs-plugin|vscode-extension)/' -and
		$_ -notmatch '(?i)(^|/)(docs?|\.github|\.vscode)/|\.(md|mdx|rst)$' -and
		$_ -notmatch '(^|/)(LICENSE|NOTICE|\.gitignore|\.prettierignore|\.prettierrc(?:\.json)?)$'
	}
}

$currentBuildSpec = Get-Content -LiteralPath 'obs-plugin/buildspec.json' -Raw | ConvertFrom-Json
$currentPackage = Get-Content -LiteralPath 'vscode-extension/package.json' -Raw | ConvertFrom-Json
$currentLock = Get-Content -LiteralPath 'vscode-extension/package-lock.json' -Raw | ConvertFrom-Json -AsHashtable
$version = $currentPackage.version
$releaseRequired = $false

# A missing unified tag alone must never turn a documentation push into a release.
# The all-zero before SHA denotes a newly created branch; inspect its full tree.
$incomingChanges = @(Get-ProductChanges -Commit $BaseCommit)
if ($incomingChanges.Count -gt 0) {
	Assert-VersionFormat -Component 'OBS plugin' -Version $currentBuildSpec.version
	Assert-VersionFormat -Component 'VS Code extension' -Version $version
	if ($currentBuildSpec.version -ne $version) {
		throw 'The OBS plugin and VS Code extension must have the same product version.'
	}
	if ($currentLock.version -ne $version -or $currentLock.packages[''].version -ne $version) {
		throw 'The VS Code package.json and package-lock.json versions must match.'
	}

	# Enumerating tags succeeds even when no tags match. Legacy component tags are ignored.
	$tags = @(git tag --list 'v*')
	if ($LASTEXITCODE -ne 0) {
		throw 'Could not enumerate unified product tags.'
	}
	$tags = @($tags | Where-Object { $_ -cmatch '^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$' })
	if ($tags -ccontains "v$version") {
		throw "Unified product tag v$version already exists. Choose a new version."
	}
	$previousTag = $tags | Sort-Object { [System.Version]$_.Substring(1) } -Descending | Select-Object -First 1

	# Require a new version even when the preceding release is still a draft.
	if ($BaseCommit -notmatch '^0+$') {
		$previousObs = Get-JsonAtCommit -Commit $BaseCommit -Path 'obs-plugin/buildspec.json'
		$previousVsCode = Get-JsonAtCommit -Commit $BaseCommit -Path 'vscode-extension/package.json'

		# Only the initial migration may reuse the existing VS Code version.
		$isMigration = (
			-not $previousTag -and
			$previousObs.version -eq '0.0.1' -and
			$previousVsCode.version -eq '0.0.2' -and
			$version -eq '0.0.2'
		)

		if (-not $isMigration) {
			Assert-VersionIncremented `
				-Component 'OBS plugin' `
				-PreviousVersion $previousObs.version `
				-CurrentVersion $version

			Assert-VersionIncremented `
				-Component 'VS Code extension' `
				-PreviousVersion $previousVsCode.version `
				-CurrentVersion $version
		}
	}

	if ($previousTag) {
		Assert-VersionIncremented -Component 'Product' -PreviousVersion $previousTag.Substring(1) -CurrentVersion $version

		git merge-base --is-ancestor "refs/tags/$previousTag" HEAD
		if ($LASTEXITCODE -ne 0) {
			throw "Previous product release $previousTag is not an ancestor of HEAD."
		}

		$releaseRequired = @(Get-ProductChanges -Commit "refs/tags/$previousTag").Count -gt 0
	} else {
		Write-Host 'No unified product tag exists; validating the initial product release.'
		$releaseRequired = $true
	}
}

if ($WriteGitHubOutputs) {
	"release=$($releaseRequired.ToString().ToLowerInvariant())" >> $env:GITHUB_OUTPUT
	"version=$version" >> $env:GITHUB_OUTPUT
}

Write-Host "Product release required: $releaseRequired (version $version)."
