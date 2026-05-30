# Pulse — AI Agent Monitor

Qt6/QML floating overlay for monitoring AI coding agents (Claude Code, Codex, OpenCode).
Supports macOS, Linux (Wayland/Hyprland), and Windows (stub).

---

## 变更记录 (Changelog)

| 日期 | 变更 |
|------|------|
| 2026-05-30 | 初次生成完整 CLAUDE.md；涵盖 src/、qml/、pulse-toggle/、openspec/ 全模块扫描结果 |

---

## 项目愿景

Pulse 是一个轻量级桌面悬浮覆层，实时显示当前运行的 AI 编码 Agent（Claude Code、Codex、OpenCode）的状态——正在工作、等待权限确认、提问、共享计划——并允许用户直接从覆层批准/拒绝操作，无需切换终端窗口。

---

## 架构总览

```
┌─────────────────────────────────────────────────────────────────┐
│  ProcScanner (2s 轮询)                                           │
│    ProcessTree (platform impl: linux/macos/win)                 │
│    → 发现 claude / codex 进程                                    │
└──────────────────────┬──────────────────────────────────────────┘
                       │ QVector<AgentInfo>
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  enrichAgents()                                                  │
│    ClaudeMetaReader → ~/.claude/sessions/<pid>.json             │
│                     → ~/.claude/projects/<enc-cwd>/<sid>.jsonl  │
│    CodexMetaReader  → ~/.codex/sessions/                        │
│    TmuxResolver     → tmux list-panes / list-clients            │
│    WindowManager    → Hyprland IPC / macOS NSRunningApplication │
└──────────────────────┬──────────────────────────────────────────┘
                       │ enriched AgentInfo
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  AgentModel (QAbstractListModel)                                 │
│    globalState: idle | working | permission | question | plan    │
│    Q_INVOKABLE: answerQuestion / approvePlan / decidePermission  │
└──────────────────────┬──────────────────────────────────────────┘
                       │ QML context property "agentModel"
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│  QML Engine                                                      │
│    main.qml (Window) → HeaderBar + Loader                       │
│    Loader:                                                       │
│      PermissionView | QuestionView | PlanView | ExpandedView    │
└─────────────────────────────────────────────────────────────────┘
```

### 状态机 (PulseState)

```
Idle ←→ Working ←→ Permission
                ←→ Question
                ←→ Plan
```

### IPC (pulse-toggle)

```
pulse-toggle toggle|show|hide  →  QLocalSocket("pulse-ipc")  →  main pulse process
```

---

## 模块结构图

```mermaid
graph TD
    ROOT["(根) vibe-pulse"] --> SRC["src/"];
    ROOT --> QML["qml/"];
    ROOT --> TOGGLE["pulse-toggle/"];
    ROOT --> OPENSPEC["openspec/"];
    ROOT --> PROTOCOLS["protocols/"];
    ROOT --> ASSETS["assets/"];

    SRC --> CORE["核心逻辑"];
    SRC --> PLATFORM["平台层"];
    SRC --> READERS["Agent 读取器"];

    CORE --> AgentInfo["AgentInfo.h\n(数据模型)"];
    CORE --> AgentModel["AgentModel.h/cpp\n(QAbstractListModel)"];
    CORE --> ProcScanner["ProcScanner.h/cpp\n(进程扫描)"];
    CORE --> ProcessTree["ProcessTree.h\n(跨平台进程树)"];
    CORE --> Settings["Settings.h/cpp\n(主题/形状持久化)"];
    CORE --> SubscriptionMonitor["SubscriptionMonitor\n(API 用量监控)"];
    CORE --> ResponseWriter["ResponseWriter\n(写回 Agent 决策)"];
    CORE --> MiniMd["MiniMd\n(Markdown→HTML)"];

    READERS --> ClaudeReader["ClaudeMetaReader\n(解析 JSONL)"];
    READERS --> CodexReader["CodexMetaReader"];

    PLATFORM --> WindowManager["WindowManager\n(抽象 WM 接口)"];
    PLATFORM --> Hyprland["HyprlandWindowManager\n(Linux)"];
    PLATFORM --> MacOS["MacOSWindowManager\n(macOS)"];
    PLATFORM --> Null["NullWindowManager\n(Windows/fallback)"];
    PLATFORM --> WaylandLS["WaylandLayerShell\n(Linux only)"];
    PLATFORM --> WindowOverlay["WindowOverlay\n(置顶层接口)"];

    QML --> MainQML["main.qml\n(根 Window)"];
    QML --> HeaderBar["HeaderBar.qml\n(标题栏/状态指示)"];
    QML --> ExpandedView["ExpandedView.qml\n(所有 Agent 列表)"];
    QML --> PermissionView["PermissionView.qml"];
    QML --> QuestionView["QuestionView.qml"];
    QML --> PlanView["PlanView.qml"];
    QML --> Theme["Theme.qml\n(Singleton 主题)"];

    TOGGLE --> ToggleMain["pulse-toggle/main.cpp\n(IPC 客户端)"];

    click SRC "./src/CLAUDE.md" "查看 src 模块文档"
    click QML "./qml/CLAUDE.md" "查看 qml 模块文档"
    click TOGGLE "./pulse-toggle/CLAUDE.md" "查看 pulse-toggle 文档"
    click OPENSPEC "./openspec/CLAUDE.md" "查看 openspec 文档"
```

---

## 模块索引

| 模块 | 路径 | 职责 |
|------|------|------|
| 核心 C++ 后端 | `src/` | 进程扫描、状态读取、数据模型、平台接口 |
| QML 前端 | `qml/` | 悬浮覆层 UI：标题栏、各交互视图、主题系统 |
| IPC 控制工具 | `pulse-toggle/` | 命令行客户端，发送 show/hide/toggle 指令 |
| 变更规格文档 | `openspec/changes/` | 各功能的设计/提案/任务追踪文档 |
| Wayland 协议 | `protocols/` | `wlr-layer-shell-unstable-v1.xml`（Wayland 层 shell 协议） |
| 静态资源 | `assets/` | Claude Code、Codex 品牌图标 |

---

## 运行与开发

### 前置依赖

| 平台 | 依赖 |
|------|------|
| 全平台 | Qt6 >= 6.4（Core / Gui / Qml / Quick / Network）、CMake >= 3.16、C++17 |
| Linux | `wayland-client`、`wayland-scanner`、Hyprland（可选，用于窗口跳转） |
| macOS | Xcode CLT、AppKit、Foundation（通过 Homebrew 安装的 Qt6） |
| Windows | MSVC 2022、psapi |

### 构建命令

```bash
make build        # cmake --build build[_mac|_win] --target pulse
make run          # build + 启动 pulse
make toggle       # build pulse-toggle + 发送 toggle 指令
make clean        # 删除 build 目录

# Mock 场景预览（不依赖真实 Agent 进程）
make mock-idle
make mock-working
make mock-permission
make mock-question
make mock-plan
make mock-expanded
make mock-subscription   # 模拟订阅用量 CC=45% CD=31%
```

### 环境变量

| 变量 | 说明 |
|------|------|
| `PULSE_MOCK=<scene>` | 注入静态 mock 快照（idle/working/permission/question/plan/expanded） |
| `PULSE_DEMO=1` | 演示模式：跳过 meta 读取，仅显示进程扫描结果 |
| `PULSE_MOCK_SUBSCRIPTION=CC=N,CD=N` | mock 订阅用量百分比 |

### macOS 特殊步骤

CMake 会自动通过 `brew --prefix qt` 查找 Qt。如果路径不同：
```bash
cmake -B build_mac -S . -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt -DCMAKE_BUILD_TYPE=Release
```

---

## 测试策略

目前无自动化测试（无 `tests/` 目录）。质量验证依赖：

1. **Mock 场景**：`make mock-<scene>` 覆盖所有 UI 状态，视觉验证动效和布局
2. **编译验证**：`make build` 确保无编译错误（C++ 静态类型检查）
3. **真实运行验证**：启动真实的 Claude Code / Codex 进程后运行 pulse 验证状态读取

---

## 编码规范

详见 `.context/prefs/coding-style.md`，核心规则：

- 函数 < 50 行，嵌套 ≤ 3 层
- 平台特定代码放 `_linux.cpp` / `_macos.cpp` / `_win.cpp` 文件，不用 `#ifdef` 污染共享文件
- Git 提交遵循 Conventional Commits + emoji 前缀
- 不记录 secrets（token、key、cookie 等）到日志
- 做决策时追加到 `.context/history/` 记录选择理由

---

## AI 使用指引

1. **修改代码前**必读 `.context/prefs/coding-style.md` 和 `.context/prefs/workflow.md`
2. **功能变更**遵循 openspec 流程：在 `openspec/changes/<feature>/` 下创建 proposal → design → tasks
3. **平台相关修改**：Linux 用 `ProcessTree_linux.cpp`，macOS 用 `ProcessTree_macos.cpp`，共享逻辑放 `ProcessTree.h` 接口
4. **QML 修改**：通过 `make mock-<scene>` 验证所有状态下的视觉效果
5. **Mock 场景**已覆盖所有 `PulseState` 枚举值，添加新状态时同步更新 `buildMockSnapshot()`
6. **IPC 格式**：`pulse-ipc` QLocalSocket，JSON payload `{"cmd": "toggle"|"show"|"hide"}`
