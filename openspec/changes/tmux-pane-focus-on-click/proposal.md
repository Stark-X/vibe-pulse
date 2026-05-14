# Proposal: Tmux Pane Focus on Agent Row Click

## Overview

点击 widget expanded view 中的 agent 行时，精确跳转到该 agent 所在的 tmux session，并将焦点移动到对应的 window + pane。同时也聚焦 Hyprland 终端窗口。

**平台**: Ubuntu 24.04 + Hyprland + tmux  
**技术栈**: C++17 + Qt6 + QML

---

## Enhanced Requirement (Prompt 增强)

**意图**: 从 expanded view 一键导航到 agent 所在的工作环境  
**技术约束**:
- 必须先聚焦 Hyprland 窗口（terminal emulator），再切换 tmux pane
- tmux 定位需要 session_name + window_index + pane_index + client_tty
- `canJump` 字段控制 QML 点击行为和光标样式
- 不能复用 `sessionId`/`sessionName`（已被 Claude/Codex 会话 ID 占用）

**范围边界**: 仅改 C++ 层（TmuxResolver, AgentInfo, AgentModel, main.cpp），QML 无需修改核心点击逻辑  
**验收标准**: 点击 agent 行后，tmux 的 active pane 精确变为 agent 所在 pane

---

## Discovered Constraints

### Hard Constraints

1. **TmuxResolver 返回类型不足**: 当前 `findTerminalPid()` 只返回 `std::optional<quint32>` terminal PID；必须扩展为返回包含 `sessionName`, `windowIndex`, `paneIndex`, `clientTty` 的结构体。

2. **AgentInfo 需要新 tmux 字段**: 不能复用 `sessionId`/`sessionName`（这些是 Claude/Codex 会话语义字段，用于 ResponseWriter IPC）。需要增加 `tmuxTarget`（格式 `session:window.pane`）和 `tmuxClientTty`（如 `/dev/pts/3`）。

3. **dataEquals() 必须包含新字段**: 若 `tmuxTarget` 变化后未触发 `dataChanged`，点击行为将使用旧 target。

4. **tmux switch-client 语法**: `tmux switch-client [-c target-client] [-t target-session]`，其中 `-c` 接受 client TTY（如 `/dev/pts/3`），`-t` 接受 `session:window.pane` 格式 target。

5. **canJump 仍基于 windowAddress**: 无 Hyprland 窗口无法聚焦终端，`canJump` 不应因 tmux 信息存在就设为 true；tmux 精确切 pane 是附加能力，不替代 Hyprland 聚焦。

6. **list-panes 格式需扩展**: 当前 `#{pane_pid}\t#{session_id}\t#{session_name}` 需增加 `#{window_index}\t#{pane_index}`；list-clients 格式需增加 `#{client_tty}`。

### Soft Constraints

1. **QML 无需核心改动**: `focusAgent()` 的 C++ 内部逻辑变化对 QML 透明，ExpandedView.qml 调用 `agentModel.focusAgent(index)` 不变。

2. **canJump 光标一致性**: `ExpandedView` 和 `AgentRow.qml` 都使用 `model.canJump` 控制光标，保持现有行为即可。

3. **ExpandedView 重构为可选**: 当前 ExpandedView 内联 delegate 而不使用 AgentRow.qml 组件，两者逻辑有重复，但本次变更范围不包含此重构。

### Dependencies

- `src/TmuxResolver.*` ← 定义新结构体 + 扩展 `tmux list-panes/list-clients` 格式
- `src/AgentInfo.h` ← 新增 `tmuxTarget`, `tmuxClientTty`，更新 `dataEquals()`
- `src/AgentModel.cpp` ← `focusAgent()` 执行 `tmux switch-client`；`data()` 和 `get()` 可选暴露新字段
- `src/main.cpp enrichAgents()` ← 调用扩展后的 TmuxResolver，写入 AgentInfo 新字段

### Risks

| 风险 | 缓解方案 |
|------|---------|
| 多 tmux client attach 同一 session | 选 `client_pid` 能映射到 `windowAddress` 的 client；退回选第一个 |
| PPID 回溯深度（8 hops）不足 | 保持现状；影响边缘情况，fallback 为只聚焦 Hyprland 窗口 |
| tmux 未运行或 list-panes 超时 | 已有 1 秒 waitForFinished 保护；失败时 `tmuxTarget` 留空，点击仍聚焦 Hyprland 窗口 |
| `client_tty` 为空或 client 已 detach | switch-client 失败时 fire-and-forget，不影响 Hyprland focus |
| `session_name` 含特殊字符 | 优先使用 `session_id:window_index.pane_index` 作为 tmux target |
| `WindowStaysOnTopHint` 遮挡终端 | 现有行为；本次不处理，可后续单独优化 |

---

## Success Criteria

可验证的成功行为：

1. **tmux agent 精确跳转**: 点击 agent 行 → Hyprland 聚焦对应 terminal 窗口 → tmux active pane 变为 agent 所在 pane
2. **多 window/pane 场景**: agent 不在当前 active window 时，点击后 tmux 切换到正确 window + pane
3. **非 tmux agent 不回退**: agent 不在 tmux 中时，点击行为与当前相同（只聚焦 Hyprland 窗口）
4. **tmux 信息更新**: agent pane 变化后，下次 enrichAgents() 刷新时 `tmuxTarget` 更新，触发 `dataChanged`
5. **失败时优雅降级**: tmux 命令失败时，Hyprland focus 仍然执行，`canJump` 不受影响

---

## Implementation Scope

### 文件变更清单

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `src/TmuxResolver.h` | 重构 | 定义 `TmuxPaneInfo` 结构体，`findPaneInfo()` 替代 `findTerminalPid()` |
| `src/TmuxResolver.cpp` | 重构 | 扩展 list-panes/list-clients 字段，返回完整 pane 定位信息 |
| `src/AgentInfo.h` | 扩展 | 新增 `tmuxTarget`, `tmuxClientTty`，更新 `dataEquals()` |
| `src/AgentModel.cpp` | 扩展 | `focusAgent()` 增加 `tmux switch-client` 调用 |
| `src/main.cpp` | 更新 | `enrichAgents()` 使用新 TmuxResolver API 填充新字段 |

### 不在范围内

- QML 视觉反馈（click flash 动画）
- ExpandedView 重构使用 AgentRow 组件
- Widget 跳转后自动收起
- 无 Hyprland 环境下的纯 tmux 跳转支持

---

## User Decisions

- **跳转精度**: 精确跳转到 pane（`tmux switch-client -c <tty> -t session:window.pane`）
- **canJump 策略**: 保持基于 `windowAddress`，tmux 为附加能力
- **tmux target 格式**: 优先 `session_id:window_index.pane_index`（避免 session_name 特殊字符问题）
