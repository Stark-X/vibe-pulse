# Design: Header Subscription Usage Display

## 系统架构

```
┌─────────────────────────────────────────────┐
│  main.cpp                                   │
│  ┌──────────────────────┐                   │
│  │  SubscriptionMonitor │ ← QML context     │
│  │  - QNetworkAccessMgr │   "subscriptionMonitor"
│  │  - QTimer (5min)     │                   │
│  │  - claudeAvailable   │                   │
│  │  - codexAvailable    │                   │
│  └──────────┬───────────┘                   │
│             │ dataChanged()                 │
└─────────────┼───────────────────────────────┘
              ↓
┌─────────────────────────────────────────────┐
│  qml/main.qml                               │
│  HeaderBar {                                │
│    claudeUtilization: subscriptionMonitor.* │
│    ...                                      │
│  }                                          │
└─────────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────────┐
│  qml/HeaderBar.qml                          │
│  ┌──────────────────────────────────────┐   │
│  │ [●] Pulse  CC 45%  CD 31%  [2 live] │   │
│  │ [●] Pulse  CC 87%  CD 31%  [...]    │   │ working
│  │ [●] Pulse                  [asking] │   │ question
│  └──────────────────────────────────────┘   │
└─────────────────────────────────────────────┘
```

## HeaderBar 布局变化

### 当前布局（44px）

```
[heartArea 20px] [← Column (headline+sub) →] [metaArea]
```

### 新布局

```
[heartArea 20px] [← Column →] [usageRow] [metaArea 14px→]
                               ↑
                           新增区域
```

**锚点变化**：
- `Column.anchors.right` = `usageRow.left`（原来是 `metaArea.left`）
- `usageRow.anchors.right` = `metaArea.left`，`rightMargin: 4`
- `metaArea` 锚点不变（`anchors.right: parent.right`，`rightMargin: 14`）

## 指示器视觉规格

```
┌──────────┐
│ CC 45%   │  高度 18px，无边框，纯文本
└──────────┘
字体: JetBrains Mono 9px
颜色: usageColor(pct)
  < 60%  → Theme.text2  (正常)
  60-85% → Theme.peach  (中警)
  > 85%  → Theme.coral  (高警)
```

**不使用 Rectangle 边框**：与 idleChip 的方框样式区分，保持视觉层次低，不喧宾夺主

## 状态可见性矩阵

| pulseState  | usageRow visible | 单个指示器 visible       |
|-------------|-----------------|------------------------|
| idle        | true            | 取决于 claudeAvailable 等 |
| working     | true            | 取决于 claudeAvailable 等 |
| question    | false           | —                      |
| plan        | false           | —                      |
| permission  | false           | —                      |
| expanded    | false           | —                      |

## SubscriptionMonitor 数据流

```
start()
  └─ singleShot(5s) ──→ refreshAll()
                              ├─ refreshClaude()
                              │    ├─ loadClaudeCredential() → (token, ok)
                              │    ├─ [ok] QNetworkRequest + send()
                              │    └─ handleClaudeReply()
                              │         ├─ parse five_hour / seven_day
                              │         └─ emit dataChanged()
                              └─ refreshCodex()
                                   ├─ loadCodexCredential() → (token, accountId, ok)
                                   ├─ [ok] QNetworkRequest + send()
                                   └─ handleCodexReply()
                                        ├─ parse primary_window
                                        └─ emit dataChanged()

QTimer(5min) ──→ refreshAll() [loop]
```

## Mock 数据流（PULSE_MOCK_SUBSCRIPTION）

```
start()
  └─ 检测 qgetenv("PULSE_MOCK_SUBSCRIPTION") 非空
       └─ parseMockEnv()
            ├─ "CC=45" → m_claudeUtilization=45, m_claudeAvailable=true
            ├─ "CD=31" → m_codexUtilization=31, m_codexAvailable=true
            └─ emit dataChanged()
            （不启动 timer，不发网络请求）
```

## Makefile mock 目标（参考）

在 `Makefile` 中可追加：
```makefile
mock-subscription:
    PULSE_MOCK=working PULSE_MOCK_SUBSCRIPTION=CC=45,CD=31 build_rel/pulse
```

## 关键实现约束

| 约束 | 原因 |
|------|------|
| 凭据每次 poll 前重新读文件 | 支持 CLI 自动刷新 token 后无需重启 Pulse |
| `m_claudeReply` 非空时跳过 | 防止 5 分钟内网络慢导致响应堆积覆盖状态 |
| `deleteLater()` 在所有 return 分支 | QNetworkReply 内存安全 |
| token 禁止记录到日志 | 编码规范安全章节 |
| GUI thread only | QNetworkAccessManager 在非 GUI thread 创建是 UB |
