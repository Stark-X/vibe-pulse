# Spec: Agent Permission Inline Actions

## Data Structures

```
PermissionRequest {
  session_id:  String          // matches AgentInfo.session_id
  tool_name:   String          // e.g. "Bash", "Edit", "Write"
  description: Option<String>  // human-readable description (may be null)
  input:       serde_json::Value // raw tool input (object, not String)
  created_at_ms: Option<u64>
  expires_at_ms: Option<u64>
}

AgentInfo (additions):
  session_id:         Option<String>           // persisted from claude_metadata
  pending_permission: Option<PermissionRequest> // null unless hook pending
```

## IPC File Paths

```
~/.claude/pulse/
├── pending/<sessionId>.json   ← hook writes, Pulse reads
├── response/<sessionId>.json  ← Pulse writes, hook reads
└── auto-approve/<sessionId>   ← one tool_name per line (exact match)
```

All paths validated: sessionId must match `^[A-Za-z0-9._-]+$`.

## Hook Script Contract

- Location: `~/.claude/pulse/permission-hook.sh` (chmod 0700)
- Claude Code fires this hook BEFORE asking user interactively
- Stdin: JSON permission request from Claude Code
- Stdout: `{"decision":"approve"}` or `{"decision":"block","feedback":"..."}` or empty
- Exit 0 with decision = hook handled it; Exit 1 = fallback to terminal (30s timeout)
- PPID walk (≤8 levels) to find `~/.claude/sessions/<pid>.json` → get sessionId

## Tauri Commands

### `respond_permission`
```
Args:
  session_id: String
  decision:   String  // "approve" | "block"
  approve_all: Option<bool>
  feedback:   Option<String>

Behavior:
  1. Validate sessionId against ^[A-Za-z0-9._-]+$
  2. Write response JSON to ~/.claude/pulse/response/<sessionId>.json
  3. If approve_all=true: append tool_name to ~/.claude/pulse/auto-approve/<sessionId>
  4. Delete ~/.claude/pulse/pending/<sessionId>.json (immediate UI update)

Returns: Result<(), String>
```

## Hook Registration (settings.json)

```json
{
  "hooks": {
    "PermissionRequest": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "/home/<user>/.claude/pulse/permission-hook.sh"
          }
        ]
      }
    ]
  }
}
```
Injection is idempotent: read existing JSON → merge PermissionRequest array → write via tmp+rename.

## Frontend Behavior

- Normal state: `buildAgentRow()` renders compact row
- Permission pending (`agent.pending_permission != null`): `buildPermissionRow()` renders expanded row:
  - Header: "!" icon + tool_name + agent title
  - Description text
  - Input preview (command/path/explanation, max 3 lines)
  - 4 buttons: **Approve** (green) / **Approve All** (orange) / **Deny** (red) / **Deny+Reason** (gray)
  - Deny+Reason: shows inline text input (NOT browser `prompt()`)
- After click: immediate `invoke('respond_permission')` → row collapses → `resizeWindow()`

## Stale Cleanup (on startup)

Delete pending/response files where `created_at_ms` > 45 000ms ago OR mtime > 45s ago.

## PBT Properties

| Invariant | Falsification |
|-----------|--------------|
| `respond_permission` never writes outside `~/.claude/pulse/response/` | sessionId with `../` → assert file path stays in response dir |
| Pending matches exactly one agent (by session_id) | Two agents same session_id → only first gets pending |
| Auto-approve uses exact line match only | `BashOutput` does not match `Bash` in auto-approve file |
| Stale files cleaned at startup (age > 45s) | Pending file with created_at_ms 60s ago → not shown in UI |
| Idempotent hook install: N installs = 1 PermissionRequest command | Install 3× → settings.json has exactly 1 pulse hook entry |
