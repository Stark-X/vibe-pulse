# Design: Agent Row UI Enhancement

## Backend Module Changes

### state.rs
```rust
pub struct AgentInfo {
    pub name: String,                   // cwd_basename only
    pub tool_type: String,
    pub pid: u32,
    pub status: AgentStatus,
    pub cwd: String,
    pub session_name: Option<String>,   // NEW
    pub current_step: Option<String>,   // NEW
    pub window_address: Option<String>,
}
```

### agents.rs — new function `claude_metadata`

```rust
use serde::Deserialize;
use serde_json::Value;
use std::io::{Read, Seek, SeekFrom};

#[derive(Deserialize)]
struct ClaudeSession {
    #[serde(rename = "sessionId")]
    session_id: Option<String>,
    name: Option<String>,
    cwd: Option<String>,
}

fn claude_metadata(pid: u32, proc_cwd: &str) -> (Option<String>, Option<String>) {
    let home = std::env::var("HOME").ok()?;
    let session_path = format!("{}/.claude/sessions/{}.json", home, pid);
    let raw = std::fs::read_to_string(session_path).ok()?;
    let session: ClaudeSession = serde_json::from_str(&raw).ok()?;

    let step = session.session_id.as_deref().and_then(|sid| {
        let cwd = session.cwd.as_deref().unwrap_or(proc_cwd);
        let encoded = cwd.replace('/', "-");
        let transcript = format!("{}/.claude/projects/{}/{}.jsonl", home, encoded, sid);
        current_step_from_transcript(&transcript)
    });

    (session.name.filter(|s| !s.is_empty()), step)
}

fn current_step_from_transcript(path: &str) -> Option<String> {
    let mut file = std::fs::File::open(path).ok()?;
    let len = file.metadata().ok()?.len();
    let start = len.saturating_sub(8192);
    file.seek(SeekFrom::Start(start)).ok()?;

    let mut buf = String::new();
    file.read_to_string(&mut buf).ok()?;

    // Discard potentially partial first line if we seeked into the middle
    let content = if start > 0 {
        buf.split_once('\n').map(|(_, rest)| rest).unwrap_or(&buf)
    } else {
        &buf
    };

    for line in content.lines().rev() {
        let Ok(v) = serde_json::from_str::<Value>(line) else { continue };
        if v["type"] != "assistant" { continue }
        let Some(items) = v.pointer("/message/content").and_then(Value::as_array) else { continue };
        for item in items.iter().rev() {
            if item["type"] == "tool_use" {
                return format_step(item);
            }
        }
    }
    None
}

fn format_step(item: &Value) -> Option<String> {
    let tool = item["name"].as_str()?.trim().to_owned();
    let input = &item["input"];
    let summary = ["description", "command", "path"].iter()
        .find_map(|&k| input[k].as_str().filter(|s| !s.trim().is_empty()))
        .map(|s| s.lines().next().unwrap_or(s).trim().to_owned());

    Some(match summary {
        Some(s) => format!("{tool} · {s}"),
        None => tool,
    })
}
```

### agents.rs — scan() integration

Dedup first, then enrich (avoids wasted reads on discarded entries):

```rust
// After dedup_to_roots and sort_by_key:
for agent in &mut agents {
    if agent.tool_type == "Claude Code" {
        let (sname, step) = claude_metadata(agent.pid, &agent.cwd);
        agent.session_name = sname;
        agent.current_step = step;
    }
}
```

### name field

Change from `format!("{} · {}", rule.tool_type, cwd_base)` to just `cwd_base`.

---

## Frontend Changes

### AGENT_ICON map addition

```js
const CLAUDE_ICON_URL = 'https://raw.githubusercontent.com/lobehub/lobe-icons/refs/heads/master/packages/static-png/dark/claudecode-color.png';

const AGENT_ICON = {
  'Claude Code': { img: CLAUDE_ICON_URL },        // img-based
  'Codex':       { color: 'var(--sky)',    glyph: 'X' },
  'OpenCode':    { color: 'var(--coral)',  glyph: 'O' },
  // ... rest unchanged
};
```

### buildIcon() update

```js
function buildIcon(tool) {
  const c = AGENT_ICON[tool] || { color: 'var(--text-2)', glyph: '·' };
  if (c.img) {
    return h('img', { className: 'agent-icon-img', src: c.img, alt: tool });
  }
  return h('div', { className: 'agent-icon', style: {
    background: `color-mix(in oklch, ${c.color} 18%, var(--bg-3))`,
    border: `1px solid color-mix(in oklch, ${c.color} 45%, var(--border))`,
    color: c.color,
  }}, c.glyph);
}
```

### buildAgentRow() update

```js
function buildAgentRow(agent) {
  const indicatorCls = agent.status === 'running' ? 'working' : 'done';
  const canJump = !!agent.window_address;
  const title = agent.session_name
    ? `${agent.name} · ${agent.session_name}`
    : agent.name;

  const nameChildren = [title];
  const whereEl = agent.current_step
    ? h('div', { className: 'where', title: agent.current_step }, agent.current_step)
    : null;

  return h('button', {
    className: 'agent-row',
    title: canJump ? `Jump to ${agent.tool_type}` : 'No terminal window found',
    onClick: () => jumpToAgent(agent),
  },
    h('span', { className: `indicator ${indicatorCls}` }),
    h('div', { style: { minWidth: 0 } },
      h('div', { className: 'name' }, title),
      whereEl,
    ),
    buildIcon(agent.tool_type),
    canJump ? null : h('span', { className: 'age', style: { color: 'var(--text-3)' } }, '—'),
  );
}
```

### CSS additions (styles.css)

```css
.agent-row .where {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.agent-icon-img {
  width: 22px;
  height: 22px;
  border-radius: 6px;
  object-fit: contain;
  display: block;
  flex-shrink: 0;
}
```

### CSP (tauri.conf.json)

```
"csp": "default-src 'self'; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; script-src 'self'; img-src 'self' https://raw.githubusercontent.com"
```
