# Pulse — AI Agent Monitor

Tauri 2 floating overlay for monitoring AI coding agents (Claude Code, Codex, OpenCode) on Ubuntu 24.04 + Hyprland.

## Stack
- Backend: Rust + Tauri 2 + sysinfo
- Frontend: Vanilla JS (no framework) + Vite
- Package manager: bun

## Dev Commands
```bash
bun run tauri dev    # hot-reload dev mode
bun run build        # frontend only
bun run tauri build  # production binary
cargo check          # Rust compile check (run from src-tauri/)
```

## .context 项目上下文

> 项目使用 `.context/` 管理开发决策上下文。

- 编码规范：`.context/prefs/coding-style.md`
- 工作流规则：`.context/prefs/workflow.md`
- 决策历史：`.context/history/commits.md`

**规则**：修改代码前必读 prefs/，做决策时按 workflow.md 规则记录日志。
