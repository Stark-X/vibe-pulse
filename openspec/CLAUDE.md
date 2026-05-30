[根目录](../CLAUDE.md) > **openspec**

# openspec/ — 变更规格文档

用 OpenSpec 管所有功能变更：提案、设计、规格、任务追踪。

---

## 变更记录 (Changelog)

| 日期 | 变更 |
|------|------|
| 2026-05-30 | 初次生成模块文档 |

---

## 模块职责

- 每变更独立子目录，含 proposal → design → specs → tasks 四层文档
- tasks.md 用 `- [x]` 追踪进度
- 完成后归档到 `changes/archive/`

---

## 已有变更一览

| 变更 | 状态 | 说明 |
|------|------|------|
| `cross-platform-support` | 基本完成（Phase 1–9 已完，10.5/10.6/11.9/11.10 待真机验证） | macOS + Windows 跨平台支持 |
| `agent-monitor-real-integration` | 历史文档（Tauri 时期规格，已完成） | 真实进程集成规格 |
| `agent-permission-inline-actions` | 待实现 | 权限操作内联到 Agent 行 |
| `agent-row-ui-enhancement` | 待实现 | Agent 行 UI 增强 |
| `context-usage-indicator` | 待实现 | Context 用量指示器 |
| `header-subscription-usage` | 待实现 | 标题栏订阅用量显示 |
| `tmux-pane-focus-on-click` | 待实现 | 点击 Agent 行聚焦 tmux pane |
| `pulse-subtitle-realtime-update` | 仅 proposal | 副标题实时更新 |
| `archive/fix-bg-agent-cwd` | 已归档 | 后台 Agent cwd 获取修复 |

---

## OpenSpec 工作流

命令在 `.claude/commands/opsx/`：

```bash
/propose <feature>   # 创建 proposal.md
/explore <feature>   # 探索设计方案，生成 design.md
/apply <feature>     # 执行实现任务（逐步执行 tasks.md）
/archive <feature>   # 归档到 changes/archive/
```

---

## 相关文件清单

```
openspec/
└── changes/
    ├── cross-platform-support/     macOS + Windows 跨平台（Phase 1-11）
    ├── agent-monitor-real-integration/  Tauri 时期集成规格（历史参考）
    ├── agent-permission-inline-actions/ 权限内联操作
    ├── agent-row-ui-enhancement/   Agent 行 UI 增强
    ├── context-usage-indicator/    Context 用量指示
    ├── header-subscription-usage/  订阅用量标题栏
    ├── tmux-pane-focus-on-click/  tmux pane 焦点
    ├── pulse-subtitle-realtime-update/ 副标题实时更新
    └── archive/
        └── fix-bg-agent-cwd/      已归档：cwd 修复
```