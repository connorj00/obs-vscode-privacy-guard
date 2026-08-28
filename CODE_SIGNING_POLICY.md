# Code Signing Policy

## Current signing status

OBS-VSCode Privacy Guard is not currently code signed. This policy documents
the intended release process and does not claim that existing builds have a
trusted signature.

After the project has been accepted into the SignPath Foundation open-source
program, signed release artifacts will use the following attribution:

> Free code signing provided by SignPath.io, certificate by SignPath Foundation.

## Official source

The official source repository is:

<https://github.com/connorj00/obs-vscode-privacy-guard>

Only artifacts built from source and build definitions committed to that
repository are eligible for release signing.

## Signing scope

The release-signing policy is intended to cover project-owned Windows release
artifacts, including:

- The native OBS plugin DLL and its distribution package.
- The packaged VS Code extension.

Third-party or upstream binaries are not signed as if they were produced by
this project.

## Build provenance and approval

Signed releases must:

- Be built by the project's approved GitHub Actions release workflow.
- Originate from an official version tag in the repository.
- Use the build scripts and dependency definitions committed with that tag.
- Pass the automated test suite before submission for signing.
- Be submitted through SignPath's trusted-build integration.
- Receive explicit release approval before publication.

Locally supplied or modified binaries are not eligible for release signing.
Signing credentials and private keys must not be committed to the repository.

## Team roles

| Role                 | Member                                                       | Responsibility                                          |
| -------------------- | ------------------------------------------------------------ | ------------------------------------------------------- |
| Author and committer | Connor J Davies ([@connorj00](https://github.com/connorj00)) | Maintains the source code and build definitions.        |
| Reviewer             | Connor J Davies ([@connorj00](https://github.com/connorj00)) | Reviews contributed changes before they are merged.     |
| Approver             | Connor J Davies ([@connorj00](https://github.com/connorj00)) | Approves artifacts for release signing and publication. |

All accounts with repository or signing access must use multi-factor
authentication. Contributors who do not have direct commit access must submit
changes through the repository's review process.

## Privacy

The project's data handling is documented in the
[Privacy Policy](PRIVACY.md).
