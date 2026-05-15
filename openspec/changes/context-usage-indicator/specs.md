# Specs: Context Usage Indicator

## FR-01: Context Token 数据提取

**行为**: `ClaudeMetaReader::read()` 必须在所有 PulseState 分支（Working、Waiting 含 Question/Plan/Permission 及 fallback）末尾填充 `ClaudeMeta.contextUsed` 和 `ClaudeMeta.contextLimit`。Idle 状态不读 transcript，两个字段保持 0。

**规则**:
- `contextUsed = input_tokens + cache_creation_input_tokens + cache_read_input_tokens`（来自最近一个有效 assistant 行的 `message.usage`）
- `contextLimit` 固定为 `200000`（`constexpr int kClaudeContextLimit = 200000`，定义一处，不重复）
- 若 tail 中无有效 usage 行，两字段均为 0
- Codex session 不调用解析，保持 0

**输入**: `QByteArray tail`（32768 字节 tail，含 Working 和 Waiting 两种状态）

**输出**: `ContextUsage { int used; int limit; }`

---

## FR-02: Tail 大小统一

**行为**: `ClaudeMetaReader::read()` 中 Working 分支的 `readTail` 调用必须从 8192 增至 **32768**，与 Waiting 分支一致。

**依据**: codex 实测，当前 busy session 最后 assistant usage 行距 EOF 8757 字节，8192 直接 miss。全量样本 p99 约 13306 字节，p95 约 4487 字节，32768 能覆盖绝大多数情况。

---

## FR-03: parseContextUsage 函数规格

**位置**: `ClaudeMetaReader.cpp` 文件内匿名 namespace，不暴露到 `.h`

**签名**:
```cpp
namespace {
struct ContextUsage { int used = 0; int limit = 0; };
ContextUsage parseContextUsage(const QByteArray &tail);
}
```

**算法**:
1. 按 `'\n'` split tail，反向扫描
2. 跳过空行和 JSON 解析失败的行
3. 要求 `obj["type"].toString() == "assistant"`
4. 取 `obj["message"].toObject()["usage"].toObject()`
5. 若 usage 对象为空则继续向前扫描
6. `used = input_tokens + cache_creation_input_tokens + cache_read_input_tokens`（用 `qint64` 累加防溢出，最终 `clamp` 到 `int`）
7. `limit = kClaudeContextLimit`（200000）
8. 找到第一条有效 usage 立即返回；全部扫描完无结果则返回 `{0, 0}`

**不变量**: 函数是 pure transform，无副作用，不读文件，不抛异常。

---

## FR-04: AgentInfo 字段扩展

**行为**: `AgentInfo` struct 追加两个字段，`dataEquals()` 必须同步包含。

**修改**:
```cpp
// 在 AgentInfo struct 末尾追加
int contextUsed  = 0;
int contextLimit = 0;

// dataEquals() 末尾追加条件
&& contextUsed  == o.contextUsed
&& contextLimit == o.contextLimit
```

**不变量**: 若 `contextUsed` 发生变化，`dataEquals()` 返回 false，触发 `dataChanged` 信号。

---

## FR-05: AgentModel Roles 扩展

**行为**: 在 `Roles` 枚举 `InteractionIdRole` 之后追加两个新 Role，同步 `roleNames()`、`data()`、`get()`。

**枚举追加**:
```cpp
ContextUsedRole,   // "contextUsed"
ContextLimitRole,  // "contextLimit"
```

**规则**: 只能末尾追加，禁止在现有枚举中间插入（破坏 Qt 模型角色映射）。

---

## FR-06: enrichAgents 同步

**行为**: `main.cpp` 的 `enrichAgents()` 函数 Claude Code 分支追加两行：
```cpp
a.contextUsed  = m.contextUsed;
a.contextLimit = m.contextLimit;
```

放在 `a.interactionId = m.interactionId;` 之后。

---

## FR-07: Mock 数据扩展

**行为**: `buildMockSnapshot()` 的 `"expanded"` 场景中，Claude Code agent 应注入代表性 `contextUsed` 值用于开发预览。

```cpp
// vibe-island (Claude Code, Working)
a.contextUsed  = 45000;
a.contextLimit = 200000;

// deepbank-fe (Claude Code, Idle)
a.contextUsed  = 0;  // 保持 0，进度条不可见
a.contextLimit = 0;
```

---

## FR-08: ExpandedView 进度条

**行为**: 每个 delegate 的背景 Rectangle 内，在 bottom separator 之后（更高 z-order）添加 context usage 进度条。

**QML 结构**:
```qml
// 容器：全宽占位，仅在有数据时可见
Item {
    visible: model.contextUsed > 0 && model.contextLimit > 0
    anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
    height: 2
    
    readonly property real usageRatio: 
        model.contextLimit > 0 ? Math.min(model.contextUsed / model.contextLimit, 1.0) : 0
    
    // 实际进度填充条
    Rectangle {
        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
        width: parent.width * parent.usageRatio
        color: parent.usageRatio > 0.9 ? Theme.coral
             : parent.usageRatio > 0.7 ? "#f59e0b"
             : Theme.accent
        opacity: 0.7
        Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
        Behavior on color { ColorAnimation  { duration: 300 } }
    }
}
```

**规则**:
- height: 2（固定，slim）
- 放置在背景 Rectangle 内 bottom separator Rectangle 之后
- 无 MouseArea（不拦截行点击事件）
- Codex 行：contextUsed = 0 → visible: false，完全不渲染
- 颜色阈值：> 90% → `Theme.coral (#f87171)`，> 70% → `#f59e0b`，其余 → `Theme.accent`
- 不增加行高（62px 不变），视觉叠加在 separator 上方

---

## Non-Functional Requirements

- **IO**: 不额外读取文件。parseContextUsage 复用现有 readTail 结果，Working 状态 tail 从 8192 提升至 32768。
- **性能**: 所有操作在 Qt 主线程，无线程安全问题（当前架构确认）。
- **向后兼容**: 旧 transcript 无 usage 字段时，contextUsed/contextLimit 保持 0，进度条隐藏，无功能退化。
