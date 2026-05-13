# Pulse: Agent Subtitle Real-time Update Fix

## Problem
两个 bug 导致 Pulse overlay 中 agent 行的小字 (subtitle/currentStep) 不实时更新：

1. **`AgentInfo::operator==` 仅比较 pid+toolType**，`AgentModel::setSnapshot` 用它判断是否触发 `dataChanged`，结果对任何已存在 agent 永远不发信号，UI 冻结在首次插入的值。
2. **`CodexMetaReader` 不识别 `task_complete` 事件**，Codex 完成 turn / 提问后仍显示上一条命令。

## Changes

### `pulse/src/AgentInfo.h`
新增 `dataEquals()` 方法，对所有显示字段做完整比较（name, status, cwd, sessionId, sessionName, sessionBusy, currentStep, windowAddress）。`operator==` 保留 identity 语义（pid+toolType）。

### `pulse/src/AgentModel.cpp`
`setSnapshot` 的更新检测从 `m_agents[oi] != na`（identity compare）改为 `!m_agents[oi].dataEquals(na)`（data compare），确保字段变化时正确触发 `dataChanged`。

### `pulse/src/CodexMetaReader.cpp`
扫描 JSONL 时同时追踪 `task_complete` payload：
- 若 `task_complete` 在最后一个 `function_call` 之后出现 → Codex 完成 turn，显示 `last_agent_message` 第一行（≤100 字符）作为 subtitle
- 否则仍显示最后的 `function_call` 命令（原有行为）

## Verification
`cmake --build build_rel --target pulse -- -j$(nproc)` 编译通过，零错误。
