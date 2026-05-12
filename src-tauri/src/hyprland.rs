use crate::state::HyprClient;
use crate::tmux::ppid_of;

pub fn available() -> bool {
    std::env::var("HYPRLAND_INSTANCE_SIGNATURE").is_ok()
}

pub fn get_clients() -> Vec<HyprClient> {
    let Ok(out) = std::process::Command::new("hyprctl")
        .args(["clients", "-j"])
        .output()
    else {
        return vec![];
    };
    serde_json::from_slice(&out.stdout).unwrap_or_default()
}

pub fn find_window_address(agent_pid: u32, clients: &[HyprClient]) -> Option<String> {
    // Direct match: terminal window IS the agent process
    if let Some(c) = clients.iter().find(|c| c.pid == agent_pid as i64) {
        return Some(c.address.clone());
    }
    // Walk PPID chain: agent lives inside a terminal (or tmux)
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
    if !available() {
        return Err("Hyprland not available".into());
    }
    if address.is_empty() {
        return Err("Empty window address".into());
    }
    let out = std::process::Command::new("hyprctl")
        .args(["dispatch", "focuswindow", &format!("address:{}", address)])
        .output()
        .map_err(|e| e.to_string())?;
    if out.status.success() {
        Ok(())
    } else {
        Err(String::from_utf8_lossy(&out.stderr).trim().to_owned())
    }
}
