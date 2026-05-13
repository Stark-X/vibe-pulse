# Design: Fix Background Agent Working Directory

## Architecture Overview

```
ProcScanner::scanAll()
  ↓ AgentInfo{cwd = procfs_cwd}
enrichAgents()
  ↓ ClaudeMetaReader::read(pid, procfs_cwd)
    → reads ~/.claude/sessions/<pid>.json
    → returns ClaudeMeta{cwd = session_cwd}  ← 新增字段
  ↓ if m.cwd ≠ "": override a.cwd, a.name   ← 新增逻辑
AgentModel::setSnapshot()
  ↓ QML 显示正确的 cwd
```

## File-Level Changes

### 1. `pulse/src/ClaudeMetaReader.h`

**Before:**
```cpp
struct ClaudeMeta {
    QString sessionId;
    QString sessionName;
    bool    sessionBusy = true;
    QString currentStep;
};
```

**After:**
```cpp
struct ClaudeMeta {
    QString sessionId;
    QString sessionName;
    bool    sessionBusy = true;
    QString currentStep;
    QString cwd;          // session JSON の原始项目路径
};
```

### 2. `pulse/src/ClaudeMetaReader.cpp`

在 `sobj` 解析完成后（第 103 行附近），添加：
```cpp
m.cwd = sobj.value(QStringLiteral("cwd")).toString();
```

位置：紧接 `m.sessionId = sobj.value(...)`，在任何 early return 之前。

**关键约束**：即使 `sessionId` 为空（session 文件格式不含 sessionId），只要 `cwd` 存在也应返回，因此 `m.cwd` 赋值必须在 `if (m.sessionId.isEmpty()) return m;` 之前。

实际上，当前代码在 `m.sessionId.isEmpty()` 时 return，此时 `m.cwd` 也没有被设置。我们需要在 return 之前设置 cwd：

```cpp
m.sessionId   = sobj.value(QStringLiteral("sessionId")).toString();
m.sessionName = sobj.value(QStringLiteral("name")).toString();
m.cwd         = sobj.value(QStringLiteral("cwd")).toString();  // ← 新增，在 early return 前
const QString status = sobj.value(QStringLiteral("status")).toString();
m.sessionBusy = (status != QStringLiteral("idle"));

if (m.sessionId.isEmpty())
    return m;  // cwd 已设置，可供 enrichAgents 使用
```

### 3. `pulse/src/main.cpp` — `enrichAgents()`

**Before:**
```cpp
ClaudeMeta m = ClaudeMetaReader::read(a.pid, a.cwd, &newPaths);
a.sessionId   = m.sessionId;
a.sessionName = m.sessionName;
a.sessionBusy = m.sessionBusy;
a.currentStep = m.currentStep;
```

**After:**
```cpp
ClaudeMeta m = ClaudeMetaReader::read(a.pid, a.cwd, &newPaths);
a.sessionId   = m.sessionId;
a.sessionName = m.sessionName;
a.sessionBusy = m.sessionBusy;
a.currentStep = m.currentStep;
if (!m.cwd.isEmpty()) {
    a.cwd  = m.cwd;
    a.name = QFileInfo(m.cwd).fileName();
}
```

**必须在 `a.windowAddress` 计算之前执行**（当前位置已满足，windowAddress 在独立 if 块后）。

### 4. `pulse/src/ProcScanner.cpp` — kRules

**Before:**
```cpp
{ "Claude Code", "claude", nullptr, nullptr       },
```

**After:**
```cpp
{ "Claude Code", "claude", nullptr, " agents"     },
```

**原理**：`argv.join(' ')` 对 `claude agents` 产生 `"claude agents"`，含子串 `" agents"`（前置空格），而合法 agent 启动命令（`claude`, `claude --bg`, `claude --resume`）均不含此子串。

## Edge Cases

| 场景 | 行为 |
|------|------|
| session JSON 无 `cwd` 字段（极旧版 Claude Code） | `m.cwd` == ""，跳过覆盖，使用 procfs cwd（降级） |
| session JSON 不存在（agent 刚启动） | `read()` 返回空 `ClaudeMeta`，`m.cwd` == ""，使用 procfs cwd |
| `claude agents --json` 等带标志的变体 | cmdline = "claude agents --json"，含 " agents"，正确排除 |
| 项目路径含 "agents" 字符串的合法 agent | cmdline = "claude" 或 "claude --bg"，不含 " agents"（有空格前缀），不受影响 |

## No Changes Required

- `AgentInfo.h` — `cwd` 字段已存在
- `AgentModel.cpp` — 无变化
- `qml/main.qml` — 无变化
- Codex/OpenCode 相关文件 — 无变化
