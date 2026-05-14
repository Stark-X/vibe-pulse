# Design: Tmux Pane Focus on Agent Row Click

## Architecture Overview

```
点击事件 (QML ExpandedView.qml)
    │
    ▼
agentModel.focusAgent(index)           [AgentModel.cpp]
    │
    ├─► HyprlandClient::focusWindow(windowAddress)   [sync, ≤100ms]
    │       └─► hyprctl dispatch focuswindow address:...
    │
    └─► QProcess::startDetached("tmux", ["switch-client",
            "-c", tmuxClientTty,        [async, fire-and-forget]
            "-t", tmuxTarget])
```

## Data Flow

```
enrichAgents() [main.cpp, 每 2 秒 or 文件变化触发]
    │
    ├─► ProcScanner: 扫描 /proc，识别 claude/codex/opencode 进程
    │
    ├─► ClaudeMetaReader / CodexMetaReader: 读取 session 元数据
    │
    ├─► HyprlandClient::clients(): 获取当前所有 Hyprland 窗口
    │
    ├─► HyprlandClient::findWindowAddress(pid, wins)
    │       如果空:
    │           ▼
    │       TmuxResolver::findPaneInfo(pid)   [NEW]
    │           │
    │           ├─► 检查 PPID 链是否含 tmux 进程
    │           ├─► tmux list-panes -a → 匹配 agent pane
    │           ├─► tmux list-clients -t sessionId → 选第一个有效 client
    │           └─► 返回 TmuxPaneInfo{terminalPid, tmuxTarget, clientTty, ...}
    │
    ├─► a.windowAddress = findWindowAddress(terminalPid, wins)
    ├─► a.tmuxTarget    = paneInfo.tmuxTarget      [NEW]
    └─► a.tmuxClientTty = paneInfo.clientTty        [NEW]
```

## File-by-File Changes

### src/TmuxResolver.h (重构)

```cpp
#pragma once
#include <QtGlobal>
#include <QString>
#include <optional>

struct TmuxPaneInfo {
    quint32 terminalPid = 0;
    QString sessionId;
    QString sessionName;
    int     windowIndex = -1;
    int     paneIndex   = -1;
    QString clientTty;
    QString tmuxTarget;   // format: "$N:w.p"
};

class TmuxResolver
{
public:
    static std::optional<TmuxPaneInfo> findPaneInfo(quint32 agentPid);
    // findTerminalPid() is removed; callers migrated to findPaneInfo()
};
```

### src/TmuxResolver.cpp (重构)

关键逻辑变化：
- `list-panes` 格式从 3 列扩展到 5 列（+windowIndex, +paneIndex）
- `list-clients` 格式从 1 列扩展到 2 列（+clientTty）
- pane 匹配时追踪 hop 距离，选最近祖先
- 返回完整 `TmuxPaneInfo` 而非 `quint32`

### src/AgentInfo.h (扩展)

```cpp
// 在 AgentInfo struct 中新增（紧跟 windowAddress）：
QString tmuxTarget;      // "$N:w.p" 或空
QString tmuxClientTty;   // "/dev/pts/N" 或空

// dataEquals() 追加：
&& tmuxTarget    == o.tmuxTarget
&& tmuxClientTty == o.tmuxClientTty
```

### src/AgentModel.cpp — focusAgent() (扩展)

```cpp
void AgentModel::focusAgent(int row)
{
    if (row < 0 || row >= m_agents.size())
        return;
    const AgentInfo &a = m_agents[row];
    if (!a.windowAddress.isEmpty())
        HyprlandClient::focusWindow(a.windowAddress);
    if (!a.tmuxTarget.isEmpty() && !a.tmuxClientTty.isEmpty()) {
        QProcess::startDetached(QStringLiteral("tmux"), {
            QStringLiteral("switch-client"),
            QStringLiteral("-c"), a.tmuxClientTty,
            QStringLiteral("-t"), a.tmuxTarget
        });
    }
}
```

**注意**: `data()`, `get()`, `roleNames()` 不暴露新字段（QML 不需要）。

### src/main.cpp — enrichAgents() (更新)

```cpp
// 原来:
if (a.windowAddress.isEmpty()) {
    if (auto tp = TmuxResolver::findTerminalPid(a.pid))
        a.windowAddress = HyprlandClient::findWindowAddress(*tp, wins);
}

// 改为:
if (a.windowAddress.isEmpty()) {
    if (auto pi = TmuxResolver::findPaneInfo(a.pid)) {
        a.windowAddress  = HyprlandClient::findWindowAddress(pi->terminalPid, wins);
        a.tmuxTarget     = pi->tmuxTarget;
        a.tmuxClientTty  = pi->clientTty;
    }
}
```

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| tmux target 使用 sessionId 还是 sessionName | sessionId（`$N`） | sessionName 可能含空格/冒号，tmux 解析时需要引号；sessionId 是稳定的机器友好标识符 |
| switch-client 同步 vs 异步 | 异步 (startDetached) | 不阻塞 UI 线程；失败无法恢复，不需要等待结果 |
| tmux 字段暴露到 QML 还是仅 C++ | 仅 C++ | QML 只需要 canJump 和点击触发 focusAgent，不需要 tmux 内部细节 |
| multi-client 选择策略 | 选 list-clients 中第一个有效 client | 无法从外部可靠地判断哪个终端是用户"当前在看的"，第一个是 tmux 自身的默认行为 |
| canJump 是否包含 tmux 感知 | 不包含，保持 `!windowAddress.isEmpty()` | 没有 Hyprland 窗口无法完成第一步 focus，tmux 跳转是 bonus |

## 时序图

```
User click
    │
    ▼
QML onClicked
    │
    └──► agentModel.focusAgent(idx)
              │
              ├──[sync]──► hyprctl dispatch focuswindow address:0xABC
              │              ↓ (Hyprland moves focus to terminal window)
              │
              └──[async]─► tmux switch-client -c /dev/pts/3 -t $1:2.1
                             ↓ (tmux changes active pane inside terminal)
```

两步操作在用户感知上是瞬时的（< 50ms 总延迟）。
