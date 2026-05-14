# Commit Decision History

> 此文件是 `commits.jsonl` 的人类可读视图，可由工具重生成。
> Canonical store: `commits.jsonl` (JSONL, append-only)

| Date | Context-Id | Commit | Summary | Decisions | Bugs | Risk |
|------|-----------|--------|---------|-----------|------|------|

## a1bf4daf — 2026-05-14 — feat(pulse): PulseState 交互系统 + QML 组件库 + 后端解析扩展

**Decisions**
- PulseState 在 C++ 端推导，QML 通过 agentModel.globalState(string) 消费
- 用户决策走 $XDG_RUNTIME_DIR/pulse/<sid>/<iid>.response.json sidecar 文件，原子写入无需终端依赖
- QML module 通过 qmldir + addImportPath 注册，Theme singleton 与多组件一起声明
- MiniMd 轻量 Markdown→HTML 转换器内嵌 C++，避免引入外部依赖

**Files**: 25 files, +1982/-109

## 9365c3f8 — 2026-05-14 — style(ui): 扁平化高亮 item + Header 无副标题时单行显示

**Decisions**
- AgentRow 高亮改为扁平色块，去除渐变/阴影，视觉更干净
- Header 无副标题时折叠为单行，减少空白浪费

**Files**: qml/AgentRow.qml, qml/main.qml
