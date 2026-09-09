[CmdletBinding()]
param(
	[Parameter(Mandatory)]
	[ValidatePattern('^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$')]
	[string]$Version,

	[Parameter(Mandatory)]
	[string]$AssetDirectory
)

$ErrorActionPreference = 'Stop'
$Tag = "v$Version"
$Title = "OBS–VS Code Privacy Guard v$Version"
$Notes = 'This product release contains both required components: the OBS Studio plugin (Windows installer and standalone DLL) and the VS Code extension (VSIX). Install both components at this version. The Windows EXE and DLL are unsigned; keep this release as a draft until they have been signed and approved for publication. SignPath integration is planned for the EXE and DLL only; the VSIX does not require SignPath signing.'
$Assets = @(
	(Join-Path $AssetDirectory "obs-vscode-privacy-guard-setup-$Version.exe")
	(Join-Path $AssetDirectory "obs-vscode-privacy-guard-$Version.dll")
	(Join-Path $AssetDirectory "obs-vscode-privacy-guard-$Version.vsix")
)

if ($env:GITHUB_SHA -notmatch '^[0-9a-f]{40}$' -or $env:GITHUB_REPOSITORY -notmatch '^[\w.-]+/[\w.-]+$') {
	throw 'GITHUB_SHA and GITHUB_REPOSITORY must identify the exact release commit and repository.'
}

# Fail before contacting GitHub when a build did not produce every expected asset.
foreach ($asset in $Assets) {
	if (-not (Test-Path -LiteralPath $asset -PathType Leaf)) {
		throw "Release asset was not found: $asset"
	}
}

# Inspect the remote as well as the checkout: a tag may have appeared since checkout.
$tagReferences = @(git ls-remote --tags origin "refs/tags/$Tag" "refs/tags/$Tag^{}")
if ($LASTEXITCODE -ne 0) {
	throw "Could not inspect remote tag $Tag."
}
$tagExists = $tagReferences.Count -gt 0
foreach ($reference in $tagReferences) {
	if ($reference -match '\^\{\}$' -or $tagReferences.Count -eq 1) {
		$tagCommit = ($reference -split '\s+')[0]
		if ($tagCommit -ne $env:GITHUB_SHA) {
			throw "Git tag $Tag already points to a different commit: $tagCommit"
		}
	}
}

# Listing releases distinguishes absence from authentication or network failures.
# Include all pages so older drafts cannot be missed.
$releaseJson = gh api --paginate --slurp "repos/$env:GITHUB_REPOSITORY/releases?per_page=100"
if ($LASTEXITCODE -ne 0) {
	throw 'Could not inspect existing GitHub releases.'
}
$pages = $releaseJson | ConvertFrom-Json
$matches = @($pages | ForEach-Object { $_ } | Where-Object { $_.tag_name -ceq $Tag })
if ($matches.Count -gt 1) {
	throw "Multiple releases match $Tag; refusing to update an ambiguous release."
}

if ($matches.Count -eq 1) {
	$release = $matches[0]
	if (-not $release.draft -or $release.target_commitish -ne $env:GITHUB_SHA) {
		throw "Release $Tag must be a draft targeting exactly $env:GITHUB_SHA before it can be updated."
	}

	gh release edit $Tag --repo $env:GITHUB_REPOSITORY --title $Title --notes $Notes
	if ($LASTEXITCODE -ne 0) {
		throw "Could not update the existing draft release $Tag."
	}

	gh release upload $Tag @Assets --repo $env:GITHUB_REPOSITORY --clobber
	if ($LASTEXITCODE -ne 0) {
		throw "Could not upload all assets to the existing draft release $Tag."
	}

	Write-Host "Updated draft release: $Tag"
	return
}

if ($tagExists) {
	throw "Git tag $Tag already exists without a matching draft release."
}

gh release create $Tag @Assets --repo $env:GITHUB_REPOSITORY --draft --target $env:GITHUB_SHA --title $Title --notes $Notes
if ($LASTEXITCODE -ne 0) {
	throw "Could not create draft release $Tag."
}

Write-Host "Created draft release: $Tag"
