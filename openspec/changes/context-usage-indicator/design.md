# Design: Context Usage Indicator

## 架构视图

```
ProcScanner::scanAll()
    → enrichAgents()
        → ClaudeMetaReader::read(pid, cwd)
              ├── readTail(txPath, 32768)  [Working: 8192→32768]
              ├── parseContextUsage(tail)  → ContextUsage{used, limit}
              ├── lastToolUse(tail) / lastClaudeInteraction(tail)
              └── return ClaudeMeta{..., contextUsed, contextLimit}
        → a.contextUsed = m.contextUsed
        → a.contextLimit = m.contextLimit
    → AgentModel::setSnapshot(agents)
         └── dataEquals() checks contextUsed + contextLimit
         └── emit dataChanged() if changed

QML: ExpandedView delegate
    └── model.contextUsed, model.contextLimit
    └── Item (height:2, bottom-anchored, visible when used>0)
         └── Rectangle (width: parent.width * ratio, color by ratio)
```

## 数据流详图

### Working 状态（8192 → 32768）

```
ClaudeMetaReader::read()
  status == "busy" (non-idle, non-waiting)
  ↓
  tail = readTail(txPath, 32768, watchPaths)  // 增大
  ↓
  ContextUsage cu = parseContextUsage(tail)    // 新增
  m.contextUsed  = cu.used
  m.contextLimit = cu.limit
  ↓
  m.currentStep = lastToolUse(tail)
  m.pulseState  = PulseState::Working
  return m
```

### Waiting 状态（含所有 early returns）

```
ClaudeMetaReader::read()
  status == "waiting"
  ↓
  tail = readTail(txPath, 32768, watchPaths)  // 已有，不变
  ↓
  ContextUsage cu = parseContextUsage(tail)    // 新增，在所有 return 之前
  m.contextUsed  = cu.used
  m.contextLimit = cu.limit
  ↓
  ix = lastClaudeInteraction(tail)
  ↓
  if Question → return m  [contextUsed 已填充]
  if Plan     → return m  [contextUsed 已填充]
  if Permission → return m [contextUsed 已填充]
  fallback    → return m  [contextUsed 已填充]
```

### Idle 状态

```
  status == "idle"
  m.pulseState = Idle
  return m    // contextUsed=0, contextLimit=0，保持默认
```

## 模块改动边界

| 文件 | 改动类型 | 改动点 |
|------|---------|-------|
| `src/AgentInfo.h` | 追加字段 | struct 末尾 + dataEquals() |
| `src/ClaudeMetaReader.h` | 追加字段 | ClaudeMeta struct |
| `src/ClaudeMetaReader.cpp` | 新增函数 + 调用 | parseContextUsage + 两处调用 + tail 8192→32768 |
| `src/AgentModel.h` | 追加枚举 | Roles enum 末尾 |
| `src/AgentModel.cpp` | 追加 case | roleNames() + data() + get() |
| `src/main.cpp` | 追加赋值 | enrichAgents() + buildMockSnapshot() |
| `qml/ExpandedView.qml` | 新增 Item | delegate 背景 Rectangle 内 |

## QML 进度条设计细节

### 布局层次（delegate 内）

```
rowItem (Item, height=62)
└── Rectangle (background, anchors.fill: parent)
    ├── Rectangle (left accent rail, visible: isActive)
    ├── Rectangle (bottom separator, height: 1)
    ├── MouseArea (rowHov)
    └── Item (context bar container, height: 2, bottom-anchored) ← 新增
        └── Rectangle (progress fill, width: parent.width * ratio)

Item (iconArea, outside background)
Column (name + step, outside background)
```

### 颜色语义

| ratio 范围 | 颜色 | 语义 |
|-----------|------|------|
| ≤ 70% | `Theme.accent` (#7c8cff 等) | 正常 |
| 70%–90% | `#f59e0b` | 警告：context 接近上限 |
| > 90% | `Theme.coral` (#f87171) | 危险：接近满载 |

所有颜色变化通过 `ColorAnimation { duration: 300 }` 平滑过渡。

### 动画规格

- 宽度变化：`NumberAnimation { duration: 400; easing.type: Easing.OutCubic }`
- 颜色变化：`ColorAnimation { duration: 300 }`
- 首次出现（从 0 → 实际宽度）：Behavior 自然触发，形成填充感

## PBT 不变量

### P1: 进度条宽度边界
- **不变量**: `0 ≤ barWidth ≤ parentWidth`
- **实现保证**: `Math.min(contextUsed / contextLimit, 1.0)` 上限钳制
- **反例生成**: 注入 `contextUsed = 999999, contextLimit = 200000` → bar 宽度 == parentWidth

### P2: Codex 会话不可见
- **不变量**: `toolType == "Codex" → !bar.visible`
- **实现保证**: Codex 分支不设置 contextUsed（保持 0）→ `visible: false`
- **反例生成**: 强制 Codex AgentInfo.contextUsed = 1 → bar 错误显示

### P3: 颜色单调性
- **不变量**: `ratio > 0.9 → coral; ratio ∈ (0.7, 0.9] → amber; ratio ≤ 0.7 → accent`
- **实现保证**: QML 嵌套 ternary 确保互斥分支
- **反例生成**: 注入 ratio = 0.95 → 必须 = coral；注入 0.75 → 必须 = amber

### P4: dataEquals 完整性
- **不变量**: contextUsed 变化 → `dataEquals()` 返回 false → `dataChanged` 信号发出
- **实现保证**: dataEquals 显式比较 contextUsed + contextLimit
- **反例生成**: 故意省略 contextUsed 的 dataEquals 比较 → usage 更新不触发 UI 刷新

### P5: Idle 状态零值
- **不变量**: `status == "idle" → contextUsed == 0 && contextLimit == 0`
- **实现保证**: Idle 分支直接 return，不调用 parseContextUsage
- **反例生成**: 在 Idle 分支调用 parseContextUsage → 读取旧 usage 数据造成假阳性

### P6: parseContextUsage 幂等性
- **不变量**: 相同 `tail` 输入 → 相同输出（纯函数）
- **实现保证**: 函数无副作用，不读文件
- **反例生成**: 连续调用两次同一 tail → 输出 `{used, limit}` 必须相同

## 边界情况处理

| 场景 | 处理 |
|------|------|
| tail 中无 assistant 行 | 返回 `{0, 0}`，bar 隐藏 |
| assistant 行 JSON 损坏 | 跳过该行，继续向前扫描 |
| usage 对象缺少 cache_* 字段 | 缺失字段视为 0，不报错 |
| contextUsed > contextLimit | `Math.min` 钳制，bar 满宽 |
| contextUsed = contextLimit | bar 满宽，颜色为 coral |
| 单行 JSON > 32768 字节 | readTail 会跳过该行（partial），contextUsed = 0，bar 隐藏 |
| Codex sessions | contextUsed 保持 0，bar 隐藏 |
