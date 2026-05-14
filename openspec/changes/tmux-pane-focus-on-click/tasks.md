# Tasks: Tmux Pane Focus on Agent Row Click

## Phase 1: TmuxResolver 重构

- [x] 1.1 `src/TmuxResolver.h` — 删除旧 `findTerminalPid` 声明，定义 `TmuxPaneInfo` 结构体（字段：terminalPid, sessionId, sessionName, windowIndex, paneIndex, clientTty, tmuxTarget），声明 `static std::optional<TmuxPaneInfo> findPaneInfo(quint32 agentPid)`
- [x] 1.2 `src/TmuxResolver.cpp` — 实现 `findPaneInfo()`：扩展 list-panes 格式为 5 列 `#{pane_pid}\t#{session_id}\t#{session_name}\t#{window_index}\t#{pane_index}`，按 hop 距离匹配最近祖先 pane；扩展 list-clients 为 2 列 `#{client_pid}\t#{client_tty}`，选第一个有效 client（pid>1 且 tty 非空）；组合 tmuxTarget = `sessionId:windowIndex.paneIndex`；删除旧 `findTerminalPid` 实现

## Phase 2: AgentInfo 扩展

- [x] 2.1 `src/AgentInfo.h` — 在 `AgentInfo` struct 的 `windowAddress` 后新增 `QString tmuxTarget` 和 `QString tmuxClientTty`（默认空字符串）；在 `dataEquals()` 中追加 `&& tmuxTarget == o.tmuxTarget && tmuxClientTty == o.tmuxClientTty`

## Phase 3: AgentModel 扩展

- [x] 3.1 `src/AgentModel.cpp` — 修改 `focusAgent()` 方法：在 `HyprlandClient::focusWindow(a.windowAddress)` 之后，当 `!a.tmuxTarget.isEmpty() && !a.tmuxClientTty.isEmpty()` 时，调用 `QProcess::startDetached("tmux", {"switch-client", "-c", a.tmuxClientTty, "-t", a.tmuxTarget})`；需要 `#include <QProcess>`（检查是否已 include）

## Phase 4: main.cpp 集成

- [x] 4.1 `src/main.cpp enrichAgents()` — 将 `TmuxResolver::findTerminalPid(a.pid)` 调用替换为 `TmuxResolver::findPaneInfo(a.pid)`；从返回的 `TmuxPaneInfo` 中取 `terminalPid` 用于 `findWindowAddress`，并将 `pi->tmuxTarget` 写入 `a.tmuxTarget`，`pi->clientTty` 写入 `a.tmuxClientTty`

## Phase 5: 构建与验证

- [x] 5.1 运行 `make build`，确认无编译错误和警告
- [ ] 5.2 启动测试：在 tmux session 中运行 `claude` 或 `codex`，运行 `make run`，展开 widget，点击 agent 行，验证 tmux pane 精确跳转（active pane 变为 agent 所在 pane）
- [ ] 5.3 回归测试：在裸终端（非 tmux）中运行 agent，点击行，验证只聚焦 Hyprland 窗口，无报错
- [ ] 5.4 多 pane 场景验证：agent 在 tmux session 的非活跃 window/pane 中，点击后确认 window 和 pane 均切换正确
