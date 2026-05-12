# Spec: Agent Monitor Real Integration

## Tauri IPC Commands

### `get_agents` → `AgentInfo[]`

```
AgentInfo {
  name:           string   // "{ToolType} · {cwd_basename}", e.g. "Claude Code · api"
  tool_type:      string   // "Claude Code" | "Codex" | "OpenCode"
  pid:            number
  status:         "running" | "exited"
  cwd:            string   // absolute path
  window_address: string | null  // Hyprland client address, null if not found
}
```

**Process detection rules (hard constraints):**
- Claude Code: `exe/comm basename == "claude"`. Node fallback only if argv contains `@anthropic-ai/claude-code`.
- Codex: `exe/comm basename == "codex"` AND NOT (`class == "Codex"` OR argv contains `app-server`). Node launcher + native child → merge into one entry (use native child PID, node launcher as parent reference).
- OpenCode: `exe/comm basename == "opencode"`. Bun/node fallback if argv contains `opencode-ai`.

**Window mapping (tmux-aware):**
1. Direct: find `hyprctl_client.pid` that is ancestor of `agent.pid` via `/proc/<pid>/status` PPID walk.
2. tmux fallback: `agent.pid` → walk PPID → find tmux server PID → `tmux list-panes -a` to find pane_pid → `tmux list-clients` to find terminal client PID → match to `hyprctl_client.pid`.
3. If neither found: `window_address = null`.

**Polling:** Backend polls every 2000ms. Emits `agents-updated` Tauri event only when snapshot hash changes.

---

### `focus_window` (address: string) → Result<(), String>

- Runs: `hyprctl dispatch focuswindow address:<address>`
- Validates address is non-empty before running.
- Returns error string if hyprctl not found or exits non-zero.
- If `HYPRLAND_INSTANCE_SIGNATURE` env var absent → return `Err("Hyprland not available")`.

---

## Frontend Behavior

### Polling
- Frontend calls `invoke('get_agents')` every 2000ms.
- Hash: `JSON.stringify(agents)` string comparison. Skip `render()` if identical.

### UI State Mapping
- `status == "running"` → render as "working" style (pulsing indicator, chip `working`)
- `status == "exited"` → render as "done" style (static, dimmed)
- `window_address == null` → disable jump button, show tooltip "no window"

### Empty State
- `agents.length === 0` → headline: `"No active agents"`, sub: `"Pulse is listening…"`
- Show as `idle` layout (no body panel, minimal width)

### Agent Name Display
- `"{tool_type} · {cwd_basename}"` where `cwd_basename` is last path segment of `cwd`

### Jump Action
- Click agent row → `invoke('focus_window', { address: agent.window_address })`
- If `window_address == null`: show toast `"No window found for this agent"`
- Keyboard: `Super+P` registered via `@tauri-apps/plugin-global-shortcut` → toggle Pulse window show/hide

### Icon Keys (must match AGENT_ICON map exactly)
- `"Claude Code"` → peach, `'C'`
- `"Codex"` → sky, `'X'`
- `"OpenCode"` → coral, `'O'`

---

## Capabilities

File: `src-tauri/capabilities/default.json`

Required additions:
```json
"permissions": [
  "core:default",
  "global-shortcut:allow-register",
  "global-shortcut:allow-unregister",
  "global-shortcut:allow-is-registered"
]
```

Custom commands `get_agents` and `focus_window` are allowed via `invoke_handler` registration (no separate capability entry needed for custom commands in Tauri 2 core).

---

## PBT Properties

| Invariant | Falsification Strategy |
|-----------|----------------------|
| `get_agents` never returns duplicate PIDs | Generate agent lists with repeated PIDs; assert `Set(pids).size == list.length` |
| `focus_window` with null/empty address never calls hyprctl | Inject null address; assert hyprctl was not invoked |
| Polling hash comparison is idempotent | Call `get_agents` twice with no state change; assert render called exactly once |
| Agent name always contains ` · ` separator | Generate agents with empty cwd; assert name still has separator (fallback to `"?"`) |
| Window resize never goes below minimum width (280px) | Set agents=[] and render; assert `LogicalSize.width >= 280` |
| Exited agent disappears within 2 poll cycles (≤4s) | Kill agent process; assert entry removed within 2 polls |
