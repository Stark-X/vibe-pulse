# Proposal: Agent Permission Inline Actions

## Overview

当 Claude Code agent 等待用户授权时，在 Pulse 中将该 agent 行展开，直接提供响应按钮（Approve / Deny / Approve All / Deny with message），无需切换到终端。

**平台**: Ubuntu 24.04 + Hyprland  
**目标工具**: Claude Code（其他工具暂不支持，无等效 Hook 机制）

---

## 技术机制（已验证）

### Claude Code PermissionRequest Hook

Claude Code 内置 `PermissionRequest` **阻塞型 Hook**：
- 当 Claude 需要执行需授权操作时，先触发此 Hook
- Hook 通过 **stdin** 接收 JSON 权限请求详情
- Hook 通过 **stdout** 返回决定（JSON）
- Hook 退出码 0 = 有决定；非 0 = 无决定（Claude 降级到终端交互）

### IPC 架构（文件系统）

```
Claude 进程
  → 触发 PermissionRequest Hook
    → Hook 脚本读取 stdin（tool_name, input, description）
    → 写入 ~/.claude/pulse/pending/<sessionId>.json
    → 轮询等待 ~/.claude/pulse/response/<sessionId>.json（30s 超时）
    → 读取响应 → echo 到 stdout → 退出 0

Pulse 后端（每 2s 轮询）
  → 检测 ~/.claude/pulse/pending/*.json 新文件
  → 匹配 sessionId → 对应 AgentInfo.pending_permission 设置
  → 前端收到 pending_permission ≠ null → 展开行

用户点击按钮
  → Pulse 写入 ~/.claude/pulse/response/<sessionId>.json
  → Hook 读取 → 返回给 Claude
```

### Hook 响应格式

```json
// Approve
{"decision": "approve"}

// Deny
{"decision": "block"}

// Approve All（本 session 内同类工具不再询问）
{"decision": "approve", "approve_all": true}

// Deny with message
{"decision": "block", "feedback": "原因文字"}
```

### Hook 脚本（~/.claude/pulse/permission-hook.sh）

逻辑：
1. 读 stdin → 解析 tool_name
2. PPID = claude 的 PID → 读 `~/.claude/sessions/<PPID>.json` → 获取 sessionId
3. 检查 `~/.claude/pulse/auto-approve/<sessionId>` 是否包含 tool_name → 若有则直接 approve
4. 写入 pending 文件（含 tool_name, description, input, sessionId）
5. 轮询 response 文件，最多 30 秒（300 × 100ms）
6. 超时 → 删 pending 文件 → exit 1（Claude 降级终端）

### settings.json Hook 注册

```json
{
  "hooks": {
    "PermissionRequest": [
      {
        "hooks": [
          {
            "type": "command",
            "command": "~/.claude/pulse/permission-hook.sh"
          }
        ]
      }
    ]
  }
}
```

---

## User Confirmations

- 响应按钮：Approve / Deny / Approve All / Deny with message
- 超时行为：降级到终端（Hook exit 1）
- 超时时间：30 秒

---

## UI 行为

1. **正常状态**：agent 行显示 name + current_step（现有行为）
2. **待权限状态**：行展开，显示：
   - 工具名 + 操作描述（来自 Hook stdin）
   - 若 Bash：显示命令预览（首行）
   - 若 Edit/Write：显示文件路径
3. **按钮**：Approve（绿）/ Deny（红）/ Approve All（橙）/ Deny + Input
4. **响应后**：恢复普通行状态

---

## Scope

**In scope:**
- `~/.claude/pulse/permission-hook.sh` 脚本（安装在 Pulse 启动时）
- `~/.claude/settings.json` 的 PermissionRequest Hook 注册（Pulse 启动时注入）
- Rust 后端：轮询 pending 目录、`respond_permission` Tauri 命令
- Frontend：展开行 + 权限 UI + 按钮
- Auto-approve list 文件管理

**Out of scope:**
- Codex / OpenCode 的权限拦截（无等效 Hook）
- 网络请求类权限（Bash 之外的 MCP 工具权限）
- 权限历史审计 UI
