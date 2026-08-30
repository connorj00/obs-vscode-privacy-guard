[CmdletBinding()]
param(
	[string]$Remote = 'origin',

	[string]$Branch = 'develop'
)

$ErrorActionPreference = 'Stop'

# The first workflow push may happen before develop is bootstrapped.
$branchReference = git ls-remote --heads $Remote "refs/heads/$Branch"
if ($LASTEXITCODE -ne 0) {
	throw "Could not inspect remote branch $Remote/$Branch."
}

if (-not $branchReference) {
	Write-Host "Remote branch $Branch does not exist yet; synchronization was skipped."
	return
}

# Fetch the exact current develop tip used by the ancestry safety check.
git fetch --no-tags $Remote "refs/heads/${Branch}:refs/remotes/${Remote}/${Branch}"
if ($LASTEXITCODE -ne 0) {
	throw "Could not fetch $Remote/$Branch."
}

# A fast-forward is safe only when the release contains every develop commit.
git merge-base --is-ancestor "refs/remotes/$Remote/$Branch" HEAD
if ($LASTEXITCODE -ne 0) {
	throw "$Remote/$Branch contains commits that are not present in the released main commit. Synchronization stopped without changing the branch."
}

# A normal push provides a second race-condition guard if develop changes now.
git push $Remote "HEAD:refs/heads/$Branch"
if ($LASTEXITCODE -ne 0) {
	throw "The fast-forward of $Remote/$Branch was rejected. The branch may have changed during synchronization."
}

Write-Host "Fast-forwarded $Remote/$Branch to the released main commit."
