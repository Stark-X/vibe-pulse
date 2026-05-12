# Tasks: Agent Row UI Enhancement

## Phase 1: Backend

- [ ] 1.1 state.rs — already has session_name + current_step fields (verify, no-op if correct)
- [ ] 1.2 agents.rs — change name field from "{tool_type} · {cwd_base}" to just cwd_base
- [ ] 1.3 agents.rs — add ClaudeSession struct and claude_metadata() function
- [ ] 1.4 agents.rs — add current_step_from_transcript() and format_step() helpers
- [ ] 1.5 agents.rs — call claude_metadata() after dedup+sort, only for Claude Code agents
- [ ] 1.6 src-tauri — cargo check passes with zero errors

## Phase 2: Frontend

- [ ] 2.1 tauri.conf.json — add img-src 'self' https://raw.githubusercontent.com to CSP
- [ ] 2.2 main.js — add CLAUDE_ICON_URL constant, update AGENT_ICON map for Claude Code to use img
- [ ] 2.3 main.js — update buildIcon() to render <img> when AGENT_ICON[tool].img exists
- [ ] 2.4 main.js — update buildAgentRow(): title = name + optional session_name suffix
- [ ] 2.5 main.js — update buildAgentRow(): render div.where with current_step + title attr, hide if null
- [ ] 2.6 styles.css — add .agent-row .where truncation rules (overflow/ellipsis/nowrap)
- [ ] 2.7 styles.css — add .agent-icon-img sizing rules (22x22, border-radius 6px, object-fit contain)

## Phase 3: Verification

- [ ] 3.1 Claude Code agent shows cwd_basename as title (e.g. "agent-workspace"), no tool prefix
- [ ] 3.2 Named session shows " · session-name" suffix (e.g. "vibe-island-hyper · vibe-island")
- [ ] 3.3 Unnamed session shows title only, no trailing " · "
- [ ] 3.4 Subtitle shows current tool step (e.g. "Bash · git status") truncated with ellipsis
- [ ] 3.5 Hovering subtitle shows full text in browser tooltip
- [ ] 3.6 Agent with no current_step (idle/Codex) shows no subtitle row
- [ ] 3.7 Claude Code icon renders as <img> with official logo
- [ ] 3.8 Codex and other tools still show styled glyph icon
