# Contributing

Thank you for contributing to OBS–VS Code Privacy Guard.

## Workflow

All normal pull requests must target `develop`. The `main` branch represents
released code and only accepts maintainer-managed `release/*` and `hotfix/*`
branches.

Create your branch from the latest `develop`:

```powershell
git switch develop
git pull --ff-only
git switch -c <type>/<scope>/<description>
```

## Branch names

For a change affecting one component, use:

```text
<type>/<scope>/<description>
```

Recommended types:

| Type       | Purpose                                                     |
| ---------- | ----------------------------------------------------------- |
| `feat`     | New functionality                                           |
| `fix`      | Bug fix                                                     |
| `docs`     | Documentation                                               |
| `refactor` | Internal restructuring without intended behavioural changes |
| `test`     | Test changes                                                |
| `ci`       | CI/CD and release automation                                |
| `build`    | Build system or dependency changes                          |
| `chore`    | Repository maintenance                                      |

Examples:

```text
fix/obs/restore-scene
feat/vscode/add-rule-type
docs/vscode/update-readme
```

For a change affecting both components or the whole project, omit the scope:

```text
<type>/<description>
```

Examples:

```text
fix/update-connection-protocol
docs/update-installation-guide
ci/improve-release-workflow
```

Use lowercase words separated by hyphens.

## Versions

Do not change component versions in normal pull requests. The maintainer selects
versions when preparing a release branch.

## Testing

### VS Code extension

```powershell
cd vscode-extension
npm ci
npm test
```

### OBS plugin

```powershell
cd obs-plugin
./scripts/build-windows.ps1
```

If your change affects the installer, also run:

```powershell
./scripts/build-installer-windows.ps1
```

## Code standards

- Follow `.editorconfig` and `.clang-format`.
- Use tabs with a width of 4 for source-code indentation.
- Use valid format-specific indentation in files such as YAML and JSON.
- Keep changes focused and avoid unrelated formatting.
- Prefer existing language and framework functionality over unnecessary custom
  implementations or dependencies.
- Preserve the project's fail-closed behaviour described in
  [`docs/architecture.md`](docs/architecture.md).

## Recommended commit messages:

Use the same types and scopes as the branch naming convention
`<type>(<optional-scope>): <description>` - example:

`fix(obs): restoring previous user-managed scene correctly`

Recommended types:

| Type       | Purpose                                                     |
| ---------- | ----------------------------------------------------------- |
| `feat`     | New functionality                                           |
| `fix`      | Bug fix                                                     |
| `docs`     | Documentation                                               |
| `refactor` | Internal restructuring without intended behavioural changes |
| `test`     | Test changes                                                |
| `ci`       | CI/CD and release automation                                |
| `build`    | Build system or dependency changes                          |
| `chore`    | Repository maintenance                                      |

## Pull requests

Before submitting a pull request:

- Target `develop`.
- Run the relevant tests.
- Review your diff for unrelated changes or local machine paths.
- Update documentation when behaviour or configuration changes.
- Confirm all CI checks pass.

## Licence

By contributing, you agree that your contribution will be licensed under the
repository's MIT Licence.
