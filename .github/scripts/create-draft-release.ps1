[CmdletBinding()]
param(
	[Parameter(Mandatory)]
	[string]$Tag,

	[Parameter(Mandatory)]
	[string]$Title,

	[Parameter(Mandatory)]
	[string]$Notes,

	[Parameter(Mandatory)]
	[string[]]$Assets
)

$ErrorActionPreference = 'Stop'

# Fail before contacting GitHub when a build did not produce every expected asset.
foreach ($asset in $Assets) {
	if (-not (Test-Path -LiteralPath $asset -PathType Leaf)) {
		throw "Release asset was not found: $asset"
	}
}

# Never create or update a release whose tag belongs to another commit.
$tagCommit = git rev-list -n 1 $Tag 2>$null
if ($LASTEXITCODE -eq 0 -and $tagCommit -and $tagCommit -ne $env:GITHUB_SHA) {
	throw "Git tag $Tag already points to a different commit: $tagCommit"
}

# Updating existing drafts makes a partially completed release job safe to rerun.
$releaseJson = gh release view $Tag --json isDraft 2>$null
$releaseExists = $LASTEXITCODE -eq 0

if ($releaseExists) {
	$release = $releaseJson | ConvertFrom-Json
	if (-not $release.isDraft) {
		throw "Release $Tag is already published and cannot be replaced."
	}

	gh release edit $Tag --title $Title --notes $Notes
	if ($LASTEXITCODE -ne 0) {
		throw "Could not update the existing draft release $Tag."
	}

	gh release upload $Tag @Assets --clobber
	if ($LASTEXITCODE -ne 0) {
		throw "Could not upload all assets to the existing draft release $Tag."
	}

	Write-Host "Updated draft release: $Tag"
	return
}
gh release create $Tag @Assets --draft --target $env:GITHUB_SHA --title $Title --notes $Notes
if ($LASTEXITCODE -ne 0) {
	throw "Could not create draft release $Tag."
}

Write-Host "Created draft release: $Tag"
