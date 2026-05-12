# Design: Agent Permission Inline Actions

## Architecture

```
Claude Code process
  → PermissionRequest hook fires
    → ~/.claude/pulse/permission-hook.sh (PPID walk → sessionId → write pending JSON)
    → polls for response (100ms × 300 = 30s)
    → echo decision → exit 0   (OR timeout → exit 1 → Claude asks in terminal)

Pulse polling thread (every 2s)
  → agents::scan() → claude_metadata() → check_pending_permissions()
    reads ~/.claude/pulse/pending/*.json
    matches sessionId to AgentInfo.session_id
    sets agent.pending_permission
  → hash change → emit "agents-updated"

Frontend
  → buildAgentRow() detects pending_permission → buildPermissionRow()
  → user clicks → invoke('respond_permission') → resizeWindow()
```

## Rust Changes

### state.rs — PermissionRequest struct (NEEDS FIX)

Current gemini implementation has `description: String, input: String`.
Must be:
```rust
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct PermissionRequest {
    pub session_id: String,
    pub tool_name: String,
    #[serde(default)]
    pub description: Option<String>,
    pub input: serde_json::Value,  // NOT String — hook writes JSON object
    #[serde(default)]
    pub created_at_ms: Option<u64>,
    #[serde(default)]
    pub expires_at_ms: Option<u64>,
}
```

### agents.rs — check_pending_permissions

Already implemented by gemini. Add stale file cleanup (created_at_ms > 45s → delete).

### main.rs — respond_permission (implemented, needs validation)

Add sessionId format validation `^[A-Za-z0-9._-]+$` before path construction.

### main.rs — startup setup

Add to `.setup()` block:
```rust
install_permission_bridge();  // idempotent: create dirs, write hook script, inject settings.json
clean_stale_permissions();    // delete pending/response files older than 45s
```

## Hook Script (`~/.claude/pulse/permission-hook.sh`)

See codex analysis for full script. Key points:
- Uses PPID walk (≤8 levels) to find sessionId
- Auto-approve check via `grep -Fxq`  
- Pending file written via tmp+rename
- 300 × 0.1s polling loop
- Validates sessionId against `^[A-Za-z0-9._-]+$`

## settings.json Injection (Rust)

```rust
fn inject_permission_hook(hook_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    let settings_path = format!("{}/.claude/settings.json", std::env::var("HOME")?);
    let mut root: serde_json::Value = if std::path::Path::new(&settings_path).exists() {
        serde_json::from_str(&std::fs::read_to_string(&settings_path)?)?
    } else {
        serde_json::json!({})
    };
    
    let hooks = root["hooks"].as_object_mut()
        .ok_or("hooks not object")?;  // or insert if missing
    let perm_hooks = hooks.entry("PermissionRequest")
        .or_insert(serde_json::json!([]));
    let arr = perm_hooks.as_array_mut().ok_or("not array")?;
    
    // Idempotent: check if our command already exists
    let already_present = arr.iter().any(|entry| {
        entry["hooks"].as_array()
            .and_then(|h| h.iter().find(|c| c["command"].as_str() == Some(hook_path)))
            .is_some()
    });
    
    if !already_present {
        arr.push(serde_json::json!({
            "hooks": [{"type": "command", "command": hook_path}]
        }));
        // Write via tmp + rename
        let tmp = format!("{}.tmp", settings_path);
        std::fs::write(&tmp, serde_json::to_string_pretty(&root)?)?;
        std::fs::rename(&tmp, settings_path)?;
    }
    Ok(())
}
```

## Frontend Changes

### buildPermissionRow() (partially implemented, needs fixes)

Issues with current gemini implementation:
1. Uses `prompt()` (browser dialog) for Deny+Reason → replace with inline `<input>` + confirm button
2. `displayInput` tries `JSON.parse(req.input)` but input is already an object → use directly

Fix:
```js
function buildPermissionRow(agent) {
  const req = agent.pending_permission;
  const title = agent.session_name ? `${agent.name} · ${agent.session_name}` : agent.name;
  
  // input is already an object (Value from Rust)
  const displayInput = req.input?.command || req.input?.path || 
                       req.input?.explanation || JSON.stringify(req.input, null, 2);
  
  let denyInputEl = null;
  
  const row = h('div', { className: 'perm-row' },
    h('div', { className: 'perm-head' },
      h('span', { className: 'warn-icon' }, '!'),
      h('span', { style: { fontWeight: 600 } }, title),
      h('span', { className: 'perm-tool mono' }, req.tool_name),
    ),
    req.description ? h('div', { className: 'perm-desc' }, req.description) : null,
    h('pre', { className: 'perm-input' }, displayInput),
    h('div', { className: 'actions' },
      h('button', { className: 'btn primary',  onClick: () => respondPermission(req.session_id, 'approve') }, 'Approve'),
      h('button', { className: 'btn approve-all', onClick: () => respondPermission(req.session_id, 'approve', true) }, 'All'),
      h('button', { className: 'btn danger',   onClick: () => respondPermission(req.session_id, 'block') }, 'Deny'),
      h('button', { className: 'btn',          onClick: () => toggleDenyInput(row) }, 'Deny+'),
    ),
  );
  return row;
}
```

### CSS Additions

```css
.perm-row { ... }
.perm-head { display: flex; align-items: center; gap: 8px; margin-bottom: 6px; }
.perm-tool { font-size: 10px; color: var(--text-3); margin-left: auto; }
.perm-desc { font-size: 11px; color: var(--text-2); margin-bottom: 6px; }
.perm-input { font-size: 10px; font-family: 'JetBrains Mono'; background: var(--bg-0);
              padding: 6px 8px; border-radius: 4px; margin-bottom: 8px; 
              max-height: 80px; overflow-y: auto; white-space: pre-wrap; word-break: break-all; }
.btn.approve-all { color: var(--peach); border-color: color-mix(in oklch, var(--peach) 40%, var(--border)); }
.perm-deny-input { width: 100%; margin-top: 6px; background: var(--bg-3); 
                   border: 1px solid var(--border); border-radius: 6px; 
                   padding: 4px 8px; color: var(--text-1); font-size: 12px; }
```
