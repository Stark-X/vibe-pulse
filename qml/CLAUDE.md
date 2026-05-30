[根目录](../CLAUDE.md) > **qml**

# qml/ — QML 前端模块

Qt Quick (QML) 悬浮覆层 UI，渲染 Agent 状态+接收用户交互。

---

## 变更记录 (Changelog)

| 日期 | 变更 |
|------|------|
| 2026-05-30 | 初次生成模块文档 |

---

## 模块职责

- 透明无边框窗口悬浮所有应用之上
- 按 `AgentModel.globalState` 动态切换视图（动画过渡）
- 显示 Agent 工作状态、当前步骤、context 用量、订阅用量
- Permission / Question / Plan 交互，决策经 `agentModel` Q_INVOKABLE 写回
- 支持拖拽移动、点击展开/收起
- 主题支持 midnight / aurora / carbon 三套配色，形状支持 round / pill / sharp

---

## 入口

**`qml/main.qml`** — 根 Window

- `Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint`
- `widthByState` 映射宽度（idle: 280, working: 380, permission/question: 420, plan: 460）
- 高度由 `animatedH` 驱动，`NumberAnimation` 实现 280ms OutQuint 动效
- `Loader` 动态加载 body 组件（PermissionView / QuestionView / PlanView / ExpandedView）

---

## 组件一览

### `main.qml` — 主窗口

状态机驱动窗口管理：

| pulseState | 宽度 | body Loader |
|------------|------|-------------|
| `idle` | 280 | 关闭（无 body） |
| `working` | 380 | 关闭（无 body） |
| `permission` | 420 | `PermissionView` |
| `question` | 420 | `QuestionView` |
| `plan` | 460 | `PlanView` |
| `expanded` | 同 backend state | `ExpandedView` |

高度动画防抖：
- `_stableBodyH` — body item 首次 layout 后真实高度（避免 QML Column 延迟问题）
- `_holdH` — body 激活时保存当前高度，防 layout 前抖动

### `HeaderBar.qml` — 标题栏（44px 高）

- 左：心跳点（pulsing ring，颜色随状态：idle=green，working=cyan，其他=coral）
- 中：标题"Pulse" + 副文本（idle 显时钟，working 显当前步骤，其他显状态描述）；副文本变化时 16ms opacity fade
- 右：订阅用量（CC N% / CD N%，>60% peach，>85% coral）+ 状态 chip（idle 显 agent 数量，working 三点动画，其他彩色状态标签）
- 拖拽：按下记录全局坐标，移动更新 `dragWindow.x/y`；无拖拽视为点击 → 切换展开/收起

### `ExpandedView.qml` — 展开 Agent 列表

- 可滚动列表（`Flickable`），每行 62px
- 每行：左侧品牌图标（Claude Code / Codex）或字母 monogram，右侧名称 + session 名 + 当前步骤
- 底部 context 用量进度条（2px，>90% coral，>70% amber）
- 超 280px 高显渐变遮罩 + "↓ N more" 提示
- 点击行 → `agentModel.focusAgent(index)` 切换终端焦点

### `PermissionView.qml` — 权限审批视图

- 展示 permissionTool（如 EDIT、BASH）、permissionTarget、permissionDesc
- 中间文本编辑框：用户可选填修改意见，Enter 确认
- Yes / No 按钮：调用 `agentModel.decidePermission` 或 `decidePermissionAmend`

### `QuestionView.qml` — 问答视图

- 显示 `agentData.questionPrompt`
- `Repeater` 渲染选项列表，每项显示 Keycap（❖1, ❖2...）+ 选项文本
- hover 时 2px 右移 translate 动效
- 点击 → `agentModel.answerQuestion(agentRow, index)`

### `PlanView.qml` — 计划审批视图

- 显示 planHtml（RichText，最大高度 180px 可滚动）
- Comment / Approve 两按钮
- Comment hover 或有内容时显文本输入框
- Approve → `agentModel.approvePlan(agentRow)`
- Comment → `agentModel.commentPlan(agentRow, text)`

### `Theme.qml` — 主题 Singleton

```
import qml 1.0  // 通过 qmldir 注册为 singleton
```

颜色系统：

| Token | midnight | aurora | carbon |
|-------|----------|--------|--------|
| text1 | `#ebe7f7` | `#f0e8ff` | `#e0e0e0` |
| accent | `#7c8cff` | `#c084fc` | `#a0a0a0` |
| running | `#10b981` | `#34d399` | `#60c060` |

固定语义色：
- `violet`: `#c4a0ff`（问答/计划）
- `green`: `#10b981`（确认/running）
- `peach`: `#fbc19d`（权限/警告）
- `coral`: `#f87171`（拒绝/错误）

形状：
- `round`（默认）: radius = 14
- `pill`: radius = 999
- `sharp`: radius = 4

`agentAccent(name)` — 基于 Agent 名 hash 从 6 色调色板确定性选色，同名 Agent 颜色稳定。

### `AgentRow.qml` — Agent 行组件（ExpandedView 未使用，内联实现）

独立组件，展示单个 Agent 名称、工具类型、当前步骤、状态点。

### `Keycap.qml` — 键帽 UI 组件

渲染键盘按键小标签（如 `Esc`、`↵`、`Super+Space`），半透明背景 + 细边框。

---

## QML 与 C++ 通信

| 方向 | 机制 |
|------|------|
| C++ → QML | `agentModel`（QAbstractListModel roles）、`appSettings`（Q_PROPERTY）、`subscriptionMonitor`（Q_PROPERTY） |
| QML → C++ | `agentModel.answerQuestion()` 等 Q_INVOKABLE |
| 设置持久化 | `appSettings.setTheme(v)` / `appSettings.setShape(v)` → QSettings |
| 主题同步 | `Connections { target: appSettings; onThemeChanged: Theme.name = appSettings.theme }` |

---

## 相关文件清单

```
qml/
├── main.qml          根 Window，状态机，动画控制
├── HeaderBar.qml     标题栏（心跳点 + 状态文字 + 用量指示）
├── ExpandedView.qml  全 Agent 列表，可滚动
├── PermissionView.qml 权限审批（tool + target + Yes/No）
├── QuestionView.qml  问答（prompt + 选项列表）
├── PlanView.qml      计划审批（HTML 预览 + Comment/Approve）
├── AgentRow.qml      单行 Agent 组件（心跳点 + 名称 + 步骤）
├── Theme.qml         颜色/形状 Singleton
├── Keycap.qml        键帽 UI 组件
└── qmldir            模块声明（注册 Theme 为 Singleton）
```