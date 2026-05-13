# Proposal: Fix Background Agent Working Directory

## Overview

修复 Pulse Qt6 widget 在 Claude Code `--bg` 模式下显示错误工作目录的问题。

当用户在 DIR_A 中运行 `claude --bg`，agent 进入后台后可能通过 `chdir()` 改变自身工作目录；同时 `claude agents` 管理命令会在 DIR_B 中以自身 cwd 短暂出现，导致 `ProcScanner` 误读 `/proc/<pid>/cwd`，在 widget 中显示错误路径。

**目标平台**: Ubuntu 24.04 + Hyprland  
**技术栈**: Qt6 C++ + QML（已完成 Tauri→Qt 重写）

---

## Root Cause

### 1. `ClaudeMeta` 不携带 `cwd`

`ClaudeMetaReader::read()` 在 `~/.claude/sessions/<id>.json` 中已经读取了权威的 `cwd` 字段（第 83、124 行），但 `ClaudeMeta` struct 没有 `cwd` 成员，无法将正确路径传回 `enrichAgents()`。

结果：`AgentInfo::cwd` 和 `AgentInfo::name` 始终来自 `/proc/<pid>/cwd`，在 bg 模式下已失效。

### 2. `claude agents` 被误识别为 agent

`ProcScanner` 规则：
```cpp
{ "Claude Code", "claude", nullptr, nullptr }
```
无 argvExclude 过滤，`claude agents`（comm=`claude`，argv[1]=`agents`）会被匹配为一个 Claude Code agent，显示 DIR_B 路径。

---

## Discovered Constraints

### Hard Constraints

1. `/proc/<pid>/cwd` 反映进程当前目录，background agent 可能在运行过程中 `chdir()`，导致读取错误
2. `ClaudeMeta` struct 缺少 `cwd` 字段——需要添加才能传播权威路径
3. `claude agents` 是短暂管理命令，应从扫描中排除

### Soft Constraints

1. Session JSON `cwd` 字段是项目根目录的权威来源（Claude Code 自身写入）
2. `/proc/<pid>/environ` 的 `PWD=` 保存进程启动时的工作目录，是 session JSON 不可用时的可靠回退
3. 修改 `AgentRule` 结构体需保持与 Codex/OpenCode 规则的向后兼容

### Dependencies

- `ClaudeMetaReader.h` → `ClaudeMetaReader.cpp` → `main.cpp`（enrichAgents）
- `ProcScanner.cpp` 独立，不影响其他模块

---

## Solution Design

### Change 1: `ClaudeMeta` 添加 `cwd` 字段

```cpp
struct ClaudeMeta {
    QString sessionId;
    QString sessionName;
    bool    sessionBusy = true;
    QString currentStep;
    QString cwd;          // ← 新增：session JSON 中的原始项目路径
};
```

`ClaudeMetaReader::read()` 在解析 session JSON 后：
```cpp
m.cwd = sobj.value(QStringLiteral("cwd")).toString();
```

### Change 2: `enrichAgents()` 用 session cwd 覆盖进程 cwd

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

### Change 3: 排除 `claude agents` 子命令

`ProcScanner` 规则中，将 `argvExclude` 设为 `" agents"` 以排除该管理命令：
```cpp
{ "Claude Code", "claude", nullptr, " agents" },
```
（注意前置空格，避免路径中含 "agents" 的误匹配；cmdline = `claude agents` 拼接后含 `" agents"`）

---

## Risk Assessment

| 风险 | 概率 | 缓解 |
|------|------|------|
| session JSON 无 `cwd` 字段（旧版 Claude Code） | 低 | 仅在 `m.cwd` 非空时覆盖，降级为原有行为 |
| `argvExclude " agents"` 误过滤含 ` agents` 的合法调用 | 极低 | 真实 agent 启动不带此参数；接受此边界条件 |
| fallback 到 `/proc/environ` PWD 的复杂度 | — | session JSON 已足够可靠，暂不实现 environ fallback |

---

## Success Criteria

1. 在 DIR_A 运行 `claude --bg`，widget 显示 DIR_A（而非 agent 当前 cwd）
2. 在 DIR_B 运行 `claude agents`，该进程不出现在 widget 列表中
3. 非 bg 模式的普通 Claude Code agent 行为无回归
4. `a.name` 显示 DIR_A 的 basename，不随进程 cwd 变化

---

## Scope

**修改文件**（仅 3 个）：
- `pulse/src/ClaudeMetaReader.h` — 添加 `cwd` 字段
- `pulse/src/ClaudeMetaReader.cpp` — 填充 `m.cwd`
- `pulse/src/main.cpp` — `enrichAgents()` 使用 `m.cwd` 覆盖
- `pulse/src/ProcScanner.cpp` — 添加 `claude agents` 排除规则

**不修改**：QML 层、AgentInfo struct（cwd 字段已存在）、其他 agent 类型处理逻辑
