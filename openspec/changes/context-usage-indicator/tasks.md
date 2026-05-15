# Tasks: Context Usage Indicator

## Status: complete

---

## Task List

- [x] 1.1 AgentInfo.h — 追加 contextUsed/contextLimit 字段及 dataEquals 比较
- [x] 1.2 ClaudeMetaReader.h — ClaudeMeta struct 追加 contextUsed/contextLimit 字段
- [x] 1.3 ClaudeMetaReader.cpp — 添加 kClaudeContextLimit 常量和 parseContextUsage 函数
- [x] 1.4 ClaudeMetaReader.cpp — Working 分支 tail 8192→32768，调用 parseContextUsage
- [x] 1.5 ClaudeMetaReader.cpp — Waiting 分支在所有 early return 前调用 parseContextUsage
- [x] 2.1 AgentModel.h — Roles 枚举末尾追加 ContextUsedRole、ContextLimitRole
- [x] 2.2 AgentModel.cpp — roleNames() 追加两条映射
- [x] 2.3 AgentModel.cpp — data() switch 追加两个 case
- [x] 2.4 AgentModel.cpp — get() QVariantMap 追加两条 contextUsed/contextLimit
- [x] 3.1 main.cpp enrichAgents — Claude Code 分支追加 contextUsed/contextLimit 赋值
- [x] 3.2 main.cpp buildMockSnapshot — expanded 场景 Claude Code agent 注入 contextUsed=45000
- [x] 4.1 ExpandedView.qml — delegate 背景 Rectangle 内添加 context usage 进度条 Item
- [x] 5.1 验证 — make build 无编译错误
- [x] 5.2 验证 — make mock-expanded 进度条可见且颜色正确
- [x] 5.3 验证 — Codex 行无进度条（contextUsed=0 → visible: false）
