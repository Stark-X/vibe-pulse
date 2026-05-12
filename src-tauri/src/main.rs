#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod agents;
mod hyprland;
mod state;
mod tmux;

use state::AgentInfo;
use sysinfo::System;
use std::sync::{Arc, Mutex};
use tauri::Emitter;

#[tauri::command]
fn get_agents(state: tauri::State<Arc<Mutex<Vec<AgentInfo>>>>) -> Vec<AgentInfo> {
    state.lock().unwrap_or_else(|e| e.into_inner()).clone()
}

#[tauri::command]
fn focus_window(address: String) -> Result<(), String> {
    hyprland::focus_window(&address)
}

#[tauri::command]
fn respond_permission(
    session_id: String,
    decision: String,
    approve_all: Option<bool>,
    feedback: Option<String>,
) -> Result<(), String> {
    if !session_id.chars().all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_' || c == '.') {
        return Err("Invalid session_id format".into());
    }
    if !matches!(decision.as_str(), "approve" | "block") {
        return Err("decision must be 'approve' or 'block'".into());
    }
    let Ok(home) = std::env::var("HOME") else { return Err("HOME not set".into()) };

    let base = std::path::Path::new(&home).join(".claude/pulse");
    let resp_dir = base.join("response");
    let pending_dir = base.join("pending");
    let _ = std::fs::create_dir_all(&resp_dir);

    let resp_path = resp_dir.join(format!("{}.json", session_id));
    let pending_path = pending_dir.join(format!("{}.json", session_id));

    let mut resp_obj = serde_json::json!({ "decision": decision });
    if approve_all == Some(true) {
        resp_obj["approve_all"] = serde_json::Value::Bool(true);
    }
    if let Some(f) = feedback.filter(|s| !s.trim().is_empty()) {
        resp_obj["feedback"] = serde_json::Value::String(f);
    }

    let raw = serde_json::to_string(&resp_obj).map_err(|e| e.to_string())?;
    std::fs::write(&resp_path, raw).map_err(|e| e.to_string())?;
    let _ = std::fs::remove_file(pending_path);

    Ok(())
}

fn main() {
    let agent_state: Arc<Mutex<Vec<AgentInfo>>> = Arc::new(Mutex::new(vec![]));

    tauri::Builder::default()
        .plugin(tauri_plugin_global_shortcut::Builder::new().build())
        .manage(agent_state.clone())
        .setup(move |app| {
            let handle = app.handle().clone();
            let state = agent_state.clone();

            // Hyprland window rules (new block syntax, Hyprland 0.45+)
            if hyprland::available() {
                for rule in &[
                    "float = yes, match:class = pulse",
                    "border_size = 0, match:class = pulse",
                    "pin = yes, match:class = pulse",
                    "decorate = false, match:class = pulse",
                ] {
                    let _ = std::process::Command::new("hyprctl")
                        .args(["keyword", "windowrule", rule])
                        .output();
                }
            }

            // Install permission bridge (idempotent)
            if let Err(e) = install_permission_bridge() {
                eprintln!("[pulse] permission bridge install failed: {e}");
            }

            std::thread::spawn(move || {
                let mut last_hash = String::new();
                loop {
                    std::thread::sleep(std::time::Duration::from_millis(2000));
                    let mut sys = System::new();
                    let clients = if hyprland::available() { hyprland::get_clients() } else { vec![] };
                    let mut snapshot = agents::scan(&mut sys);
                    for agent in &mut snapshot {
                        agent.window_address = hyprland::find_window_address(agent.pid, &clients);
                        if agent.window_address.is_none() {
                            if let Some(term_pid) = tmux::find_terminal_for_tmux_pane(agent.pid) {
                                agent.window_address = hyprland::find_window_address(term_pid, &clients);
                            }
                        }
                    }
                    let hash = format!("{:?}", snapshot);
                    if hash != last_hash {
                        last_hash = hash;
                        *state.lock().unwrap_or_else(|e| e.into_inner()) = snapshot.clone();
                        let _ = handle.emit("agents-updated", &snapshot);
                    }
                }
            });

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![get_agents, focus_window, respond_permission])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

fn install_permission_bridge() -> Result<(), Box<dyn std::error::Error>> {
    let home = std::env::var("HOME")?;
    let base = std::path::Path::new(&home).join(".claude/pulse");

    // Create directories (mode 0700)
    for sub in &["pending", "response", "auto-approve"] {
        let dir = base.join(sub);
        std::fs::create_dir_all(&dir)?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            std::fs::set_permissions(&dir, std::fs::Permissions::from_mode(0o700))?;
        }
    }

    // Write hook script
    let hook_path = base.join("permission-hook.sh");
    let hook_content = hook_script(&home);
    let current = std::fs::read_to_string(&hook_path).unwrap_or_default();
    if current != hook_content {
        let tmp = base.join("permission-hook.sh.tmp");
        std::fs::write(&tmp, &hook_content)?;
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            std::fs::set_permissions(&tmp, std::fs::Permissions::from_mode(0o700))?;
        }
        std::fs::rename(&tmp, &hook_path)?;
    }

    // Inject PermissionRequest hook into ~/.claude/settings.json
    inject_settings_hook(&home, hook_path.to_str().unwrap_or(""))?;

    // Clean stale pending/response files (> 45s)
    for sub in &["pending", "response"] {
        clean_stale(&base.join(sub));
    }

    Ok(())
}

fn clean_stale(dir: &std::path::Path) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    let cutoff = std::time::SystemTime::now()
        .checked_sub(std::time::Duration::from_secs(45))
        .unwrap_or(std::time::SystemTime::UNIX_EPOCH);
    for entry in entries.flatten() {
        if let Ok(meta) = entry.metadata() {
            if meta.modified().map(|m| m < cutoff).unwrap_or(false) {
                let _ = std::fs::remove_file(entry.path());
            }
        }
    }
}

fn inject_settings_hook(home: &str, hook_path: &str) -> Result<(), Box<dyn std::error::Error>> {
    let settings_path = format!("{}/.claude/settings.json", home);
    let mut root: serde_json::Value = if std::path::Path::new(&settings_path).exists() {
        let raw = std::fs::read_to_string(&settings_path)?;
        serde_json::from_str(&raw)?
    } else {
        serde_json::json!({})
    };

    if !root.is_object() {
        root = serde_json::json!({});
    }
    let root_obj = root.as_object_mut().ok_or("settings.json root is not an object")?;
    if !root_obj.contains_key("hooks") {
        root_obj.insert("hooks".to_owned(), serde_json::json!({}));
    }
    let hooks_obj = root_obj.get_mut("hooks")
        .and_then(|v| v.as_object_mut())
        .ok_or("hooks is not an object")?;
    if !hooks_obj.contains_key("PermissionRequest") {
        hooks_obj.insert("PermissionRequest".to_owned(), serde_json::json!([]));
    }
    let arr = hooks_obj.get_mut("PermissionRequest")
        .and_then(|v| v.as_array_mut())
        .ok_or("PermissionRequest hooks is not an array")?;

    let already = arr.iter().any(|entry| {
        entry.get("hooks")
            .and_then(|h| h.as_array())
            .map(|h| h.iter().any(|c| c.get("command").and_then(|v| v.as_str()) == Some(hook_path)))
            .unwrap_or(false)
    });

    if !already {
        arr.push(serde_json::json!({
            "hooks": [{"type": "command", "command": hook_path}]
        }));
        let tmp = format!("{}.pulse.tmp", settings_path);
        std::fs::write(&tmp, serde_json::to_string_pretty(&root)?)?;
        std::fs::rename(tmp, &settings_path)?;
    }

    Ok(())
}

fn hook_script(home: &str) -> String {
    format!(r#"#!/usr/bin/env bash
set -u

BASE="{home}/.claude/pulse"
PENDING_DIR="$BASE/pending"
RESPONSE_DIR="$BASE/response"
AUTO_DIR="$BASE/auto-approve"
mkdir -p "$PENDING_DIR" "$RESPONSE_DIR" "$AUTO_DIR"
umask 077

REQ_FILE="$(mktemp "$BASE/request.XXXXXX")" || exit 1
cat > "$REQ_FILE"
trap 'rm -f "$REQ_FILE"' EXIT

parent_of() {{
  local pid="$1"
  [[ -r "/proc/$pid/stat" ]] || return 1
  sed -E 's/^.*\) [A-Z] ([0-9]+) .*$/\1/' "/proc/$pid/stat"
}}

find_session_file() {{
  local pid="$1" depth=0
  while [[ "$pid" =~ ^[0-9]+$ && "$pid" -gt 1 && "$depth" -lt 8 ]]; do
    local file="{home}/.claude/sessions/$pid.json"
    if [[ -f "$file" ]]; then printf '%s\n' "$file"; return 0; fi
    pid="$(parent_of "$pid" 2>/dev/null || true)"
    depth=$((depth + 1))
  done
  return 1
}}

SESSION_FILE="$(find_session_file "$PPID" || true)"
[[ -n "$SESSION_FILE" ]] || exit 1

SESSION_ID="$(python3 -c "
import json, sys
try:
    with open('$SESSION_FILE') as f:
        print(json.load(f).get('sessionId') or '')
except: print('')
" 2>/dev/null)"

[[ "$SESSION_ID" =~ ^[A-Za-z0-9._-]+$ ]] || exit 1

TOOL_NAME="$(python3 -c "
import json, sys
try:
    with open('$REQ_FILE') as f:
        d = json.load(f)
    print(d.get('tool_name') or d.get('toolName') or d.get('name') or '')
except: print('')
" 2>/dev/null)"

[[ -n "$TOOL_NAME" ]] || exit 1

AUTO_FILE="$AUTO_DIR/$SESSION_ID"
if [[ -f "$AUTO_FILE" ]] && grep -Fxq -- "$TOOL_NAME" "$AUTO_FILE"; then
  printf '{{"decision":"approve"}}\n'
  exit 0
fi

PENDING_TMP="$PENDING_DIR/$SESSION_ID.json.tmp.$$"
python3 -c "
import json, sys, time
with open('$REQ_FILE') as f:
    req = json.load(f)
tool = req.get('tool_name') or req.get('toolName') or req.get('name') or ''
now = int(time.time() * 1000)
out = {{
    'session_id': '$SESSION_ID',
    'pid': $PPID,
    'tool_name': tool,
    'description': req.get('description') or req.get('message'),
    'input': req.get('input', req),
    'created_at_ms': now,
    'expires_at_ms': now + 30000,
}}
with open('$PENDING_TMP', 'w') as f:
    json.dump(out, f, separators=(',', ':'))
" 2>/dev/null || exit 1

mv -f "$PENDING_TMP" "$PENDING_DIR/$SESSION_ID.json"

RESPONSE_FILE="$RESPONSE_DIR/$SESSION_ID.json"
for _ in $(seq 1 300); do
  if [[ -f "$RESPONSE_FILE" ]]; then
    if python3 -m json.tool "$RESPONSE_FILE" >/dev/null 2>&1; then
      APPROVE_ALL="$(python3 -c "
import json, sys
with open('$RESPONSE_FILE') as f:
    d = json.load(f)
print('true' if d.get('approve_all') is True else 'false')
" 2>/dev/null)"
      if [[ "$APPROVE_ALL" == "true" ]]; then
        touch "$AUTO_FILE"
        grep -Fxq -- "$TOOL_NAME" "$AUTO_FILE" || printf '%s\n' "$TOOL_NAME" >> "$AUTO_FILE"
      fi
      cat "$RESPONSE_FILE"
      rm -f "$RESPONSE_FILE" "$PENDING_DIR/$SESSION_ID.json"
      exit 0
    fi
  fi
  sleep 0.1
done

rm -f "$PENDING_DIR/$SESSION_ID.json"
exit 1
"#, home = home)
}
