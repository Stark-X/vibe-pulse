# Proposal: Header Subscription Usage Display (Claude Code & Codex)

## Overview

在 Pulse 浮层的 **HeaderBar** 中，针对 idle 和 working 状态新增两个紧凑型订阅用量指示器，分别展示 Claude Code（claude.ai Max/Pro 官方套餐）和 Codex（ChatGPT Plus/Pro 官方套餐）的**五小时滚动窗口**用量百分比。

**前提条件**：仅在用户使用**官方订阅**（OAuth 模式）时显示；API key 或第三方代理路由不显示。  
**目标**：用户无需打开 cc-switch 即可即时感知 Claude Code 和 Codex 套餐的消耗情况。  
**平台**：Ubuntu 24.04 + Hyprland  
**技术栈**：C++/Qt6 backend + QML frontend

---

## Data Sources（已验证）

### Claude Code 订阅用量

**官方订阅判断**：`~/.claude/.credentials.json` 存在且包含 `claudeAiOauth.accessToken`。  
文件不存在、字段缺失、或仅有 `ANTHROPIC_API_KEY` → **不显示**（3rd-party 路由）。

```json
{
  "claudeAiOauth": {
    "accessToken": "<Bearer token>",
    "expiresAt": 1234567890000
  }
}
```

**API 端点**：
```
GET https://api.anthropic.com/api/oauth/usage
Authorization: Bearer {accessToken}
anthropic-beta: oauth-2025-04-20
Accept: application/json
```

**响应结构**（关键字段）：
```json
{
  "five_hour":  { "utilization": 45.2, "resets_at": "2026-05-15T22:00:00Z" },
  "seven_day":  { "utilization": 12.8, "resets_at": "2026-05-21T00:00:00Z" }
}
```

**展示字段**：`five_hour.utilization`（0–100，表示已用百分比）。若 `five_hour` 不存在则降级到 `seven_day`。

### Codex 订阅用量

**官方订阅判断**：`~/.codex/auth.json` 存在且 `auth_mode == "chatgpt"`。  
`auth_mode` 为其他值（如 API key 模式）→ **不显示**。

```json
{
  "auth_mode": "chatgpt",
  "tokens": {
    "access_token": "<Bearer token>",
    "account_id": "<account id>"
  }
}
```

**API 端点**：
```
GET https://chatgpt.com/backend-api/wham/usage
Authorization: Bearer {access_token}
User-Agent: codex-cli
Accept: application/json
ChatGPT-Account-Id: {account_id}   (可选)
```

**响应结构**（关键字段）：
```json
{
  "rate_limit": {
    "primary_window": {
      "used_percent": 31.5,
      "limit_window_seconds": 18000,
      "reset_at": 1747436400
    }
  }
}
```

**展示字段**：`rate_limit.primary_window.used_percent`（五小时窗口，0–100）

---

## Discovered Constraints

### Hard Constraints（不可违反）

- [HC-1] **官方订阅判断**：
  - Claude：`~/.claude/.credentials.json` 含 `claudeAiOauth.accessToken` → 官方；否则 `NotFound`，静默隐藏
  - Codex：`auth_mode == "chatgpt"` → 官方；否则 `NotFound`，静默隐藏
  - 任何第三方 API key / 代理配置 → 对应指示器不显示
- [HC-2] QML 组件使用 `Theme.*` 颜色系统，禁止硬编码颜色
- [HC-3] HTTP 请求必须异步，不得阻塞 UI 线程（`QNetworkAccessManager` 可用，Qt6::Network 已链入）
- [HC-4] 禁止将凭据/Token 写入日志（编码规范安全章节）
- [HC-5] HeaderBar 高度固定 44px，新增指示器不得改变 Header 高度
- [HC-6] 新增 C++ 属性必须通过 `Q_PROPERTY` 暴露给 QML

### Soft Constraints

- [SC-1] 启动后延迟 5 秒首次请求（避免阻塞启动流程），之后每 5 分钟轮询一次
- [SC-2] API 失败（401/网络超时）→ 静默隐藏，保留上次成功值；首次失败前不显示
- [SC-3] 指示器仅在 `pulseState === "idle"` 或 `"working"` 时显示；其他状态（question/plan/permission/expanded）不显示，不抢夺视觉焦点
- [SC-4] 指示器宽度紧凑：工具标识（CC/CD）+ 百分比，每个 ≤ 56px
- [SC-5] HTTP 超时 10 秒

### Dependencies

- `Qt6::Network` 已在 `CMakeLists.txt` 链接
- `src/main.cpp` 需创建 `SubscriptionMonitor` 并注册为 QML 上下文属性
- `qml/HeaderBar.qml` 通过新 property 接收数据
- `CMakeLists.txt` 追加新源文件

### Risks & Mitigations

| 风险 | 缓解 |
|------|------|
| Token 过期 → 401 | 静默隐藏，不报错；若 expiresAt 字段明确过期，跳过请求 |
| `five_hour` 字段 API 变更移除 | 降级到 `seven_day`；全无则隐藏 |
| Codex 非 OAuth 模式 | auth_mode 检查直接判断，跳过请求 |
| 网络离线 | 10s 超时静默失败，保留上次值 |

---

## Architecture

### 新增文件

| 文件 | 说明 |
|------|------|
| `src/SubscriptionMonitor.h` | 类声明：凭据读取 + QNetworkAccessManager 轮询 + Q_PROPERTY |
| `src/SubscriptionMonitor.cpp` | 实现：读 JSON、发请求、解析响应、emit dataChanged |

### 修改文件

| 文件 | 操作 |
|------|------|
| `CMakeLists.txt` | 追加 `src/SubscriptionMonitor.{h,cpp}` |
| `src/main.cpp` | 创建 `SubscriptionMonitor`，注册为 `subscriptionMonitor` |
| `qml/main.qml` | 向 HeaderBar 传入 subscription 属性 |
| `qml/HeaderBar.qml` | 新增 property；在右侧添加双指示器 Row |

### SubscriptionMonitor 接口（草案）

```cpp
class SubscriptionMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(double claudeUtilization READ claudeUtilization NOTIFY dataChanged)
    Q_PROPERTY(double codexUtilization  READ codexUtilization  NOTIFY dataChanged)
    Q_PROPERTY(bool   claudeAvailable   READ claudeAvailable   NOTIFY dataChanged)
    Q_PROPERTY(bool   codexAvailable    READ codexAvailable    NOTIFY dataChanged)
public:
    explicit SubscriptionMonitor(QObject *parent = nullptr);
    void start(); // 5s 延迟首次请求 + 5分钟定时轮询
signals:
    void dataChanged();
};
```

### QML 视觉设计（HeaderBar 右侧）

```
[ CC 45% ] [ CD 31% ]    ← idle/working 状态下显示
```

- 仅显示于 idle 和 working 状态
- 颜色：< 60% → `Theme.text2`；60–85% → peach；> 85% → `Theme.coral`
- 字体：JetBrains Mono 9px
- 宽度：每个指示器约 50px，两个合计不超过 Header 右侧可用空间

---

## Success Criteria

1. 编译：`cmake --build build_rel --target pulse` 零错误
2. 官方 OAuth 凭据存在时：Header 显示 CC 和/或 CD 百分比指示器
3. 使用 API key / 代理 / 无凭据时：指示器不显示，Header 高度布局不变
4. 网络请求异步，不影响 UI 响应
5. 5 分钟定时刷新正常工作，数字随时间更新

---

## User Confirmations

- [UC-1] 分开展示 Claude Code 和 Codex 两个指示器
- [UC-2] 官方 OAuth 凭据 → 显示；API key / 3rd-party → 不显示
- [UC-3] 参考 cc-switch 实现方式（读取本地凭据 + 调用官方 API）
