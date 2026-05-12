# Design: Agent Monitor Real Integration

## Architecture

```
Frontend (JS)                     Tauri Runtime              System
──────────────────────────────────────────────────────────────────
setInterval 2000ms
  → invoke('get_agents')    →    agents.rs::scan_agents()
                                   sysinfo::System::new()
                                   filter by exe basename
                                   ↓
                                 hyprland.rs::get_clients()
                                   hyprctl clients -j
                                   ↓
                                 tmux.rs::find_window()  →  tmux list-panes
                                   ppid_walk()           →  /proc/<pid>/status
                                                         →  tmux list-clients
                                 return AgentInfo[]
  ← JSON response
  hash compare → render()

click jump    →    invoke('focus_window', {address})
                    hyprland.rs::focus_window()
                    hyprctl dispatch focuswindow address:X
```

## Rust Module Structure

```
src-tauri/src/
├── main.rs          # Tauri setup, command registration, polling thread
├── state.rs         # AgentInfo, HyprClient structs, AppState
├── agents.rs        # sysinfo process scanning + detection rules
├── hyprland.rs      # hyprctl wrapper (clients JSON, focus dispatch)
└── tmux.rs          # tmux list-panes, list-clients, ppid walk
```

## Data Structures

```rust
// state.rs
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AgentInfo {
    pub name: String,            // "{ToolType} · {cwd_basename}"
    pub tool_type: String,       // "Claude Code" | "Codex" | "OpenCode"
    pub pid: u32,
    pub status: AgentStatus,
    pub cwd: String,
    pub window_address: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum AgentStatus { Running, Exited }

#[derive(Debug, Clone, Deserialize)]
pub struct HyprClient {
    pub address: String,
    pub pid: i64,
    pub class: String,
    pub title: String,
    pub workspace: HyprWorkspace,
}
```

## Key Implementation Details

### Process Scanner (`agents.rs`)

```rust
use sysinfo::{ProcessesToUpdate, System};

pub fn scan_agents(sys: &mut System) -> Vec<AgentInfo> {
    sys.refresh_processes(ProcessesToUpdate::All, true);
    let mut found = vec![];
    for (pid, proc) in sys.processes() {
        let exe_name = proc.exe()
            .and_then(|p| p.file_name())
            .and_then(|n| n.to_str())
            .unwrap_or("");
        let tool_type = match exe_name {
            "claude" => Some("Claude Code"),
            "codex"  => {
                // exclude desktop app-server
                let args = proc.cmd().join(" ");
                if args.contains("app-server") { None } else { Some("Codex") }
            },
            "opencode" => Some("OpenCode"),
            _ => None,
        };
        if let Some(tt) = tool_type {
            let cwd = proc.cwd().map(|p| p.display().to_string())
                .unwrap_or_default();
            let cwd_base = std::path::Path::new(&cwd)
                .file_name().and_then(|n| n.to_str())
                .unwrap_or("?").to_string();
            found.push(AgentInfo {
                name: format!("{} · {}", tt, cwd_base),
                tool_type: tt.to_string(),
                pid: pid.as_u32(),
                status: AgentStatus::Running,
                cwd,
                window_address: None, // filled later
            });
        }
    }
    found
}
```

### PPID Walk (`tmux.rs`)

```rust
pub fn ppid_of(pid: u32) -> Option<u32> {
    let status = std::fs::read_to_string(format!("/proc/{}/status", pid)).ok()?;
    for line in status.lines() {
        if line.starts_with("PPid:") {
            return line.split_whitespace().nth(1)?.parse().ok();
        }
    }
    None
}

pub fn find_tmux_pane_client(agent_pid: u32, known_pids: &[u32]) -> Option<u32> {
    // Walk up PPID chain; if we find a tmux-server, use tmux IPC
    let mut cur = agent_pid;
    for _ in 0..16 {
        cur = ppid_of(cur)?;
        if known_pids.contains(&cur) { return Some(cur); }
    }
    None
}
```

### Hyprland Window Map (`hyprland.rs`)

```rust
pub fn get_hypr_clients() -> Vec<HyprClient> {
    let out = std::process::Command::new("hyprctl")
        .args(["clients", "-j"]).output().ok()?;
    serde_json::from_slice(&out.stdout).unwrap_or_default()
}

pub fn find_window_for_pid(agent_pid: u32, clients: &[HyprClient]) -> Option<String> {
    // Direct match
    if let Some(c) = clients.iter().find(|c| c.pid == agent_pid as i64) {
        return Some(c.address.clone());
    }
    // PPID chain match
    let client_pids: Vec<u32> = clients.iter().map(|c| c.pid as u32).collect();
    let mut cur = agent_pid;
    for _ in 0..32 {
        cur = ppid_of(cur)?;
        if let Some(c) = clients.iter().find(|c| c.pid == cur as i64) {
            return Some(c.address.clone());
        }
    }
    None
}

pub fn focus_window(address: &str) -> Result<(), String> {
    if std::env::var("HYPRLAND_INSTANCE_SIGNATURE").is_err() {
        return Err("Hyprland not available".into());
    }
    let out = std::process::Command::new("hyprctl")
        .args(["dispatch", "focuswindow", &format!("address:{}", address)])
        .output().map_err(|e| e.to_string())?;
    if out.status.success() { Ok(()) } else {
        Err(String::from_utf8_lossy(&out.stderr).into_owned())
    }
}
```

### Polling Thread (`main.rs`)

```rust
tauri::Builder::default()
    .setup(|app| {
        let handle = app.handle().clone();
        std::thread::spawn(move || {
            let mut sys = sysinfo::System::new();
            let mut last_hash = String::new();
            loop {
                std::thread::sleep(std::time::Duration::from_millis(2000));
                let clients = hyprland::get_hypr_clients();
                let mut agents = agents::scan_agents(&mut sys);
                for a in &mut agents {
                    a.window_address = hyprland::find_window_for_pid(a.pid, &clients);
                }
                let hash = format!("{:?}", agents);
                if hash != last_hash {
                    last_hash = hash;
                    let _ = handle.emit("agents-updated", &agents);
                }
            }
        });
        Ok(())
    })
    .invoke_handler(tauri::generate_handler![get_agents, focus_window_cmd])
    .run(tauri::generate_context!())
```

## Frontend Changes (`src/main.js`)

### Replace mock AGENTS with real state
```js
// Remove: const AGENTS = { ... }
// Replace st with:
const st = {
  agents: [],          // AgentInfo[]
  lastHash: '',
  isHyprland: true,
  theme: 'midnight',
  shape: 'round',
  position: 'top',
  settingsOpen: false,
  clock: '',
};

// Add polling:
async function startPolling() {
  const { invoke } = await import('@tauri-apps/api/core');
  setInterval(async () => {
    try {
      const agents = await invoke('get_agents');
      const hash = JSON.stringify(agents);
      if (hash !== st.lastHash) {
        st.lastHash = hash;
        st.agents = agents;
        render();
      }
    } catch (_) { /* not in Tauri context */ }
  }, 2000);
}
```

### Status rendering
- `agent.status === 'running'` → CSS class `working` (existing pulsing dot)
- `agent.status === 'exited'` → CSS class `done` (dimmed)

### Jump action
```js
async function jumpToAgent(agent) {
  if (!agent.window_address) {
    toast('No window found for this agent');
    return;
  }
  const { invoke } = await import('@tauri-apps/api/core');
  await invoke('focus_window', { address: agent.window_address });
}
```

### Global shortcut registration
```js
async function registerShortcuts() {
  try {
    const { register } = await import('@tauri-apps/plugin-global-shortcut');
    const win = await import('@tauri-apps/api/window');
    await register('Super+P', async () => {
      const w = win.getCurrentWindow();
      const visible = await w.isVisible();
      if (visible) { await w.hide(); } else { await w.show(); await w.setFocus(); }
    });
  } catch (_) { /* dev mode */ }
}
```

## Cargo.toml Changes

```toml
[dependencies]
tauri = { version = "2", features = [] }
tauri-plugin-global-shortcut = "2"
serde = { version = "1", features = ["derive"] }
serde_json = "1"
sysinfo = "0.32"
```

## package.json Changes

```json
"dependencies": {
  "@tauri-apps/api": "^2",
  "@tauri-apps/plugin-global-shortcut": "^2"
}
```
