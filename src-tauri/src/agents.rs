use sysinfo::{ProcessesToUpdate, System};
use crate::state::{AgentInfo, AgentStatus};
use crate::tmux::ppid_of;

struct AgentRule {
    tool_type: &'static str,
    proc_name: &'static str,
    exe_path_exclude: Option<&'static str>,
    argv_exclude: Option<&'static str>,
}

const RULES: &[AgentRule] = &[
    AgentRule {
        tool_type: "Claude Code",
        proc_name: "claude",
        exe_path_exclude: None,
        argv_exclude: None,
    },
    AgentRule {
        tool_type: "Codex",
        proc_name: "codex",
        // Codex desktop app lives in /usr/lib/codex/; CLI is in user's home dir
        exe_path_exclude: Some("/usr/lib/"),
        argv_exclude: Some("app-server"),
    },
    AgentRule {
        tool_type: "OpenCode",
        proc_name: "opencode",
        exe_path_exclude: None,
        argv_exclude: None,
    },
];

pub fn scan(sys: &mut System) -> Vec<AgentInfo> {
    sys.refresh_processes(ProcessesToUpdate::All, true);

    let mut agents: Vec<AgentInfo> = Vec::new();

    for (pid, proc) in sys.processes() {
        let proc_name = proc.name().to_string_lossy();

        let exe_path = proc
            .exe()
            .map(|p| p.display().to_string())
            .unwrap_or_default();

        let matched = RULES.iter().find(|rule| {
            if proc_name != rule.proc_name {
                return false;
            }
            if let Some(ex) = rule.exe_path_exclude {
                if exe_path.contains(ex) {
                    return false;
                }
            }
            if let Some(ex) = rule.argv_exclude {
                let argv = proc.cmd()
                    .iter()
                    .map(|s| s.to_string_lossy())
                    .collect::<Vec<_>>()
                    .join(" ");
                if argv.contains(ex) {
                    return false;
                }
            }
            true
        });

        if let Some(rule) = matched {
            let pid_u32 = pid.as_u32();

            // sysinfo 0.32 returns threads as separate entries alongside processes.
            // Skip threads: a thread has Pid != Tgid in /proc/pid/status.
            if !is_process_leader(pid_u32) {
                continue;
            }

            // sysinfo 0.32 doesn't reliably populate cwd — read directly from /proc
            let cwd = std::fs::read_link(format!("/proc/{}/cwd", pid_u32))
                .map(|p| p.display().to_string())
                .unwrap_or_default();
            let cwd_base = std::path::Path::new(&cwd)
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or("?")
                .to_owned();

            agents.push(AgentInfo {
                name: format!("{} · {}", rule.tool_type, cwd_base),
                tool_type: rule.tool_type.to_owned(),
                pid: pid_u32,
                status: AgentStatus::Running,
                cwd,
                window_address: None,
            });
        }
    }

    // Keep only root processes per tool type.
    // Subagent claude processes are children of the session claude — discard them.
    dedup_to_roots(&mut agents);

    // Stable sort by PID so the list order never changes between polls.
    agents.sort_by_key(|a| a.pid);

    agents
}

fn dedup_to_roots(agents: &mut Vec<AgentInfo>) {
    let all: Vec<(u32, String)> = agents.iter().map(|a| (a.pid, a.tool_type.clone())).collect();

    let to_remove: std::collections::HashSet<u32> = all
        .iter()
        .filter(|(pid, tool)| {
            all.iter().any(|(other, other_tool)| {
                other != pid && other_tool == tool && is_ancestor(*other, *pid)
            })
        })
        .map(|(pid, _)| *pid)
        .collect();

    agents.retain(|a| !to_remove.contains(&a.pid));
}

/// Returns true only if this pid is a process group leader (Pid == Tgid).
/// Threads have Pid != Tgid and should be excluded.
fn is_process_leader(pid: u32) -> bool {
    let Ok(status) = std::fs::read_to_string(format!("/proc/{}/status", pid)) else {
        return false; // can't read status — exclude to avoid ghost entries
    };
    let mut pid_val: Option<u32> = None;
    let mut tgid_val: Option<u32> = None;
    for line in status.lines() {
        if let Some(v) = line.strip_prefix("Pid:") {
            pid_val = v.trim().parse().ok();
        } else if let Some(v) = line.strip_prefix("Tgid:") {
            tgid_val = v.trim().parse().ok();
        }
        if pid_val.is_some() && tgid_val.is_some() {
            break;
        }
    }
    pid_val.is_some() && pid_val == tgid_val
}

fn is_ancestor(ancestor: u32, pid: u32) -> bool {
    let mut cur = pid;
    for _ in 0..16 {
        match ppid_of(cur) {
            Some(p) if p == ancestor => return true,
            Some(p) if p <= 1 => return false,
            Some(p) => cur = p,
            None => return false,
        }
    }
    false
}
