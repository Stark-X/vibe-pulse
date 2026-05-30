[根目录](../CLAUDE.md) > **src**

# src/ — C++ 后端模块

Qt6 C++ 后端，负责进程扫描、Agent 状态读取、数据模型、平台窗口管理。

---

## 变更记录 (Changelog)

| 日期 | 变更 |
|------|------|
| 2026-05-30 | 初次生成模块文档 |

---

## 模块职责

- 每 2 秒轮询系统进程，识别 `claude` / `codex` 可执行文件
- 读取 `~/.claude/sessions/` JSON 和 `~/.claude/projects/*/` JSONL，解析当前工具调用状态
- 将状态映射到 `PulseState` 枚举，驱动 QML 视图切换
- 通过 `ResponseWriter` 把用户决策写回 Agent 的 session 文件（问答、计划审批、权限决定）
- 平台窗口管理（Hyprland IPC / macOS NSRunningApplication / NullWM）

---

## 入口与启动

**`src/main.cpp`** — 应用入口

关键启动流程：
1. 创建 `WindowManager`（工厂方法，自动检测平台）
2. 创建 `AgentModel`、`Settings`、`ProcScanner`、`SubscriptionMonitor`
3. 设置 `QFileSystemWatcher`（监视 `~/.claude/sessions/` + 活跃 JSONL）
4. 加载 `qml/main.qml`，注入 context properties
5. `WindowOverlay::create()->setup(window)` 设置窗口置顶层
6. 启动 `QLocalServer("pulse-ipc")` 监听 toggle/show/hide 指令
7. `scanner->start()` 开始轮询

---

## 数据模型

### AgentInfo（`AgentInfo.h`）

核心数据结构，每个 Agent 一个实例：

| 字段 | 类型 | 说明 |
|------|------|------|
| `name` | `QString` | Agent 工作目录 basename |
| `toolType` | `QString` | `"Claude Code"` / `"Codex"` / `"OpenCode"` |
| `pid` | `quint32` | 进程 PID |
| `pulseState` | `PulseState` | Idle / Working / Permission / Question / Plan |
| `sessionId` | `QString` | Claude session UUID |
| `currentStep` | `QString` | 当前工具调用描述（如 `"Edit · src/foo.cpp"`） |
| `contextUsed` | `int` | Claude context 已用 token 数 |
| `contextLimit` | `int` | Claude context 上限（默认 200000） |
| `questionPrompt` | `QString` | 待回答的问题文本 |
| `questionOptions` | `QStringList` | 选项列表 |
| `planTitle` | `QString` | 计划标题 |
| `planHtml` | `QString` | Markdown 转换后的 HTML |
| `permissionTool` | `QString` | 需要授权的工具名（Edit/Bash/Write） |
| `permissionTarget` | `QString` | 操作目标（文件路径或命令） |
| `interactionId` | `QString` | 工具调用 ID，用于写回 |
| `windowAddress` | `QString` | 窗口 ID（Hyprland 地址或 macOS PID 字符串） |
| `tmuxTarget` | `QString` | tmux 目标格式 `$N:win.pane` |

### PulseState 枚举

```
Idle = 0      → 无活动（session.status == "idle"）
Working = 1   → 工具调用中（session.status == "running"）
Permission = 2 → 等待权限（session.status == "waiting" + Edit/Bash/Write tool）
Question = 3  → 等待问答（session.status == "waiting" + AskUserQuestion tool）
Plan = 4      → 等待计划审批（session.status == "waiting" + ExitPlanMode tool）
```

---

## 对外接口（QML 可调用）

`AgentModel` 暴露给 QML 的 Q_INVOKABLE 方法：

```cpp
QVariantMap get(int row)                                 // 获取 Agent 数据
bool answerQuestion(int row, int optionIndex)            // 回答 Agent 问题
bool approvePlan(int row)                                // 批准计划
bool commentPlan(int row, const QString &comment)        // 评论计划
bool decidePermission(int row, bool allow)               // 授权/拒绝
bool decidePermissionAmend(int row, bool allow, const QString &amendment) // 带修改意见授权
void focusAgent(int index)                               // 切换终端窗口焦点
```

QML context properties（由 `main.cpp` 注入）：

| 名称 | 类型 | 说明 |
|------|------|------|
| `agentModel` | `AgentModel*` | Agent 列表模型 |
| `appSettings` | `Settings*` | 主题/形状设置 |
| `subscriptionMonitor` | `SubscriptionMonitor*` | API 用量数据 |
| `mockForceExpanded` | `bool` | mock expanded 场景时为 true |

---

## 关键组件

### ProcScanner（`ProcScanner.h/cpp`）

- 静态方法 `scanAll()` 枚举所有 PID，过滤 comm/exe 匹配 `claude`/`codex`/`opencode`
- 启动后每 2 秒发 `snapshotReady` 信号
- 使用 `ProcessTree` 获取跨平台进程信息

### ProcessTree（`ProcessTree.h`，实现分平台）

| 文件 | 平台 | 实现方式 |
|------|------|----------|
| `ProcessTree_linux.cpp` | Linux | 读取 `/proc/<pid>/` 目录 |
| `ProcessTree_macos.cpp` | macOS | `libproc`：`proc_pidinfo` / `proc_pidpath` |
| `ProcessTree_win.cpp` | Windows | `CreateToolhelp32Snapshot` + `QueryFullProcessImageName` |

静态方法：`comm()` / `exe()` / `cwd()` / `cmdline()` / `ppid()` / `ancestorChain()` / `listAll()` / `openFileMatching()`

### ClaudeMetaReader（`ClaudeMetaReader.h/cpp`）

读取 `~/.claude/sessions/<pid>.json` 或按 cwd 匹配最新 session 文件，再读对应 JSONL：
- 解析 `waitingFor` 字段判断 session 状态
- 从 JSONL 末尾 32KB 提取最后一条 `assistant` 消息，识别工具调用类型
- 支持的工具调用：`AskUserQuestion` → Question、`ExitPlanMode` → Plan、`Edit/Write/MultiEdit/Bash` → Permission
- 解析 context usage（`input_tokens + cache_*`）

### ResponseWriter（`ResponseWriter.h/cpp`）

写回 Agent 决策，文件路径 `~/.claude/projects/<enc-cwd>/<sessionId>.jsonl`（通过 `user` 类型行追加）：
- `writeQuestionAnswer(sessionId, interactionId, optionIndex, answer)`
- `writePlanDecision(sessionId, interactionId, decision, comment)`
- `writePermissionDecision(sessionId, interactionId, allow, amendment)`

### WindowManager 继承体系

```
WindowManager (abstract)
├── HyprlandWindowManager    Linux: hyprctl IPC，findWindowByPid + resizeWindow
├── MacOSWindowManager       macOS: NSRunningApplication activate，PID 作为 window ID
└── NullWindowManager        fallback，no-op（Windows / 非 Hyprland Linux）
```

工厂方法 `WindowManager::create()` 在运行时检测平台，选择实现。

### WaylandLayerShell（`WaylandLayerShell.h/cpp`，仅 Linux）

实现 `wlr-layer-shell-unstable-v1` 协议，将 pulse 窗口挂载到 Wayland compositor 的 overlay 层，保持全局可见（不被其他窗口遮挡）。

### TmuxResolver（`TmuxResolver.h` + `TmuxResolver.cpp`）

当 Agent 运行在 tmux pane 中时，通过 `tmux list-panes -a` 找到 pane PID，再追溯终端模拟器 PID，最终映射到 WindowManager 可识别的窗口地址。

### SubscriptionMonitor（`SubscriptionMonitor.h/cpp`）

定时轮询 Claude / Codex API，获取订阅用量百分比，暴露给 HeaderBar 显示：
- `claudeUtilization`: 0.0–100.0
- `codexUtilization`: 0.0–100.0
- 支持 `PULSE_MOCK_SUBSCRIPTION=CC=N,CD=N` 环境变量 mock

### MiniMd（`MiniMd.h/cpp`）

轻量 Markdown → HTML 转换器，用于 `PlanView` 渲染计划内容（无依赖外部库）。

---

## 平台文件一览

| 文件 | 平台 | 功能 |
|------|------|------|
| `ProcessTree_linux.cpp` | Linux | `/proc` 进程树读取 |
| `ProcessTree_macos.cpp` | macOS | libproc API |
| `ProcessTree_win.cpp` | Windows | Win32 Toolhelp32 |
| `WindowOverlay_linux.cpp` | Linux | WaylandLayerShell 包装 |
| `WindowOverlay_macos.mm` | macOS | NSWindow level + 右上角定位 |
| `WindowOverlay_win.cpp` | Windows | HWND TOPMOST + WS_EX_TOOLWINDOW |
| `HyprlandClient.h/cpp` | Linux | hyprctl JSON IPC 解析 |
| `HyprlandWindowManager.h/cpp` | Linux | Hyprland 窗口操作 |
| `MacOSWindowManager.h/mm` | macOS | NSRunningApplication 激活 |
| `WaylandLayerShell.h/cpp` | Linux | wlr-layer-shell 协议 |
| `TmuxResolver_win.cpp` | Windows | WSL tmux best-effort 代理 |

---

## 相关文件清单

```
src/
├── main.cpp                    应用入口
├── AgentInfo.h                 PulseState + AgentInfo 数据结构
├── AgentModel.h/cpp            QAbstractListModel，QML 数据源
├── ProcScanner.h/cpp           进程扫描轮询
├── ProcessTree.h               跨平台进程信息接口
├── ProcessTree_{linux,macos,win}.cpp  平台实现
├── ClaudeMetaReader.h/cpp      Claude session/JSONL 解析
├── CodexMetaReader.h/cpp       Codex session 解析
├── TmuxResolver.h / TmuxResolver.cpp / TmuxResolver_win.cpp
├── ResponseWriter.h/cpp        写回 Agent 决策
├── MiniMd.h/cpp                Markdown→HTML
├── Settings.h/cpp              主题/形状持久化（QSettings）
├── SubscriptionMonitor.h/cpp   API 用量监控
├── WindowManager.h/cpp         抽象 WM + 工厂方法
├── NullWindowManager.h         no-op WM
├── HyprlandWindowManager.h/cpp Hyprland IPC
├── HyprlandClient.h/cpp        hyprctl 通信
├── MacOSWindowManager.h/mm     macOS 窗口激活
├── WaylandLayerShell.h/cpp     Wayland layer-shell（Linux）
├── WindowOverlay.h             窗口置顶层接口
├── WindowOverlay_{linux,macos,win}.{cpp,mm}
└── LayerShell.h/cpp            LayerShell 基础封装（Linux）
```
