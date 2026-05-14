# Pulse — Design Feature Implementation Plan

> Source: `vibe-islad-cross-platform/project/Pulse.html` (Claude Design handoff).
> Scope: 实现 3 个缺失视觉特性 — **agent asking (question)**、**plan review (plan)**、**idle 收缩胶囊**。同时为后续 `permission` / `expanded` 状态留好接口。
> Stack: Qt 6 + QML (no QtQuick.Controls) + C++17, wlr-layer-shell.

---

## 1. 任务类型

- [x] 后端 (C++ AgentInfo / Reader / Model 扩展) → codex
- [x] 前端 (QML 状态机 + 组件解耦 + 动效) → gemini
- [x] 全栈协同（数据 schema + IPC 写回）

---

## 2. 技术方案概览

### 2.1 数据模型扩展（C++）

引入 `PulseState` 枚举 + 结构化负载，**所有状态推导在 C++ 端完成**，QML 只消费状态与渲染。

```cpp
// src/AgentInfo.h
enum class PulseState : int {
    Idle       = 0,
    Working    = 1,
    Permission = 2,
    Question   = 3,
    Plan       = 4
    // Expanded 不放 backend，纯 UI 状态由 QML 维护
};
Q_ENUM_NS(PulseState)   // 注册到 meta，QML 可用名字访问

struct PermissionLine {
    QString kind;   // "add" | "del" | "context"
    QString text;
    int     n = 0;  // 行号
};

struct AgentInfo {
    // ... 原字段保留
    PulseState pulseState = PulseState::Idle;

    // Question
    QString     questionPrompt;
    QStringList questionOptions;

    // Plan
    QString     planTitle;
    QString     planMarkdown;  // 原文，便于 IPC 透传
    QString     planHtml;      // C++ 预渲染为简易 HTML（Text.RichText 用）

    // Permission
    QVector<PermissionLine> permissionDiff;
    QString permissionTool;     // "Edit" | "Write" | ...
    QString permissionTarget;   // 文件路径

    // 交互稳定 id（写回 sidecar 时使用，避免重复处理）
    QString interactionId;
};
```

`AgentInfo::dataEquals()` 需把所有新字段纳入比较 — 否则 `AgentModel::setSnapshot()` 不会触发 `dataChanged`，QML 不刷新。

### 2.2 全局状态裁决（AgentModel）

```cpp
// src/AgentModel.h 新增
Q_PROPERTY(QString globalState       READ globalState       NOTIFY globalStateChanged)
Q_PROPERTY(int     activeAgentRow    READ activeAgentRow    NOTIFY globalStateChanged)
Q_PROPERTY(bool    idleCollapsed     READ idleCollapsed     NOTIFY globalStateChanged)
Q_PROPERTY(int     idleCollapseDelay MEMBER m_idleDelayMs)

Q_INVOKABLE void answerQuestion   (int row, int optionIndex);
Q_INVOKABLE void approvePlan      (int row);
Q_INVOKABLE void commentPlan      (int row, const QString &comment);
Q_INVOKABLE void decidePermission (int row, bool allow);
```

优先级：`Permission > Question > Plan > Working > Idle`（同优先级按 pid 稳定排序取第一）。`Expanded` 由 QML 自己管理，不进 backend。

```cpp
static int priority(PulseState s) {
    switch (s) {
    case PulseState::Permission: return 50;
    case PulseState::Question:   return 40;
    case PulseState::Plan:       return 30;
    case PulseState::Working:    return 20;
    default:                     return 10;
    }
}

void AgentModel::recomputeGlobalState() {
    int bestRow = -1; int bestPrio = -1;
    for (int i = 0; i < m_agents.size(); ++i) {
        int p = priority(m_agents[i].pulseState);
        if (p > bestPrio) { bestPrio = p; bestRow = i; }
    }
    m_activeAgentRow = bestRow;
    PulseState s = bestRow < 0 ? PulseState::Idle : m_agents[bestRow].pulseState;
    m_globalState   = stateName(s);                  // "idle"/"working"/...
    // idleCollapsed：仅在 idle 且持续 >2.5s（或 count==0）后才收缩
    if (s == PulseState::Idle) {
        if (!m_idleSince.isValid()) m_idleSince.start();
        m_idleCollapsed = rowCount() == 0 || m_idleSince.elapsed() > m_idleDelayMs;
    } else {
        m_idleSince.invalidate();
        m_idleCollapsed = false;
    }
    emit globalStateChanged();
}
```

### 2.3 ClaudeMetaReader 扩展

`status == "waiting"` 时不再立即 return，而是 tail-scan transcript（32KB）找最近一次 assistant `tool_use`，按工具名分流到 Question / Plan / Permission：

```cpp
// 文件：src/ClaudeMetaReader.cpp 新增 lastClaudeInteraction()
struct Interaction {
    enum Kind { None, Question, Plan, Permission } kind = None;
    QString id;
    QString prompt;
    QStringList options;
    QString title;
    QString markdown;
    QString tool;       // for permission/edit
    QString target;
};

static Interaction lastClaudeInteraction(const QByteArray &tail) {
    auto lines = tail.split('\n');
    for (int i = lines.size() - 1; i >= 0; --i) {
        QJsonObject obj = parseLine(lines[i]);
        if (obj.value("type").toString() != "assistant") continue;

        QJsonArray content = obj["message"].toObject()["content"].toArray();
        for (int j = content.size() - 1; j >= 0; --j) {
            QJsonObject it = content[j].toObject();
            if (it["type"].toString() != "tool_use") continue;
            QString name = it["name"].toString();
            QJsonObject in = it["input"].toObject();
            QString id = it["id"].toString();

            if (name == "AskUserQuestion") {
                Interaction ix; ix.kind = Interaction::Question; ix.id = id;
                ix.prompt  = firstNonEmpty(in["question"], in["prompt"], in["message"]);
                ix.options = stringArray(in["options"]).isEmpty()
                           ? stringArray(in["choices"]) : stringArray(in["options"]);
                if (!ix.prompt.isEmpty()) return ix;
            }
            if (name == "ExitPlanMode") {
                Interaction ix; ix.kind = Interaction::Plan; ix.id = id;
                ix.markdown = firstNonEmpty(in["plan"], in["markdown"], in["content"]);
                ix.title    = firstNonEmpty(in["title"], QStringLiteral("Plan review"));
                if (!ix.markdown.isEmpty()) return ix;
            }
            if (name == "Edit" || name == "Write" || name == "MultiEdit") {
                Interaction ix; ix.kind = Interaction::Permission; ix.id = id;
                ix.tool   = name;
                ix.target = in["file_path"].toString();
                return ix;
            }
            // 第一个 tool_use 决定状态 → break
            return {};
        }
    }
    return {};
}
```

`read()` 内的 `if (status == "waiting")` 分支替换：

```cpp
if (status == "waiting") {
    m.currentStep = sobj["waitingFor"].toString();
    QByteArray tail = readTail(txPath, 32*1024, watchPaths);
    auto ix = lastClaudeInteraction(tail);
    switch (ix.kind) {
    case Interaction::Question:
        m.pulseState     = PulseState::Question;
        m.questionPrompt = ix.prompt;
        m.questionOptions= ix.options;
        m.interactionId  = ix.id;
        break;
    case Interaction::Plan:
        m.pulseState     = PulseState::Plan;
        m.planTitle      = ix.title;
        m.planMarkdown   = ix.markdown;
        m.planHtml       = miniMdToHtml(ix.markdown);
        m.interactionId  = ix.id;
        break;
    case Interaction::Permission:
        m.pulseState       = PulseState::Permission;
        m.permissionTool   = ix.tool;
        m.permissionTarget = ix.target;
        m.interactionId    = ix.id;
        break;
    default:
        m.pulseState = PulseState::Permission;  // 未知 waiting fallback
    }
    return m;
}
if (status == "idle") { m.pulseState = PulseState::Idle; return m; }

// busy 路径正常 tail-scan 找 currentStep，pulseState = Working
m.pulseState = PulseState::Working;
```

### 2.4 极简 Markdown → HTML 渲染（C++ 端，无外部依赖）

`Text { textFormat: Text.RichText }` 仅支持基础 HTML。我们手写约 60 行的极简渲染器：

```cpp
// src/MiniMd.cpp（新文件）
QString miniMdToHtml(const QString &src) {
    QStringList out; bool inUl = false;
    for (const QString &raw : src.split('\n')) {
        QString line = raw;
        // inline code: `xx` → <code>xx</code>
        static QRegularExpression rxCode("`([^`]+)`");
        line.replace(rxCode, "<code>\\1</code>");

        if (line.startsWith("# "))      { out << "<h4>" + line.mid(2)  + "</h4>";  closeUl(out, inUl); }
        else if (line.startsWith("## ")){ out << "<h4>" + line.mid(3)  + "</h4>";  closeUl(out, inUl); }
        else if (line.startsWith("- ")) { openUl(out, inUl); out << "<li>" + line.mid(2) + "</li>"; }
        else if (line.trimmed().isEmpty()){ closeUl(out, inUl); }
        else                            { closeUl(out, inUl); out << "<p>" + line + "</p>"; }
    }
    closeUl(out, inUl);
    return out.join('\n');
}
```

外部依赖：**零**。够用于 Plan View 的标题/段落/列表/`<code>` 渲染。

### 2.5 用户动作 IPC：sidecar 文件

GUI 进程无法拿到 Claude CLI 的 stdin；写终端有 tmux/pty 复杂度。**采用 sidecar response 文件**：

```
$XDG_RUNTIME_DIR/pulse/<sessionId>/<interactionId>.response.json
fallback: /tmp/pulse-<uid>/<sessionId>/<interactionId>.response.json
```

Payload：
```json
{ "type":"question_answer", "optionIndex":1, "answer":"option text", "ts": 1778660000 }
{ "type":"plan_decision",   "decision":"approve" }
{ "type":"plan_decision",   "decision":"comment", "comment":"please ..." }
{ "type":"permission_decision", "decision":"allow" }
```

写入用 `QSaveFile`（原子）。Claude Code 侧需要 hook/wrapper 读取该文件并把结果回填 turn — **本次只交付 Pulse 端写入和 UI**；agent 侧的 hook 安装作为后续工作。

### 2.6 QML 状态机重构

**当前**：`qml/main.qml` 是固定 `ListView`，width 440。
**重构**：状态机 + Loader 切换 body。

```qml
// qml/main.qml
Window {
    id: root
    visible: true; color: "transparent"
    flags:   Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    // 状态来源：backend 全局 state；本地可覆写为 expanded
    property string backendState: agentModel.globalState        // "idle" | "working" | ...
    property bool   userExpanded: false
    property string pulseState:   userExpanded ? "expanded" :
                                   agentModel.idleCollapsed ? "idle" : backendState

    readonly property var widthByState: ({
        "idle": 280, "working": 380, "permission": 440,
        "question": 420, "plan": 460, "expanded": 420
    })

    width:  widthByState[pulseState] ?? 380
    height: bodyVisible ? (header.height + body.implicitHeight + 24)
                        : (header.height + 8)
    readonly property bool bodyVisible: pulseState !== "idle"
                                      && pulseState !== "working"

    Behavior on width  { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }
    Behavior on height { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }

    Rectangle {
        id: card
        anchors.fill: parent
        color: Theme.bg2; border.color: Theme.borderStrong; border.width: 1
        radius: pulseState === "idle"
                ? (Theme.shape === "pill" ? height/2 : Theme.radius)
                : Theme.radius
        Behavior on radius { NumberAnimation { duration: 280 } }

        // 顶部 1px 高光线，模拟 backdrop-filter 玻璃感
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 12 }
            height: 1
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(1,1,1,0.08) }
                GradientStop { position: 1; color: "transparent" }
            }
        }

        HeaderBar {
            id: header
            anchors { top: parent.top; left: parent.left; right: parent.right }
            pulseState: root.pulseState
            onHeaderClicked: root.userExpanded = !root.userExpanded
        }

        Loader {
            id: body
            anchors { top: header.bottom; left: parent.left; right: parent.right
                      margins: 0 }
            active: root.bodyVisible
            sourceComponent: ({
                "permission": permComp,
                "question":   questComp,
                "plan":       planComp,
                "expanded":   expandedComp
            })[pulseState] ?? null
            opacity: active ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 180 } }
        }

        Component { id: permComp;     PermissionView { agentRow: agentModel.activeAgentRow } }
        Component { id: questComp;    QuestionView   { agentRow: agentModel.activeAgentRow } }
        Component { id: planComp;     PlanView       { agentRow: agentModel.activeAgentRow } }
        Component { id: expandedComp; ExpandedView   { /* 复用现有 ListView */ } }
    }
}
```

### 2.7 组件分解

| 文件 | 角色 | 实现要点 |
|------|------|---------|
| `qml/Theme.qml` | 设计 tokens | 扩展三主题；新增 bg-0..3, text-1..3, border, border-strong, violet/green/peach/coral, pulse-radius/pad |
| `qml/HeaderBar.qml` | **新建**，所有状态的头部 | 心跳点 + heartbeat ring 动画 + headline + sub + chip + dots loader（working 态显示） |
| `qml/QuestionView.qml` | **新建** | 顶部 prompt（text1 12.5px 500），下方 Repeater 渲染 options，每项 ⌘N chip + 文本，hover 时 `transform: Translate { x: 2 }`；Foot row "Claude Code · foot" 与 "dismiss Esc" |
| `qml/PlanView.qml` | **新建** | Row(violet chip + 副标题) + Flickable 包裹 `Text { textFormat: Text.RichText; text: model.planHtml }`，maxHeight 180；底部 [Comment] + [Approve plan ↵] |
| `qml/PermissionView.qml` | **新建** | 顶部 warn 三角 + tool/target；Repeater 渲染 diff 行（kind=add/del 着色）；[Deny Super+N] [Allow Super+Y] |
| `qml/ExpandedView.qml` | **新建**（从现有 main.qml 抽离） | 当前 `ListView` + `AgentRow` 的逻辑搬过来；按 status 分组（waiting / working / done） |
| `qml/AgentRow.qml` | 保留 | 微调 padding 让其在 expanded 中工作 |
| `qml/main.qml` | 重写 | 状态机驱动 width/height/body Loader |

### 2.8 动效细节

- **width/height/radius**：`Behavior on ... { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }` —— `OutQuint` 在 0–1 段拟合 `cubic-bezier(0.2, 0.8, 0.2, 1)` 已经很接近，肉眼难辨。
- **body slideDown**：`opacity` 由 0→1（180ms）+ `y` 由 -6→0（180ms） —— 用 `ParallelAnimation` on `onLoaded`。
- **heart pulse**：现有 `SequentialAnimation on opacity` 已有，扩展加一个 ring rect 用 `scale: 0.6→2.2`、`opacity: 0.8→0` 2s 循环。
- **dots loading**：3 个小圆点用 `SequentialAnimation`，间隔 200ms。

### 2.9 main.cpp 接入

```cpp
// 增加 widthChanged 监听（已有 heightChanged）：
QObject::connect(window, &QWindow::widthChanged, resizeTimer, [resizeTimer]{ resizeTimer->start(); });
// 在 resize 回调里：HyprlandClient::resizeWindow(*selfAddr, window->width(), qMin(window->height(), 600));
```

### 2.10 CMakeLists.txt

```cmake
qt_add_resources(pulse pulse_qml
    PREFIX "/"
    FILES
        qml/main.qml
        qml/Theme.qml
        qml/HeaderBar.qml
        qml/QuestionView.qml
        qml/PlanView.qml
        qml/PermissionView.qml
        qml/ExpandedView.qml
        qml/AgentRow.qml
        assets/claude-code.png
        assets/codex.png
)

target_sources(pulse PRIVATE
    src/MiniMd.h src/MiniMd.cpp
    src/ResponseWriter.h src/ResponseWriter.cpp
)
```

---

## 3. 实施步骤（Step-by-step）

| # | Step | 文件 | 预期产物 |
|---|------|------|---------|
| 1 | 扩展 `AgentInfo` 添加 `PulseState` 枚举 + 新字段 + 更新 `dataEquals` + `Q_DECLARE_METATYPE`（含 `PermissionLine`） | `src/AgentInfo.h` | 编译通过 |
| 2 | 扩展 `ClaudeMeta` 同步字段 | `src/ClaudeMetaReader.h` | 编译通过 |
| 3 | 实现 `lastClaudeInteraction()` + 改写 `waiting` 分支 + 设置 `pulseState`（busy→Working、idle→Idle） | `src/ClaudeMetaReader.cpp` | 单元逻辑验证（fixture JSONL） |
| 4 | 新增 `MiniMd` 极简渲染器 | `src/MiniMd.{h,cpp}` | 用 `qDebug()` 验证 3 类输入：h/p/ul/code |
| 5 | 新增 `ResponseWriter`（atomic `QSaveFile` 写 sidecar JSON） | `src/ResponseWriter.{h,cpp}` | 文件落地验证 |
| 6 | 扩展 `AgentModel`：新增 roles、`globalState`、`activeAgentRow`、`idleCollapsed`、`Q_INVOKABLE` 写回 | `src/AgentModel.{h,cpp}` | QML 看见属性 |
| 7 | `main.cpp` 在 enrichAgents() 中拷贝新字段；监听 `widthChanged` 触发 resize | `src/main.cpp` | 行为不退化 |
| 8 | 扩展 `Theme.qml`：新增 design tokens；三主题对齐 | `qml/Theme.qml` | QML 编译通过 |
| 9 | 新建 `HeaderBar.qml`（心跳点+heartbeat ring+headline+sub+chip+dots loader） | `qml/HeaderBar.qml` | 单独预览 OK |
| 10 | 重写 `qml/main.qml` 为状态机 + Loader 切换 + width/height/radius Behavior | `qml/main.qml` | 编译且动画顺滑 |
| 11 | 新建 `QuestionView.qml`（prompt + Repeater 选项 + foot row） | `qml/QuestionView.qml` | 点击触发 `agentModel.answerQuestion` |
| 12 | 新建 `PlanView.qml`（plan-head + Flickable RichText + actions） | `qml/PlanView.qml` | RichText 显示 mini-md |
| 13 | 新建 `PermissionView.qml`（diff + Deny/Allow） | `qml/PermissionView.qml` | 点击触发 `decidePermission` |
| 14 | 抽离 `ExpandedView.qml`（现有 ListView 逻辑） | `qml/ExpandedView.qml` | 与原 main.qml 同行为 |
| 15 | `CMakeLists.txt` 注册新源文件与 QRC 资源 | `CMakeLists.txt` | `cmake --build` 通过 |
| 16 | 注入 demo 数据：环境变量 `PULSE_DEMO_STATE=question/plan/idle` 时 ProcScanner 注入一个 fake agent | `src/ProcScanner.cpp` | 无 agent 时也能看 UI |
| 17 | `.context/history/commits.jsonl` 决策记录 + `bun run build`/`cmake --build build` 验证 | — | 零 warning 编译通过 |

---

## 4. 关键文件改动一览

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/AgentInfo.h` | 修改 | 加 PulseState 枚举 + 7 个新字段 + dataEquals 更新 |
| `src/ClaudeMetaReader.h` | 修改 | ClaudeMeta 同步加字段 |
| `src/ClaudeMetaReader.cpp` | 修改 | 新增 lastClaudeInteraction(); 重写 waiting 分支 |
| `src/CodexMetaReader.{h,cpp}` | 微改 | 仅设置 `pulseState = sessionBusy ? Working : Idle` |
| `src/AgentModel.{h,cpp}` | 修改 | 新 roles + globalState + recomputeGlobalState() + 4 个 Q_INVOKABLE |
| `src/main.cpp` | 修改 | enrichAgents 拷贝新字段；widthChanged 监听 |
| `src/MiniMd.{h,cpp}` | 新建 | ~60 行极简 md→html |
| `src/ResponseWriter.{h,cpp}` | 新建 | sidecar JSON 原子写 |
| `src/ProcScanner.cpp` | 微改 | demo state 注入（仅当 env 设置时） |
| `qml/Theme.qml` | 修改 | 扩展 design tokens（保留旧 key 别名以避免破坏 AgentRow） |
| `qml/main.qml` | 重写 | 状态机 + Loader |
| `qml/HeaderBar.qml` | 新建 | 公共头部 |
| `qml/QuestionView.qml` | 新建 | Question UI |
| `qml/PlanView.qml` | 新建 | Plan UI |
| `qml/PermissionView.qml` | 新建 | Permission UI（先简化 diff） |
| `qml/ExpandedView.qml` | 新建 | 现有列表逻辑迁移 |
| `qml/AgentRow.qml` | 微改 | 适配 ExpandedView 的 group section label |
| `CMakeLists.txt` | 修改 | 注册新源文件 + QRC |

---

## 5. 风险与缓解

| 风险 | 缓解 |
|------|------|
| Claude `ExitPlanMode` / `AskUserQuestion` 工具实际 input shape 不确定 | `firstNonEmpty(in["plan"], in["markdown"], in["content"])` 兜底；运行时用真实 session 校验后再调整 |
| sidecar 写回但 agent 侧不消费 → 决策被忽略 | 第一阶段只做 UI + 写文件，**明确告诉用户需后续装 hook**；UI 显示 "decision saved, awaiting agent ingest" |
| `Text.RichText` 对 OKLCH 色值不支持 | MiniMd 输出固定 hex/RGBA；颜色由 stylesheet 内联 |
| 状态频繁切换闪烁 | `idleCollapseDelayMs = 2500`，全局 state 计算只在 setSnapshot/recompute 时；动画 280ms |
| Hyprland resize 与 QML width Behavior 同时跑可能抖 | 用 `singleShot 80ms` 节流（已有 resizeTimer 50ms，可拉到 80）|
| QML enum 与 C++ enum 不同步 | C++ 端用 `Q_ENUM_NS(PulseState)`；QML 通过 `globalState` 字符串消费，避免数字绑定 |
| 现有 AgentRow 的 Theme key（textPrimary 等）会被新 token 名打破 | Theme.qml 同时保留旧别名 `textPrimary = text1` 等，做兼容 |

---

## 6. 验收标准

1. 启动 pulse，无 agent 运行时 → 2.5s 后收缩到 **280×40 的小胶囊**（heart 绿点 + "0 agents · all calm" + 时间）。
2. 运行 `claude` interactive，session.json 出现 `status: busy` → Pulse 切到 **working 380×40**，head 显示 dots + "writing X" / lastToolUse。
3. 模拟 `AskUserQuestion` tool_use（用 fixture 或环境变量 demo）→ Pulse 切到 **question 420**，body 显示 prompt + 选项；点击选项 → `$XDG_RUNTIME_DIR/pulse/<sid>/<iid>.response.json` 出现，内容含 `optionIndex`。
4. 模拟 `ExitPlanMode` → Pulse 切到 **plan 460**，body 显示 mini-md 渲染的 h4/p/ul/code；点击 Approve → response 文件落地，`decision:"approve"`。
5. 多 agent 并发：一个 working、一个 question → globalState=question，胶囊形态正确。
6. 全部退出后 2.5s → 回到 idle 胶囊。
7. `cmake --build build` 零 warning；切主题 midnight/aurora/carbon 立即生效。

---

## 7. 不在本次范围

- Claude Code 侧的 hook/wrapper 安装（消费 sidecar response 文件）。本次仅交付 Pulse 端 UI + 写回。
- Wayland 全局 keybind（Super+Y/Super+N/Super+J 在原型中显示，但需要 Hyprland config，作为后续 docs）。
- Permission state 的真实 diff 渲染（先用 "tool · target" 单行；diff 行渲染留接口，待 hook 投递结构化 diff 后启用）。

---

## 8. SESSION_ID（供 /ccg:execute resume 使用）

- **CODEX_SESSION**: `019e20ee-7170-77e1-9eb4-07280e272de7`
- **GEMINI_SESSION**: `889aa376-8bc7-4166-9488-468d451a4ec9`

---

## 9. 决策记录（待补到 `.context/history/`）

- **Decision**: `pulseState` 在 C++ 端推导，QML 仅消费
- **Alternatives**: 在 QML 解析 transcript（脆弱 + 性能差）
- **Reason**: 文本解析、tail-scan、JSON 解码都是 C++ 强项，且 QML 应保持视图纯净
- **Risk**: tool_use schema 变更时需同步改 C++ 解析器（用宽容字段名兜底）

- **Decision**: 用户决策走 sidecar 文件（不写 stdin/tmux）
- **Alternatives**: 写终端 stdin 通过 ptyspawn/tmux send-keys
- **Reason**: 安全可审计 + 跨平台一致 + 无 GUI 焦点依赖
- **Risk**: agent 侧需安装 hook 才能闭环；第一阶段仅写文件，UI 提示用户

- **Decision**: Mini-Markdown 自实现而非引入 md4c/cmark
- **Alternatives**: `md4c` C 库
- **Reason**: Plan 文本结构有限（h4/p/ul/inline code），自写 60 行可控；避免新增依赖与许可证审查
- **Risk**: 复杂 md（表格、代码块）无法渲染，超出本次范围
