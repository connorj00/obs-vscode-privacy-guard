# Local protocol version 1

Privacy Guard uses UTF-8, newline-delimited frames over the Windows named pipe
`\\.\pipe\obs-vscode-privacy-guard-v1`.

The protocol deliberately excludes paths, filenames, workspace names, and file
contents.

## Client frames

```text
PG/1 HELLO <client-id>
PG/1 STATE <sequence> SAFE
PG/1 STATE <sequence> SENSITIVE
PG/1 GOODBYE
```

- `client-id` is a random UUID for one VS Code extension-host lifetime.
- `sequence` is a monotonically increasing non-negative integer.
- The client sends `HELLO`, immediately followed by its latest `STATE`.
- The client republishes its latest explicit `STATE` every 500 ms. These frames
  provide liveness and allow fail-closed re-arming without a separate heartbeat.
- Frames larger than 1 KiB or frames that do not match the grammar are invalid.

## OBS behavior

- A connection is not safe until a valid `HELLO` and `STATE` are received.
- Any client reporting `SENSITIVE` makes the aggregate state sensitive.
- An expected `GOODBYE` removes that client from the aggregate.
- An unexpected disconnect or state update older than 2 seconds changes the
  aggregate state to disconnected and hides the output.
- Receiving a lower sequence number, unknown command, or malformed frame fails
  closed.
- After an unexpected disconnect, periodic updates from an existing connection
  cannot clear the failure. A newly connected client must send its initial
  explicit `STATE` frame.
