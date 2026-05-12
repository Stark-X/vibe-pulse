# Tasks: Agent Monitor Real Integration

## Phase 1: Rust Backend

- [x] 1.1 Update Cargo.toml — add sysinfo 0.32 and tauri-plugin-global-shortcut 2
- [x] 1.2 Create src-tauri/src/state.rs — AgentInfo, AgentStatus, HyprClient structs with serde derives
- [x] 1.3 Create src-tauri/src/hyprland.rs — get_hypr_clients() and focus_window() using std::process::Command
- [x] 1.4 Create src-tauri/src/tmux.rs — ppid_of(), find_tmux_pane_client() for /proc PPID walking
- [x] 1.5 Create src-tauri/src/agents.rs — scan_agents() using sysinfo with claude/codex/opencode detection rules
- [x] 1.6 Update src-tauri/src/main.rs — register get_agents + focus_window commands, start 2s polling thread emitting agents-updated event
- [x] 1.7 Update src-tauri/capabilities/default.json — add global-shortcut permissions

## Phase 2: Frontend

- [x] 2.1 Install @tauri-apps/plugin-global-shortcut via bun add
- [x] 2.2 Replace AGENTS mock in src/main.js — replace with empty st.agents array and startPolling() using invoke('get_agents') every 2000ms with JSON hash guard
- [x] 2.3 Update render() in src/main.js — map st.agents to agent rows using tool_type for AGENT_ICON lookup, show "No active agents" empty state when agents.length === 0
- [x] 2.4 Update status indicator rendering — map status='running' to 'working' CSS class, status='exited' to 'done' CSS class
- [x] 2.5 Update jump action — replace handleAction('jump:id') with invoke('focus_window', {address}) and show toast when window_address is null
- [x] 2.6 Register Super+P global shortcut in init() to toggle window visibility

## Phase 3: Verification

- [ ] 3.1 Launch claude — verify agent appears within 4s with correct name format "Claude Code · {dir}"
- [ ] 3.2 Launch codex CLI — verify one agent card (not desktop app), no duplicate node+native entries
- [ ] 3.3 Launch opencode — verify agent appears correctly
- [ ] 3.4 Click agent row — verify Hyprland focus switches to correct terminal window
- [ ] 3.5 tmux scenario — run agent inside tmux, verify jump still works
- [ ] 3.6 Kill agent process — verify card disappears within 4s
- [ ] 3.7 Press Super+P — verify Pulse window toggles show/hide
- [ ] 3.8 Start without Hyprland — verify agents list shows but jump toasts error
