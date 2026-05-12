# Proposal: Agent Row UI Enhancement

## Overview

改进 Pulse 中每个 agent 行的信息密度与视觉质量：
- 去除冗余工具名重复，标题改为 `{cwd_basename}` 或 `{cwd_basename} · {session_name}`
- 副标题显示当前执行步骤（截断），hover 显示完整内容
- Claude Code 图标替换为官方 logo

**目标平台**: Ubuntu 24.04 + Hyprland  
**技术栈**: Tauri 2 + Rust backend + Vanilla JS frontend

---

## Data Sources (已验证)

`~/.claude/sessions/<pid>.json` — 按 PID 命名，字段：
```json
{ "pid": 862784, "sessionId": "uuid", "cwd": "/path", "name": "vibe-island", "status": "idle|busy" }
```

Transcript: `~/.claude/projects/<encoded-cwd>/<sessionId>.jsonl`
- encoded-cwd = `cwd.replace('/', '-')`
- 每行 JSON，`type=="assistant"` 的行包含 `message.content[]` 数组
- `content[].type=="tool_use"` 的条目有 `name`（工具名）和 `input`（参数）
- `input` 优先字段：`description` > `command`（首行）> `path`

---

## User Confirmations

- 标题格式：`{cwd_basename}` 必须显示，有 name 时附加 ` · {session_name}`
- Claude Code 图标 URL: `https://raw.githubusercontent.com/lobehub/lobe-icons/refs/heads/master/packages/static-png/dark/claudecode-color.png`
- Codex / OpenCode：保持现有字形图标
- hover 用原生 `title` 属性（浏览器 tooltip）

---

## Scope

**In scope:**
- Rust: `state.rs` 新增字段，`agents.rs` 读取 claude session + transcript
- Frontend: `buildAgentRow` 重构，`AGENT_ICON` / `buildIcon` 更新
- CSP: 添加 `img-src` for raw.githubusercontent.com

**Out of scope:**
- `idle/busy` 状态显示（future enhancement）
- Codex / OpenCode 的 current_step（无等效数据源）
- 多 session 管理
