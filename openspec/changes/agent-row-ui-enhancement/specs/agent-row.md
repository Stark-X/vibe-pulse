# Spec: Agent Row UI Enhancement

## AgentInfo Changes

```
AgentInfo {
  name:           string   // cwd_basename ONLY (e.g. "agent-workspace") — NO tool prefix
  tool_type:      string   // unchanged
  pid:            number
  status:         "running" | "exited"
  cwd:            string
  session_name:   string | null   // NEW: from ~/.claude/sessions/<pid>.json "name" field
  current_step:   string | null   // NEW: last tool_use from transcript
  window_address: string | null
}
```

## Backend: claude_metadata()

Called for **Claude Code processes only**, after dedup_to_roots (avoid wasted reads on discarded processes).

```
function claude_metadata(pid, proc_cwd):
  1. Read ~/.claude/sessions/<pid>.json
     - Parse: session_id, name (optional), cwd (optional)
     - On any error → return (None, None)
  2. encoded_cwd = session.cwd ?? proc_cwd → replace('/', '-')
  3. transcript = ~/.claude/projects/{encoded_cwd}/{session_id}.jsonl
  4. Read last 8192 bytes of transcript
     - If offset > 0, discard first (possibly partial) line
     - Iterate lines in reverse
     - Find first "assistant" entry with tool_use content
     - Format: "{tool_name} · {summary}" where summary = input.description ?? input.command.split('\n')[0] ?? input.path ?? ""
     - If summary empty → return just "{tool_name}"
     - On any error → return None
  5. Return (session.name, current_step)
```

## current_step Format

`"Bash · git status --short"` — tool name + ` · ` + first meaningful input field.

Priority: `input.description` → `input.command` (first line) → `input.path` → omit ` · ` part.

Max length: no server-side truncation (frontend handles via CSS ellipsis).

## Frontend: buildAgentRow()

```
Title (div.name):
  - Always: agent.name  (cwd_basename)
  - If agent.session_name: append " · " + agent.session_name
  
Subtitle (div.where):
  - If agent.current_step !== null: show truncated text, title attr = full text
  - If null: do NOT render the div.where element (single-line layout)

Icon:
  - Claude Code: <img class="agent-icon-img" src="{CLAUDE_ICON_URL}" alt="Claude Code">
  - Others: existing styled glyph div (unchanged)
```

## CSS Requirements

```css
/* Subtitle truncation */
.agent-row .where {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

/* Official icon img sizing */
.agent-icon-img {
  width: 22px;
  height: 22px;
  border-radius: 6px;
  object-fit: contain;
  display: block;
  flex-shrink: 0;
}
```

## CSP

```
img-src 'self' https://raw.githubusercontent.com
```

Full new value:
```
default-src 'self'; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; script-src 'self'; img-src 'self' https://raw.githubusercontent.com
```

## PBT Properties

| Invariant | Falsification |
|-----------|--------------|
| `name` never contains " · {tool_type}" prefix | Generate agents, assert no name starts with "Claude Code" or "Codex" |
| If `session_name` null → title has no " · " suffix | Set session_name=null, render, assert title == cwd_basename |
| `current_step` null → no div.where in DOM | Set current_step=null, render, assert querySelector('.where') === null |
| Claude Code icon is `<img>` not `<div>` | Render Claude Code agent, assert icon element tagName === "IMG" |
| Others icon is `<div>` not `<img>` | Render Codex agent, assert icon element tagName === "DIV" |
| 8KB tail read never panics on file < 8KB | Transcript size 0..8192, always returns Option not crash |
