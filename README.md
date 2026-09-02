# OBS VS Code Privacy Guard

## 📑 1. Overview

OBS VS Code Privacy Guard is a paired VS Code extension and OBS Studio plugin
for developers who stream their work. It automatically protects the complete
OBS output whenever a sensitive file is visible in VS Code.

The project was created to reduce the chance of accidentally exposing secrets,
credentials, environment variables, private configuration, or other sensitive
files during a live stream. Instead of relying on the streamer to be alert and
change scenes in advance, Privacy Guard reacts as soon as a configured filename
rule matches and takes action.

Privacy Guard consists of two required components:

- The **VS Code extension** checks the basename of every visible editor against
  the configured sensitive-file rules.
- The **OBS plugin** receives only the resulting `SAFE` or `SENSITIVE` state sent
  from VSCode and controls what OBS renders based on the OBS Plugins config.

By default, Privacy Guard covers its entire output with an event-specific bundled
image over an opaque black background when:

- A visible filename matches a privacy rule.
- VS Code has not connected to OBS yet.
- The connection to VS Code is unexpectedly lost.
- Periodic state updates stop arriving.
- Invalid protocol data is received.

The OBS plugin is fail-closed: silence or uncertain state is never interpreted
as safe.

Within the OBS plugin config Users may replace either bundled image with their
own image while retaining post-render protection, or opt for a custom OBS scene.
Custom scenes and transitions are user-managed and should be tested before use.

### VSCode Sensitive-File Config Example

```json
{
	"obsPrivacyGuard.rules": [
		{
			"id": "dotenv",
			"type": "extension",
			"value": ".env"
		},
		{
			"id": "privacy",
			"type": "includes",
			"value": "privacy"
		}
	]
}
```

- This tells the VS Code plugin that the sensitive files are ruled as follows:
    - Any files with the file extension .env.
    - Any files that include the word `privacy` in the name.

You can see the full options for sensitive-file rules in the configuration section below.

### Example of NON-SENSITIVE file being open in VSCode

![Non-sensitive file open](https://i.gyazo.com/2876ba3a85988b0f7a788d0555eda5ab.png)

- You'll see in this screenshot that we have a file called `protocol.cpp` open
  which doesn't match any of our above sensitive file rules, therefore the file
  is treated as `safe`.

### Example of SENSITIVE file being open in VSCode

![Sensitive file open gif](https://i.gyazo.com/4de1cf627864e8b81f1a85a1b9333cf1.gif)

- You'll see in the screen recording that as we are browsing `protocol.cpp` the
  OBS output remains clear, but as soon as we open `privacy-state.cpp` - VSCode
  immediately reports to OBS that a file matching the above sensitive rules has
  been opened. OBS immediately reacts covering the output with the sensitive file
  bundled image.

### Example of OBS losing connection to VSCode

![Losing connection to VSCode](https://i.gyazo.com/166274ba8b9624d16e87c00b1d124cc2.gif)

- You'll see that as soon as we exit VS Code, OBS immediately reacts and applies
  the connection-loss bundled image over the output.
- This example simply simulates what would happen if VSCode were to stop communicating
  with OBS, in cases where the VSCode extension encounters a bug or hitches. The fallback
  is to heavily reduce the risk of OBS showing a sensitive file.

## 💾 2. Installation

Privacy Guard is currently only supported for Windows. Both components must be installed:
the VSCode extension - detects sensitive files. Also, the OBS plugin - enforces
the protected output. Installing only one component will not provide desired functionality.

### Requirements

For installation:

- Windows 10 or Windows 11.
- Visual Studio Code 1.96 or newer.
- OBS Studio.

For manual building & installation:

- Node.js 20 or newer when building the VS Code extension from source.
- Visual Studio 2022 Build Tools with **Desktop development with C++**, CMake
  3.28 or newer, and a Windows SDK when building the OBS plugin from source.
- Inno Setup 6 when building the OBS plugin installer from source.

Automated GitHub release packaging is not implemented yet, so the current
version is built and packaged from source.

### Install the VS Code extension

From PowerShell in the repository root:

```powershell
cd vscode-extension
npm install
npm test
npm run package
```

This produces:

```text
vscode-extension/obs-vscode-privacy-guard-0.0.1.vsix
```

Install the resulting package using either method:

- In VS Code, open **Extensions**, select the `...` menu, choose **Install from
  VSIX...**, and select the generated file.
- From PowerShell, run:

```powershell
code --install-extension .\obs-vscode-privacy-guard-0.0.1.vsix
```

Reload VS Code after installing the extension.

### Install the OBS plugin

Return to the repository root, then build the native plugin:

```powershell
cd obs-plugin
.\scripts\build-windows.ps1
```

The build bootstrap downloads and verifies the required OBS and Qt development
dependencies. The compiled plugin is written to:

```text
obs-plugin/build_x64/RelWithDebInfo/obs-vscode-privacy-guard.dll
```

Build the Windows installer:

```powershell
.\scripts\build-installer-windows.ps1
```

This produces:

```text
obs-plugin/dist/obs-vscode-privacy-guard-setup-0.0.1.exe
```

Close OBS completely and run the installer. It requests administrator access,
validates the selected OBS installation directory, and installs the plugin to:

```text
C:\Program Files\obs-studio\obs-plugins\64bit\obs-vscode-privacy-guard.dll
```

The two default privacy images are compiled into the DLL as Qt resources, so no
separate image files need to be installed. The installer also registers a
Windows uninstaller and prevents installation or removal while OBS is open.

For rapid local installation, the existing `scripts/install-windows.ps1` script
can still copy a compiled DLL directly after OBS has been closed.

### Confirm the connection

With OBS and VS Code open:

- The VS Code status bar should show **OBS: safe** when no visible file matches.
- Opening a matching file should change the status to **OBS: hidden**.
- OBS should show the configured privacy output while the status is hidden.

Do not stream with the plugin until sensitive-file and connection-loss tests
have both succeeded in your actual OBS scene collection.

## ⚙️ 3. VS Code Extension Configuration

Open VS Code settings and search for **OBS Privacy Guard**, or edit your
`settings.json` directly.

### Enable reporting

```json
{
	"obsPrivacyGuard.enabled": true
}
```

The default is `true`. Setting it to `false` does not remotely disarm the OBS
plugin; the extension reports a protected state so that VS Code cannot weaken
the OBS-side safety layer.

### Configure filename rules

Rules are matched against the file basename, not its full path or contents.
They are case-insensitive unless `caseSensitive` is explicitly set to `true`.

| Rule type    | Behaviour                             | Rule example        | Matching filename           |
| ------------ | ------------------------------------- | ------------------- | --------------------------- |
| `extension`  | Matches a filename or extension       | `.env`              | `production.env`            |
| `startsWith` | Matches the beginning of the basename | `secret_`           | `secret_credentials.json`   |
| `endsWith`   | Matches the end of the basename       | `_credentials.json` | `database_credentials.json` |
| `includes`   | Matches text anywhere in the basename | `config`            | `database_config.json`      |

Example configuration:

```json
{
	"obsPrivacyGuard.rules": [
		{
			"id": "dotenv",
			"type": "extension",
			"value": ".env"
		},
		{
			"id": "secret-prefix",
			"type": "startsWith",
			"value": "secret_"
		},
		{
			"id": "credentials-suffix",
			"type": "endsWith",
			"value": "_credentials.json"
		},
		{
			"id": "config",
			"type": "includes",
			"value": "config",
			"caseSensitive": true
		}
	]
}
```

`caseSensitive` is optional and defaults to `false`.

Each rule requires:

- `id`: a non-sensitive identifier used for diagnostics.
- `type`: one of the supported rule types.
- `value`: the basename text to match.

`caseSensitive` is optional and defaults to `false`.

Only visible text editors are checked. A background tab that is open but not
visible does not trigger protection. Every editor visible in a split layout is
checked, so any matching split protects the OBS output.

## ⚙️ 4. OBS Plugin Configuration

Open OBS and select **Tools → VS Code Privacy Guard**.

The settings are stored per OBS scene collection, allowing different scene
collections to use different privacy outputs.

### Sensitive-file output

Choose what OBS displays when VS Code reports a matching visible file:

- **Built-in Privacy Guard image (recommended):** draws the bundled sensitive-
  file image after the complete OBS output.
- **Custom image (post-render protected):** draws a user-selected image using
  the same protected render path.
- **Custom OBS scene (user managed):** switches to a selected scene created and
  maintained by the user.

### VSCode unavailable output

Choose what OBS displays while waiting for VS Code or after communication is
lost:

- **Built-in Privacy Guard image (recommended).**
- **Custom image (post-render protected).**
- **Custom OBS scene (user managed).**

Custom images are selected independently for the two events and are fitted to
the OBS canvas while preserving their aspect ratio. The plugin always draws an
opaque black background first, so transparent areas cannot expose the scene.
While a custom image is active, Privacy Guard draws its bundled watermark over
the complete output. An invalid or missing custom image
falls back to the corresponding bundled image; an unexpected texture failure leaves
the black background in place.

Custom scenes require the risk-acknowledgement checkbox. Privacy Guard cannot
validate the contents of a user-created scene or guarantee that its configured
transition does not briefly expose captured content. While a custom scene is
active, the watermark is drawn after that scene has rendered.

RE: Watermarks - I really don't care if you remove the watermark in your build
but credit to the repo where possible would be appreciated.

When custom-scene protection activates, the plugin remembers the previously
active scene. After VS Code reports `SAFE`, that scene is restored. Image-based
protection does not change the selected scene. If a configured custom scene is
missing or removed, the plugin keeps the opaque black fallback active.

Recommended test procedure:

1. Start OBS and VSCode with a normal file visible.
2. Confirm that OBS shows the intended normal scene.
3. Open a file matching each configured VSCode rule.
4. Confirm that the complete OBS output changes to the selected privacy output.
5. Close or hide the matching file and confirm that the previous scene returns.
6. Close VSCode and confirm that OBS immediately uses the connection-failure
   output.
7. Repeat the test while recording locally and inspect the recording for unsafe
   transition frames.

In the following screenshot you'll see the default settings used for the bundled
images for sensitive files and connection lost:

![OBS Plugin settings defaults](https://i.gyazo.com/8bc6951320f9088d335c13e9b3292d17.png)

This screenshot then shows the option of using a custom image instead of the
bundled images for sensitive files and connection lost:

![OBS Plugin settings custom image](https://i.gyazo.com/53aaa4d20f02347045855d7608192728.png)

Lastly, this shows the plugin configured to use a custom scene instead of either
image-overlay option:

![OBS Plugin settings custom scene](https://i.gyazo.com/fbd1fb4365733c431e78d69cdac4cacf.png)

## 💻 5. Technical Bits

### Repository layout

```text
obs-vscode-privacy-guard/
├── vscode-extension/    TypeScript VS Code extension
├── obs-plugin/          C++20 native OBS plugin
├── docs/                Architecture and protocol documentation
├── .clang-format        C++ and TypeScript formatting rules
├── .editorconfig        IDE format config
└── README.md            <--- YOU ARE HERE
```

### Architecture

The VS Code extension observes `window.visibleTextEditors`, validates the
configured rules, and evaluates only editor basenames. It never reads file
contents for matching and never transmits filenames, paths, workspace names, or
file contents to OBS.

Each VS Code extension-host lifetime creates a random client UUID and connects
to the local Windows named pipe:

```text
\\.\pipe\obs-vscode-privacy-guard-v1
```

The native OBS plugin owns the authoritative privacy state. Multiple VS Code
windows are tracked independently; any client reporting `SENSITIVE` protects
the output. A newly connected client remains unsafe until its handshake and
first explicit state are accepted.

### Protocol

Privacy Guard uses a versioned UTF-8, newline-delimited protocol:

```text
PG/1 HELLO <client-id>
PG/1 STATE <sequence> SAFE
PG/1 STATE <sequence> SENSITIVE
PG/1 GOODBYE
```

Sequence numbers must increase monotonically. Replayed, reordered, malformed,
unknown, or oversized frames are rejected and fail closed.

The VS Code extension republishes its explicit state every 500 milliseconds.
OBS treats a client as disconnected when no valid state update arrives for two
seconds. Periodic updates from an existing connection cannot clear a different
client's failure; a newly connected client's initial `STATE` is required.

See [docs/protocol.md](docs/protocol.md) for the complete wire contract.

### Privacy state machine

| State                | Meaning                                          | Default output                   |
| -------------------- | ------------------------------------------------ | -------------------------------- |
| `Disabled`           | OBS-side protection explicitly disabled          | Normal output                    |
| `AwaitingConnection` | No complete client state is available            | Connection-loss image over black |
| `Safe`               | Every connected client explicitly reports safe   | Normal output                    |
| `Sensitive`          | At least one client reports sensitive            | Sensitive-file image over black  |
| `Disconnected`       | A client failed, timed out, or sent invalid data | Connection-loss image over black |

State is guarded by a mutex because named-pipe workers and the OBS tick thread
access it concurrently. Each connection tracks its handshake, last sequence,
latest explicit state, and last-update timestamp.

### OBS rendering and scenes

The native module registers a post-main-render callback. It first draws an
opaque solid layer over the base OBS canvas after the current scene has rendered,
then composites the selected bundled or custom image over that layer. This
protects the entire program output rather than attempting to obscure only the
VS Code source. Custom images and custom scenes receive a transparent bundled
watermark as the final post-render layer.

Custom-scene operations are queued onto the OBS UI thread. Generation numbers
prevent stale queued state changes from overriding newer privacy decisions.
Scene deletion and scene-collection changes invalidate cached scene state and
restore the black fallback when required.

### Technology

- VS Code extension: TypeScript, Node.js named pipes, VS Code Extension API.
- OBS plugin: C++20, OBS frontend API, `libobs`, Qt 6, Windows named pipes.
- Build systems: npm/TypeScript and CMake/MSBuild.
- Tests: Node's built-in test runner plus native parser and state-machine tests.

See [docs/architecture.md](docs/architecture.md) for the detailed security model
and current implementation boundaries.

### 📄 Project policies

- [MIT licence](LICENSE)
- [Privacy Policy](PRIVACY.md)
- [Code Signing Policy](CODE_SIGNING_POLICY.md)
