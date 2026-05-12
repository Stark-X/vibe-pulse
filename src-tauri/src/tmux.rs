pub fn ppid_of(pid: u32) -> Option<u32> {
    let status = std::fs::read_to_string(format!("/proc/{}/status", pid)).ok()?;
    for line in status.lines() {
        if let Some(rest) = line.strip_prefix("PPid:") {
            return rest.trim().parse().ok();
        }
    }
    None
}

fn exe_name_of(pid: u32) -> Option<String> {
    let comm = std::fs::read_to_string(format!("/proc/{}/comm", pid)).ok()?;
    Some(comm.trim().to_owned())
}

/// Maps a tmux pane's shell PID to the terminal emulator PID attached to that session.
/// Returns terminal client PID if found.
pub fn find_terminal_for_tmux_pane(agent_pid: u32) -> Option<u32> {
    // Walk PPID chain looking for a tmux: server process
    let mut cur = agent_pid;
    let mut tmux_found = false;
    for _ in 0..16 {
        let parent = ppid_of(cur)?;
        let name = exe_name_of(parent).unwrap_or_default();
        if name.starts_with("tmux") {
            tmux_found = true;
            break;
        }
        cur = parent;
    }
    if !tmux_found {
        return None;
    }

    // Find which tmux pane contains agent_pid
    let panes_out = std::process::Command::new("tmux")
        .args(["list-panes", "-a", "-F", "#{pane_pid} #{session_name} #{window_index} #{pane_index}"])
        .output()
        .ok()?;
    let panes = String::from_utf8_lossy(&panes_out.stdout);

    let mut target_session: Option<String> = None;
    for line in panes.lines() {
        let mut parts = line.split_whitespace();
        let pane_pid: u32 = parts.next()?.parse().ok()?;
        let session = parts.next()?.to_owned();
        // Check if agent_pid is a child of this pane's shell
        if is_ancestor(pane_pid, agent_pid) {
            target_session = Some(session);
            break;
        }
    }
    let session = target_session?;

    // Find terminal client attached to that session
    let clients_out = std::process::Command::new("tmux")
        .args(["list-clients", "-t", &session, "-F", "#{client_pid}"])
        .output()
        .ok()?;
    String::from_utf8_lossy(&clients_out.stdout)
        .lines()
        .next()
        .and_then(|l| l.trim().parse::<u32>().ok())
}

fn is_ancestor(ancestor_pid: u32, child_pid: u32) -> bool {
    let mut cur = child_pid;
    for _ in 0..16 {
        match ppid_of(cur) {
            Some(p) if p == ancestor_pid => return true,
            Some(p) if p <= 1 => return false,
            Some(p) => cur = p,
            None => return false,
        }
    }
    false
}
