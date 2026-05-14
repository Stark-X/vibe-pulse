# Team Research: permission-ui-fixes

## 增强后的需求

修复 3 个交互界面（PermissionView / PlanView / QuestionView）的 UI 问题，同时重设计 PermissionView 的 amend 交互模式：

- **PermissionView**: 3 按钮（No/Amend/Yes）→ 2 按钮（No/Yes），hover 时在按钮下方展开可选输入框；有无文字均可提交，有文字调 `decidePermissionAmend`，无文字调 `decidePermission`；按钮点击 + 输入框回车均可提交
- **PlanView**: Comment 按钮 → 实现为 hover 展开输入框，调用 `commentPlan(row, text)` 后端方法（已存在）；保留 Approve 按钮
- **QuestionView**: `⌘` 符号改为 `Super+数字`

## 约束集

### 硬约束
- [HC-1] QML 组件使用 `Theme.*` 颜色系统，不允许硬编码颜色 — 来源：代码审查
- [HC-2] 后端方法签名固定：`decidePermission(row, bool)`、`decidePermissionAmend(row, bool, string)`、`approvePlan(row)`、`commentPlan(row, string)` — 来源：AgentModel.h
- [HC-3] `implicitHeight` 必须随内容动态变化，窗口自动 resize 由 main.qml 处理 — 来源：main.qml height 计算
- [HC-4] 展开/收起使用 `visible` + `Behavior on implicitHeight` 实现，不用 opacity-only（避免占位高度残留）— 来源：现有 amendMode 模式

### 软约束
- [SC-1] 按钮高度：主操作 36px，次操作可 30px — 来源：现有代码惯例
- [SC-2] 动画时长 100-120ms，easing OutQuad/OutCubic — 来源：现有 Behavior
- [SC-3] 输入框使用 JetBrains Mono 字体，pixelSize 11 — 来源：现有 amendInput

### 依赖关系
- [DEP-1] PermissionView → AgentModel.decidePermission / decidePermissionAmend
- [DEP-2] PlanView → AgentModel.commentPlan（已有但 QML 未调用）
- [DEP-3] main.qml Loader 高度 → 依赖子组件 implicitHeight 正确变化

## 成功判据
- [OK-1] PermissionView 只显示 No / Yes 两个按钮，hover 任一按钮时输入框滑出
- [OK-2] 输入框为空时点击按钮调 decidePermission，有文字时调 decidePermissionAmend
- [OK-3] 输入框内按 Enter 等效点击对应按钮
- [OK-4] PlanView Comment 按钮 hover 展开输入框，Enter/按钮提交调 commentPlan
- [OK-5] QuestionView 快捷键显示 "❖1"（Super）而非 "⌘1"
- [OK-6] 窗口高度随展开/收起平滑动画

## 开放问题（已解决）
- Q1: Comment 按钮如何处理？→ A: hover 展开输入框，调用已有 commentPlan 方法 → 约束：[HC-2]
- Q2: Linux 快捷键符号？→ A: Super+数字 → [SC-1] 改 Keycap key 为 "❖" + (index+1) 或 "⊞"+数字
- Q3: hover 输入框提交方式？→ A: 按钮点击 + 回车均可
