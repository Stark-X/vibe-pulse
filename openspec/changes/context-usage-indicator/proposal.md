# Proposal: Context Usage Indicator in Agent Session Rows

## Overview

在 `ExpandedView` 每个 agent 行底部增加一条 2px 细进度条，实时显示 Claude Code session 的 context token 使用量与模型上限的比例。

**目标**: 让用户在 agent 列表视图中直接感知 context 压力，无需展开会话查看详情。  
**平台**: Ubuntu 24.04 + Hyprland  
**技术栈**: C++/Qt6 backend + QML frontend

---

## Data Sources（已验证）

### Context 使用量来源

Transcript JSONL: `~/.claude/projects/<encoded-cwd>/<sessionId>.jsonl`

每个 `type == "assistant"` 行的 `message.usage` 字段：
```json
{
  "input_tokens": 1,
  "cache_creation_input_tokens": 171,
  "cache_read_input_tokens": 43832,
  "output_tokens": 80
}
```

**已验证**：`ClaudeMetaReader::read()` 在 Working 状态读取 8192 字节尾部，在 Waiting 状态读取 32768 字节尾部。context 使用量解析需从同一 tail 中提取。

**context_used 计算**：
```
context_used = input_tokens + cache_creation_input_tokens + cache_read_input_tokens
```

### 模型上限来源

同一 JSONL assistant 行的 `message.model` 字段（e.g. `"claude-sonnet-4-6"`）。

**上限映射**（所有当前 Claude 模型均为 200k）：
```
claude-*-*  → 200000 tokens（硬编码常量，无需动态查询）
```

---

## Discovered Constraints

### Hard Constraints（不可违反）

1. **仅 Claude Code 可用**：Codex 无 transcript 文件，`CodexMetaReader` 不读取 usage，`contextUsed` 对 Codex 保持 0。
2. **readTail 覆盖范围**：Working 状态 tail 为 8192 字节，Waiting 状态为 32768 字节。必须在现有 tail 中解析，不得额外读文件（避免 IO 开销）。
3. **dataEquals 需同步**：`AgentInfo::dataEquals()` 必须包含新字段，否则 model diff 无法检测变化，UI 不更新。
4. **AgentModel::Roles 枚举顺序**：只能在末尾追加，禁止插入（破坏现有 QML 绑定的数值映射）。
5. **行高不变**：`ExpandedView` 行高固定 62px，进度条叠加在 bottom separator 上方，不增加行高。

### Soft Constraints（约定/风格）

6. **颜色使用 Theme**：进度条颜色应引用 `Theme.accent`、`Theme.running`；警告色使用 `#f59e0b`（已有 `Theme.peach`），危险色使用 `Theme.coral`。
7. **QML 属性命名风格**：`camelCase`，与现有 `sessionBusy`、`currentStep` 等保持一致。
8. **C++ 命名风格**：`contextUsed`、`contextLimit`（int 类型，token 数）。
9. **进度条仅在有效数据时显示**：`contextUsed > 0 && contextLimit > 0` 才 visible。

### Dependencies（跨模块依赖顺序）

```
AgentInfo.h          ← 先加字段
    ↓
ClaudeMetaReader     ← 从 tail 解析 usage
    ↓
AgentModel           ← 加 Roles + data() + roleNames() + dataEquals
    ↓
ExpandedView.qml     ← 绑定 model.contextUsed / model.contextLimit
```

---

## Risks & Mitigations

| 风险 | 缓解 |
|------|------|
| readTail 8192 字节不包含最近 assistant 消息 | 扫描 tail 中所有行，取最后一个有 usage 字段的 assistant 消息；Working 状态 tail 若过小可适度增大（但优先复用现有值） |
| model 字段不存在（旧版本 transcript）| 缺失时 contextLimit 默认 200000，contextUsed 默认 0 |
| Codex session 显示空进度条 | `contextUsed == 0` 时 `visible: false`，对 Codex 完全隐藏 |
| 行底部进度条与 separator 重叠 | 进度条置于 separator 正上方（anchors.bottom 对齐 parent.bottom，y 偏移 -1） |

---

## Success Criteria（可验证）

1. `make mock-expanded` 场景下，Claude Code session 行底部可见一条细进度条。
2. 进度条宽度与 `contextUsed / contextLimit` 比例一致（肉眼可辨）。
3. `contextUsed == 0` 时进度条不可见（Codex 行无进度条）。
4. 进度 > 70% 时进度条变为 `#f59e0b`（黄色），> 90% 时变为 `#f87171`（红色），其余为 `Theme.accent`。
5. `make build` 无编译错误，无 QML 绑定警告。
6. `AgentInfo::dataEquals()` 包含 `contextUsed` 和 `contextLimit` 的比较，确保数据变化触发 UI 更新。

---

## User Confirmations

- **进度条位置**: ExpandedView 每行底部（原 separator 位置），2px 高度，`visible` 仅在 `contextUsed > 0`。
- **不增加行高**: 进度条叠加显示，不改变 62px 行高。
- **不修改 AgentRow.qml**: AgentRow 用于 HeaderBar 场景（单行 idle/working 显示），范围外。
