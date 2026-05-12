# Proposal: Agent Monitor Real Integration

## Overview

将 Pulse 从静态 UI 原型升级为真实 AI agent 监控工具。Tauri 2 后端轮询系统进程、通过 Hyprland IPC 定位窗口，前端展示实时 agent 状态并支持一键跳转。

**目标平台**: Ubuntu 24.04 + Hyprland  
**技术栈**: Tauri 2 + Vite + Vanilla JS（不引入前端框架）

---

## Discovered Constraints

### Hard Constraints

1. **进程检测规则**
   - Claude Code: `comm/exe basename == "claude"`（native ELF，`~/.local/bin/claude`）；node 兜底仅限 argv 含 `@anthropic-ai/claude-code` 时
   - Codex: `comm/exe basename == "codex"` 且非 desktop 形态（排除 `codex app-server`、Electron class=Codex）；node launcher 与 native child 合并为一个 agent
   - OpenCode: `comm/exe basename == "opencode"`；node/bun 兜底匹配 argv 含 `opencode-ai`

2. **Hyprland IPC 字段**
   - `hyprctl clients -j` 返回数组，关键字段：`address`、`pid`、`class`、`title`、`workspace.id`
   - `hyprctl dispatch focuswindow address:<addr>` 执行窗口聚焦
   - `hyprctl` 属同步 IPC，高频调用导致 compositor 卡顿；**轮询间隔 ≥ 2s**，结果需缓存

3. **tmux 映射链**（用户选择支持）
   - agent PID → `/proc/<pid>/status` 追溯父链 → 找 tmux server PID
   - `tmux list-panes -a -F '#{pane_pid} ...'` 定位 pane
   - `tmux list-clients -F '#{client_pid} ...'` 找 terminal client PID
   - terminal client PID → 遍历 `hyprctl clients` 的 `pid` 字段匹配窗口

4. **窗口配置约束**
   - `focus: false` — Pulse 不抢焦点，键盘事件在窗口非聚焦时无效
   - `transparent + alwaysOnTop` — 窗口渲染链不可破坏
   - `resizable: false` — 只能通过 `setSize` 动态调整高度

5. **状态精度**: 仅 `running` / `exited`，不解析日志判断 thinking/waiting

6. **Tauri 架构**
   - 后端用 `std::process::Command` 调 `hyprctl`，不需要 `tauri-plugin-shell`
   - 新增 Cargo 依赖：`sysinfo`（进程扫描），可选 `thiserror`（错误类型）
   - 前端通过 `@tauri-apps/api/core invoke()` 拉取数据；后端可 `emit` 事件推送变更

7. **Tauri commands 需注册**
   ```rust
   invoke_handler(tauri::generate_handler![get_agents, focus_window])
   ```
   能力文件 `capabilities/default.json` 需添加对应权限。

### Soft Constraints

- 前端 `render()` 全量重建 DOM；接入真实数据后需节流（变更 hash 比对），避免无效刷新
- CSS 变量与 OKLCH 色彩体系不变
- 无框架，继续用 `h()` hyperscript 构建 DOM

---

## Dependencies

| 类型 | 依赖 | 说明 |
|------|------|------|
| Cargo | `sysinfo = "0.30"` | 进程扫描，跨平台 |
| Cargo | `thiserror = "1"` | 错误类型（可选） |
| npm | `@tauri-apps/plugin-global-shortcut` | 全局快捷键（两者都支持方案） |

---

## Risks & Mitigations

| 风险 | 缓解 |
|------|------|
| Codex CLI vs desktop 同名误判 | 过滤 `class=Codex` 的 Hyprland 窗口（desktop），仅匹配无 GUI class 的 codex 进程 |
| tmux 场景 PID 链中断 | 多步 fallback：直连 → tmux pane → tmux client；任一步失败标记 `window_address: null` |
| hyprctl 频繁调用卡 compositor | 固定 2s 轮询 + 变更 hash 缓存，focus_window 前校验 address 有效性 |
| Pulse 非 Hyprland session 启动 | 检测 `HYPRLAND_INSTANCE_SIGNATURE`，缺失时降级：显示 agent 进程但禁用跳转按钮 |
| render() 高频刷新闪烁 | 比对 agents JSON hash，无变化跳过 render |
| API key/token 出现在 argv | get_agents 返回 DTO 只暴露 `name, tool_type, pid, status, cwd, window_address`，不暴露完整 argv |

---

## Success Criteria

1. 启动 `claude` 后，Pulse 在 ≤ 4s 内显示 Claude Code agent 卡片，状态 `running`
2. 启动 `codex` 后，只出现一个 Codex agent 卡片（CLI），不含 desktop app-server
3. 启动 `opencode` 后，正确识别并显示
4. 点击 agent 卡片或触发快捷键，焦点切换到对应终端窗口（含 tmux 场景）
5. 关闭 agent 后，卡片在 ≤ 4s 内消失或标记 `exited`
6. Hyprland 不可用时，显示 agent 列表但跳转按钮禁用，无崩溃
7. 连续运行 10 分钟，Hyprland 无明显卡顿，CPU 占用 < 2%

---

## User Confirmations

- Codex 范围：**仅 CLI 终端**（排除 desktop app）
- tmux 支持：**是**（需 pane → client → hyprland window 映射链）
- 状态精度：**running / exited 二态**
- 快捷键方式：**两者都支持**（Tauri global-shortcut + 文档说明 hyprland.conf 方案）

---

## Scope Boundary

**In scope:**
- Rust 后端：进程扫描、Hyprland window 映射、tmux 映射、Tauri IPC 命令
- 前端：替换 mock 数据为真实 IPC 调用、空状态 UI、跳转动作
- 全局快捷键注册（Tauri plugin）

**Out of scope:**
- agent 内部状态（thinking/writing/waiting）解析
- Windows / macOS 支持
- Codex desktop app 监控
- agent 进程的启动/停止控制
