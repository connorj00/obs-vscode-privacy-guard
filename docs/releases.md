# Product releases

OBS–VS Code Privacy Guard ships both required components in one draft GitHub
release titled `OBS–VS Code Privacy Guard v<version>` with tag `v<version>`:

- `obs-vscode-privacy-guard-setup-<version>.exe`
- `obs-vscode-privacy-guard-<version>.dll`
- `obs-vscode-privacy-guard-<version>.vsix`

Normal contributor PRs target `develop` and leave versions unchanged. Maintainers
prepare an internal `release/*` or `hotfix/*` branch targeting `main`, update
`obs-plugin/buildspec.json`, `vscode-extension/package.json`, and both root version
entries in `vscode-extension/package-lock.json` to the same `major.minor.patch`,
and merge the release PR using a merge commit. The new version must exceed the
highest existing unified product version, whose tag must be an ancestor of HEAD.

Changes under either component directory, including build configuration,
packaging, resources, tests, and version files, require release validation.
Markdown/reStructuredText documentation, documentation directories, and CI/editor
metadata are excluded. Changes outside the component directories do not trigger
a product release. Detection first checks the incoming main push (or PR diff),
then compares against the previous unified release. A documentation-only or
CI-only push does not bootstrap or retry an unreleased product version.

On a qualifying push to `main`, the release workflow validates the shared version,
builds and tests both components in parallel, and uploads temporary Actions
artifacts. The final job downloads both artifacts and attaches all three files
to one draft. The standalone DLL is renamed during staging; the installer still
installs the normal unversioned DLL filename expected by OBS.

The workflow fast-forwards `develop` only after both builds and the draft release
succeed, or after successful validation when no release is needed. The existing
ancestry and concurrent-update checks still apply; divergent `develop` requires
maintainer resolution. A missing `develop` branch is skipped.

## Initial migration and retries

The first unified release is `v0.0.2`: OBS moves from `0.0.1` to `0.0.2`, while
the VS Code package and lockfile remain at `0.0.2`. Without a unified tag, the
shared version may equal the higher existing component version, but may not
downgrade either component. Legacy `obs-v*` and `vscode-v*` tags are ignored and
must be retained along with their published releases. Do not create `v0.0.2`
manually before the workflow runs.

An existing unified tag fails release validation. To retry a partially completed
run, rerun failed jobs so the successful validation outputs are reused. The draft
helper can update only an unpublished draft with the exact same tag and full
target commit SHA. It refuses published releases, branch-name targets, conflicting
tags, or tags without a matching draft. API failures stop the job rather than
being treated as a missing release.

## Signing and publication

The EXE and DLL are currently unsigned. Keep the release as a draft pending
signing and explicit publication approval. SignPath integration is planned for
both Windows files; the VSIX is outside that signing scope. No automatic
publication or signing is implemented by this workflow.
