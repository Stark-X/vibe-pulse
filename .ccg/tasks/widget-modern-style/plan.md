# Plan: Widget UI Modernization

> Strategy: guided-develop | Phase: 4-plan | Source: agy analysis

## 需求

将 vibe-pulse widget dot/collapsed 模式（52×52px）更新为现代风格。
保持 hover-expand 行为，兼容 macOS + Linux，不新增 C++。

---

## agy 诊断（按影响排序）

1. **硬编码颜色绕过 Theme** — main.qml card 背景/边框直接写死颜色，与 Theme.qml 脱节，无法随主题同步
2. **高密度等宽字体** — 52px 圆内塞 "N/M" JetBrains Mono 12px，可读性差，视觉拥挤
3. **脉冲动画过于粗暴** — 环缩放 1→2.4 超出 card 边界，生硬不自然

## 推荐方案："Minimalist Breathing Aura"

- 背景/边框迁移到 `Theme.bg0` / `Theme.border`（与主题系统统一）
- Dot 内容：只显示 `workingCount`（有时才显示），隐藏 total count
- 脉冲动画：内核微呼吸（scale 1.0↔1.1，Easing.InOutQuad）+ 外光晕柔和扩散（1.0→1.8，Easing.OutExpo，opacity 0.35→0）

---

## 实施步骤

### Step 1 — `qml/main.qml`: 颜色 token 化
- `color: Qt.rgba(0.078, 0.075, 0.11, 0.93)` → `color: Theme.bg0`
- `border.color: "#3a384e"` → `border.color: Theme.border`
- **Why**: 主题一致性，动态切换 midnight/aurora/carbon 自动生效

### Step 2 — `qml/main.qml`: dot 内容简化
- 移除 "N/M" 双数字文本
- 改为：仅当 `workingCount > 0` 时显示单个 `workingCount`，字体换 `font.family: Theme.fontSans`（或 fallback 系统 sans-serif）
- 无 agent 工作时只显示状态色圆点，不显示数字
- **Why**: 减少视觉噪音，dot 模式下展示精华信息

### Step 3 — `qml/main.qml`: 动画升级
- 外光晕 ring：`scale` 1.0→1.8，duration 1800ms，`Easing.OutExpo`，opacity 0.35→0
- 内核：`scale` 1.0↔1.1，duration 2000ms，`Easing.InOutQuad`（微呼吸感）
- 动画 `running` 条件：`!isExpanded && agentModel.workingCount > 0`（空闲时停止，省 GPU）
- **Why**: 流畅有机感，消除超出边界的跳跃

### Step 4 — `qml/Theme.qml`: 可选
- 如 Step 2 需要 sans-serif token，添加 `readonly property string fontSans: "system-ui"`
- **Why**: 统一字体引用，平台自适应

---

## 影响范围

- 修改: `qml/main.qml`, `qml/Theme.qml`（可选）
- 新增: 无
- 测试: `make mock-idle`, `make mock-working`, `make mock-expanded` 视觉验证

---

## 风险

| 风险 | 缓解 |
|------|------|
| 无限动画持续唤醒 GPU | `running: !isExpanded && workingCount > 0` |
| macOS Metal vs Linux OpenGL 锯齿 | `antialiasing: true` on scaling items |
| 字体平台差异（CoreText vs FreeType） | `renderType: Text.QtRendering` + anchor 对齐 |
