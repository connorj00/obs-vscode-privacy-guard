# OBS VS Code Privacy Guard

This is the VS Code half of OBS VS Code Privacy Guard. It watches all visible
text editors and reports either `SAFE` or `SENSITIVE` to the paired native OBS
plugin over a local Windows named pipe.

The extension never sends filenames, paths, workspace names, or file contents.
It also cannot remotely disable OBS protection.

## Rule configuration

Configure `obsPrivacyGuard.rules` in VS Code settings. Rules apply to the file
basename and are case-insensitive by default.

```json
{
  "obsPrivacyGuard.rules": [
    { "id": "dotenv", "type": "extension", "value": ".env" },
    { "id": "server-prefix", "type": "startsWith", "value": "sv_" },
    { "id": "server-suffix", "type": "endsWith", "value": "_sv.lua" },
    { "id": "config", "type": "includes", "value": "config" }
  ]
}
```

Open but hidden/background tabs do not trigger protection. Every editor visible
in a split layout is checked.

