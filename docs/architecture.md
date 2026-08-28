# Architecture

## Components

### VS Code extension

The UI-side TypeScript extension observes `window.visibleTextEditors`. It
matches only the basename of each visible file against configured rules. Tabs
which are open but not visible do not affect the privacy state.

Every VS Code window is an independent client. The OBS pipe server aggregates
clients so that any `SENSITIVE` client hides the output. A newly connected
client also hides the output until its handshake and first explicit state are
accepted.

### OBS plugin

The native OBS module owns the final decision. It registers a post-main-render
callback and draws an opaque black layer across the complete OBS canvas whenever
the state is anything other than healthy and safe. It then composites the
selected bundled or custom privacy image over that layer.

The intended states are:

| State                | Output                                     |
| -------------------- | ------------------------------------------ |
| `Disabled`           | Normal output                              |
| `AwaitingConnection` | Bundled/custom connection image over black |
| `Safe`               | Normal output                              |
| `Sensitive`          | Bundled/custom sensitive image over black  |
| `Disconnected`       | Bundled/custom connection image over black |

Each scene collection independently selects a bundled image, custom image, or
user-managed scene for `Sensitive` and connection-unavailable states. Bundled
images are compiled into the plugin. Custom images retain the post-render black
base and fall back to the event-specific bundled image when unavailable.
Custom scenes require an explicit risk acknowledgment. Scene contents and OBS
transitions are user controlled; if a configured scene cannot be activated,
the bundled image and opaque black layer remain in place. When Privacy Guard
switches to a custom scene, returning to `Safe` restores the preceding scene.
Image-based protection never changes or restores the user's selected scene.

Successful custom-image and custom-scene outputs receive a bundled transparent
watermark as the final post-main-render layer. The watermark is active for both
sensitive-file and connection-unavailable protection, but is omitted from the
bundled default images and every non-protected state. A missing watermark does
not weaken or disable the underlying privacy output.

The underlying fallback is deliberately opaque. Bundled and custom images are
always composited over black so transparency or texture-loading failure cannot
expose the scene underneath.

## Security properties

- The pipe is local to Windows and never opens a network port.
- Filenames and paths are never sent to OBS.
- OBS does not infer that silence means safe.
- A newly connected client remains unsafe until it sends an explicit state.
- State-refresh expiry and malformed protocol input fail closed.
- Protection can only be disabled explicitly in OBS; VS Code cannot remotely
  disarm it.

## Current scaffold boundary

Implemented:

- Versioned protocol contract.
- VS Code settings and rule validation.
- Visible-editor matching.
- Reconnecting named-pipe client with periodic explicit state refreshes.
- VS Code status-bar state.
- Native privacy state machine.
- Strict native protocol parser.
- Local-only Windows named-pipe server.
- Multi-window state aggregation and sequence replay rejection.
- Disconnect latching and explicit-state recovery.
- Native full-canvas opaque post-render callback.
- Event-specific bundled privacy images compiled into the plugin DLL.
- Post-render watermark for successful custom images and custom scenes.
- Per-scene-collection bundled image, custom image, and custom scene settings.
- OBS Tools dialog with image pickers, scene selectors, and risk acknowledgment.
- Previous-scene restoration and layered bundled/black fallbacks.
- Windows installer with embedded plugin resources.
- Matcher and wire-frame unit tests.
- Native parser and state-machine tests.

Still to implement:

- Live OBS/VS Code named-pipe lifecycle test.
- CI release packaging.
