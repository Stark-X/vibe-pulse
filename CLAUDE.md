# Pulse — AI Agent Monitor

Qt6/QML floating overlay for monitoring AI coding agents (Claude Code, Codex, OpenCode) on Ubuntu 24.04 + Hyprland.

## Stack
- Backend: C++ + Qt6 + sysinfo (via /proc)
- Frontend: QML (Qt Quick)
- Build: CMake

## Dev Commands
```bash
make build   # cmake --build build_rel --target pulse
make run     # build + run
make toggle  # build + run pulse-toggle
make clean   # clean build artifacts

# Mock preview scenes
make mock-idle
make mock-working
make mock-permission
make mock-question
make mock-plan
make mock-expanded
```

## .context 项目上下文

> 项目使用 `.context/` 管理开发决策上下文。

- 编码规范：`.context/prefs/coding-style.md`
- 工作流规则：`.context/prefs/workflow.md`
- 决策历史：`.context/history/commits.md`

**规则**：修改代码前必读 prefs/，做决策时按 workflow.md 规则记录日志。
