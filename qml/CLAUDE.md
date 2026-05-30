[根目录](../CLAUDE.md) > **qml**

# qml/ — QML 前端模块

Qt Quick (QML) 悬浮覆层 UI，渲染 Agent 状态并接收用户交互。

---

## 变更记录 (Changelog)

| 日期 | 变更 |
|------|------|
| 2026-05-30 | 初次生成模块文档 |

---

## 模块职责

- 以透明无边框窗口形式悬浮在所有应用之上
- 根据 `AgentModel.globalState` 动态切换视图（动画过渡）
- 显示 Agent 工作状态、当前步骤、context 用量、订阅用量
- 提供 Permission / Question / Plan 交互，将决策通过 `agentModel` Q_INVOKABLE 方法写回
- 支持拖拽移动、点击展开/收起
- 主题系统支持 midnight / aurora / carbon 三套配色，形状支持 round / pill / sharp

---

## 入口

**`qml/main.qml`** — 根 Window

- `Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint`
- 通过 `widthByState` 映射控制宽度（idle: 280, working: 380, permission/question: 420, plan: 460）
- 高度由 `animatedH` 属性驱动，通过 `NumberAnimation` 实现 280ms OutQuint 动效
- `Loader` 动态加载 body 组件（PermissionView / QuestionView / PlanView / ExpandedView）

---

## 组件一览

### `main.qml` — 主窗口

状态机驱动的窗口管理：

| pulseState | 宽度 | body Loader |
|------------|------|-------------|
| `idle` | 280 | 关闭（无 body） |
| `working` | 380 | 关闭（无 body） |
| `permission` | 420 | `PermissionView` |
| `question` | 420 | `QuestionView` |
| `plan` | 460 | `PlanView` |
| `expanded` | 同 backend state | `ExpandedView` |

高度动画防抖机制：
- `_stableBodyH` — body item 首次 layout 后的真实高度（避免 QML Column 延迟问题）
- `_holdH` — body 激活时保存的当前高度，防止 layout 前的抖动

### `HeaderBar.qml` — 标题栏（44px 高）

- 左侧：心跳点（pulsing ring 动画，颜色随状态变化：idle=green，working=cyan，其他=coral）
- 中间：标题"Pulse" + 副文本（idle 显示时钟，working 显示当前步骤，其他显示状态描述）
- 副文本变化时有 16ms opacity fade 动效
- 右侧：订阅用量指示（CC N% / CD N%，超 60% 显示 peach，超 85% 显示 coral）+ 状态 chip（idle chip 显示 agent 数量，working 显示三点动画，其他显示彩色状态标签）
- 支持拖拽：按下记录全局坐标，移动时更新 `dragWindow.x/y`；无拖拽视为点击 → 切换展开/收起

### `ExpandedView.qml` — 展开后的 Agent 列表

- 可滚动列表（`Flickable`），每行 62px
- 每行：左侧品牌图标（Claude Code / Codex）或字母 monogram，右侧 agent 名称 + session 名 + 当前步骤
- 底部 context 用量进度条（2px 高，>90% 显示 coral，>70% 显示 amber）
- 超出 280px 高时显示渐变遮罩 + "↓ N more" 提示
- 点击行 → `agentModel.focusAgent(index)` 切换终端焦点

### `PermissionView.qml` — 权限审批视图

- 展示 permissionTool（如 EDIT、BASH）、permissionTarget、permissionDesc
- 中间文本编辑框：用户可选填修改意见，Enter 确认
- Yes / No 按钮：调用 `agentModel.decidePermission` 或 `decidePermissionAmend`

### `QuestionView.qml` — 问答视图

- 显示 `agentData.questionPrompt`
- 选项列表用 `Repeater` 渲染，每项显示 Keycap（❖1, ❖2...）+ 选项文本
- hover 时有 2px 右移 translate 动效
- 点击 → `agentModel.answerQuestion(agentRow, index)`

### `PlanView.qml` — 计划审批视图

- 显示 planHtml（RichText 格式，最大高度 180px 可滚动）
- Comment / Approve 两个按钮
- Comment 按钮 hover 或有内容时显示文本输入框
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

`agentAccent(name)` — 基于 Agent 名称 hash 从 6 色调色板中确定性选色，保证同名 Agent 颜色稳定。

### `AgentRow.qml` — Agent 行组件（ExpandedView 未使用此文件，内联实现）

独立组件，展示单个 Agent 的名称、工具类型、当前步骤、状态点。

### `Keycap.qml` — 键帽 UI 组件

渲染类似键盘按键的小标签（如 `Esc`、`↵`、`Super+Space`），半透明背景 + 细边框。

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
