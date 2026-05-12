use std::io::{Read, Seek, SeekFrom};
use sysinfo::{ProcessesToUpdate, System};
use serde::Deserialize;
use serde_json::Value;
use crate::state::{AgentInfo, AgentStatus};
use crate::tmux::ppid_of;

struct AgentRule {
    tool_type: &'static str,
    proc_name: &'static str,
    exe_path_exclude: Option<&'static str>,
    argv_exclude: Option<&'static str>,
}

const RULES: &[AgentRule] = &[
    AgentRule { tool_type: "Claude Code", proc_name: "claude",   exe_path_exclude: None,           argv_exclude: None },
    AgentRule { tool_type: "Codex",       proc_name: "codex",    exe_path_exclude: Some("/usr/lib/"), argv_exclude: Some("app-server") },
    AgentRule { tool_type: "OpenCode",    proc_name: "opencode", exe_path_exclude: None,           argv_exclude: None },
];

pub fn scan(sys: &mut System) -> Vec<AgentInfo> {
    sys.refresh_processes(ProcessesToUpdate::All, true);

    let mut agents: Vec<AgentInfo> = Vec::new();

    for (pid, proc) in sys.processes() {
        let proc_name = proc.name().to_string_lossy();

        let exe_path = proc.exe().map(|p| p.display().to_string()).unwrap_or_default();

        let matched = RULES.iter().find(|rule| {
            if proc_name != rule.proc_name { return false; }
            if let Some(ex) = rule.exe_path_exclude {
                if exe_path.contains(ex) { return false; }
            }
            if let Some(ex) = rule.argv_exclude {
                let argv = proc.cmd().iter().map(|s| s.to_string_lossy()).collect::<Vec<_>>().join(" ");
                if argv.contains(ex) { return false; }
            }
            true
        });

        if let Some(rule) = matched {
            let pid_u32 = pid.as_u32();
            if !is_process_leader(pid_u32) { continue; }

            let cwd = std::fs::read_link(format!("/proc/{}/cwd", pid_u32))
                .map(|p| p.display().to_string())
                .unwrap_or_default();
            let cwd_base = std::path::Path::new(&cwd)
                .file_name()
                .and_then(|n| n.to_str())
                .unwrap_or("?")
                .to_owned();

            agents.push(AgentInfo {
                name: cwd_base,
                tool_type: rule.tool_type.to_owned(),
                pid: pid_u32,
                status: AgentStatus::Running,
                cwd,
                session_name: None,
                current_step: None,
                window_address: None,
            });
        }
    }

    dedup_to_roots(&mut agents);
    agents.sort_by_key(|a| a.pid);

    // Enrich Claude Code agents after dedup (avoid reads on discarded processes)
    for agent in &mut agents {
        if agent.tool_type == "Claude Code" {
            let (sname, step) = claude_metadata(agent.pid, &agent.cwd);
            agent.session_name = sname;
            agent.current_step = step;
        }
    }

    agents
}

// ── Claude Code session + transcript ────────────────────────────────────────

#[derive(Deserialize)]
struct ClaudeSession {
    #[serde(rename = "sessionId")]
    session_id: Option<String>,
    name: Option<String>,
    cwd: Option<String>,
}

fn claude_metadata(pid: u32, proc_cwd: &str) -> (Option<String>, Option<String>) {
    let Ok(home) = std::env::var("HOME") else { return (None, None) };

    let session_path = format!("{}/.claude/sessions/{}.json", home, pid);
    let Ok(raw) = std::fs::read_to_string(session_path) else { return (None, None) };
    let Ok(session) = serde_json::from_str::<ClaudeSession>(&raw) else { return (None, None) };

    let session_name = session.name.filter(|s| !s.is_empty());

    let step = session.session_id.as_deref().and_then(|sid| {
        let cwd = session.cwd.as_deref().unwrap_or(proc_cwd);
        let encoded = cwd.replace('/', "-");
        let transcript = format!("{}/.claude/projects/{}/{}.jsonl", home, encoded, sid);
        current_step_from_transcript(&transcript)
    });

    (session_name, step)
}

fn current_step_from_transcript(path: &str) -> Option<String> {
    let mut file = std::fs::File::open(path).ok()?;
    let len = file.metadata().ok()?.len();
    let start = len.saturating_sub(8192);
    file.seek(SeekFrom::Start(start)).ok()?;

    let mut buf = String::new();
    file.read_to_string(&mut buf).ok()?;

    // If we seeked into the middle, discard the partial first line
    let content = if start > 0 {
        buf.split_once('\n').map(|(_, r)| r).unwrap_or(&buf)
    } else {
        &buf
    };

    for line in content.lines().rev() {
        let Ok(v) = serde_json::from_str::<Value>(line) else { continue };
        if v["type"] != "assistant" { continue }
        let Some(items) = v.pointer("/message/content").and_then(Value::as_array) else { continue };
        for item in items.iter().rev() {
            if item["type"] == "tool_use" {
                if let Some(step) = format_step(item) {
                    return Some(step);
                }
            }
        }
    }
    None
}

fn format_step(item: &Value) -> Option<String> {
    let tool = item["name"].as_str()?.trim().to_owned();
    let input = &item["input"];

    let summary = ["description", "command", "path"]
        .iter()
        .find_map(|&k| {
            input[k].as_str()
                .filter(|s| !s.trim().is_empty())
                .map(|s| s.lines().next().unwrap_or(s).trim().to_owned())
        });

    Some(match summary {
        Some(s) => format!("{tool} · {s}"),
        None => tool,
    })
}

// ── Process helpers ──────────────────────────────────────────────────────────

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

fn is_process_leader(pid: u32) -> bool {
    let Ok(status) = std::fs::read_to_string(format!("/proc/{}/status", pid)) else {
        return false;
    };
    let mut pid_val: Option<u32> = None;
    let mut tgid_val: Option<u32> = None;
    for line in status.lines() {
        if let Some(v) = line.strip_prefix("Pid:") { pid_val = v.trim().parse().ok(); }
        else if let Some(v) = line.strip_prefix("Tgid:") { tgid_val = v.trim().parse().ok(); }
        if pid_val.is_some() && tgid_val.is_some() { break; }
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
