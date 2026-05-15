# Specs: Header Subscription Usage Display

## FR-01: SubscriptionMonitor 类

**行为**：新增 `src/SubscriptionMonitor.{h,cpp}`，继承 `QObject`，承载 Claude Code 和 Codex 的官方订阅用量查询。

**接口**：
```cpp
class SubscriptionMonitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(double claudeUtilization READ claudeUtilization NOTIFY dataChanged)
    Q_PROPERTY(double codexUtilization  READ codexUtilization  NOTIFY dataChanged)
    Q_PROPERTY(bool   claudeAvailable   READ claudeAvailable   NOTIFY dataChanged)
    Q_PROPERTY(bool   codexAvailable    READ codexAvailable    NOTIFY dataChanged)
public:
    explicit SubscriptionMonitor(QObject *parent = nullptr);
    void start();
signals:
    void dataChanged();
private:
    double m_claudeUtilization = 0.0;
    double m_codexUtilization  = 0.0;
    bool   m_claudeAvailable   = false;
    bool   m_codexAvailable    = false;
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply         *m_claudeReply = nullptr;
    QNetworkReply         *m_codexReply  = nullptr;
};
```

**规则**：
- 默认值 `available=false`（启动前不显示）
- `start()` 安排 `QTimer::singleShot(5000, ...)` 首次 `refreshAll()`，随后 `QTimer` 每 5 分钟重复
- `QNetworkAccessManager` 以 `SubscriptionMonitor` 为 parent，在 GUI 线程创建/使用

---

## FR-02: 凭据读取规格

### Claude 凭据

**文件**：`QDir::homePath() + "/.claude/.credentials.json"`

**解析**：
```json
{ "claudeAiOauth": { "accessToken": "...", "expiresAt": 1234567890000 } }
```

**规则**：
- 文件不存在 → `claudeAvailable = false`，跳过请求
- `claudeAiOauth.accessToken` 缺失或空 → `claudeAvailable = false`，跳过请求
- `expiresAt` 存在且为整数 → 按毫秒 epoch 处理；`QDateTime::currentMSecsSinceEpoch() > expiresAt - 60000` → 跳过请求，`claudeAvailable = false`
- `expiresAt` 缺失或无法解析 → 忽略过期检查，继续发请求
- **禁止**将 token 写入任何日志

### Codex 凭据

**文件**：`QDir::homePath() + "/.codex/auth.json"`

**解析**：
```json
{ "auth_mode": "chatgpt", "tokens": { "access_token": "...", "account_id": "..." } }
```

**规则**：
- 文件不存在 → `codexAvailable = false`，跳过请求
- `auth_mode != "chatgpt"` → `codexAvailable = false`，跳过请求（非官方 OAuth 模式）
- `tokens.access_token` 缺失或空 → `codexAvailable = false`，跳过请求
- `tokens.account_id` 有值则在请求头中设置 `ChatGPT-Account-Id`，缺失则不设
- **禁止**将 token 写入任何日志

---

## FR-03: Claude 配额请求规格

**端点**：`GET https://api.anthropic.com/api/oauth/usage`

**请求头**：
```
Authorization: Bearer {accessToken}
anthropic-beta: oauth-2025-04-20
Accept: application/json
```

**超时**：10 秒（`QNetworkRequest::setTransferTimeout(10000)`）

**响应解析**：
1. 尝试提取 `response["five_hour"]["utilization"]`（double）
2. 若 `five_hour` 缺失，降级提取 `response["seven_day"]["utilization"]`
3. 找到有效数值 → clamp 到 [0.0, 100.0]，写入 `m_claudeUtilization`，`m_claudeAvailable = true`
4. 未找到（两个字段都缺失）→ `m_claudeAvailable = false`

**错误处理**：
- HTTP 401/403 → `m_claudeAvailable = false`
- 其他非 2xx → `m_claudeAvailable = false`
- 网络超时/错误 → `m_claudeAvailable = false`
- JSON 解析失败 → `m_claudeAvailable = false`
- 所有错误路径：静默，不打印 token，emit `dataChanged()`

---

## FR-04: Codex 配额请求规格

**端点**：`GET https://chatgpt.com/backend-api/wham/usage`

**请求头**：
```
Authorization: Bearer {access_token}
User-Agent: codex-cli
Accept: application/json
ChatGPT-Account-Id: {account_id}   // 仅当 account_id 非空时设置
```

**超时**：10 秒

**响应解析**：
1. 提取 `response["rate_limit"]["primary_window"]["used_percent"]`（double）
2. 找到有效数值 → clamp 到 [0.0, 100.0]，写入 `m_codexUtilization`，`m_codexAvailable = true`
3. 路径上任意字段缺失或非数值 → `m_codexAvailable = false`

**错误处理**：与 FR-03 相同规则

---

## FR-05: 并发请求防护

**规则**：
- `refreshAll()` 调用 `refreshClaude()` 和 `refreshCodex()` 两个独立方法
- 每个方法：若对应 `m_*Reply` 指针非空（上次请求未完成），直接返回跳过
- `finished` 信号处理末尾：`reply->deleteLater()`，并将成员指针置 nullptr
- 两个 provider 相互独立，一个失败不影响另一个

---

## FR-06: Mock 支持

**环境变量**：`PULSE_MOCK_SUBSCRIPTION`

**格式**：`CC=<0-100>,CD=<0-100>`（不区分大小写，字段可选）

**示例**：
- `PULSE_MOCK_SUBSCRIPTION=CC=45,CD=31` → claude 45%，codex 31%
- `PULSE_MOCK_SUBSCRIPTION=CC=87` → claude 87%，codex 不显示
- `PULSE_MOCK_SUBSCRIPTION=CC=0,CD=0` → 两者都 0%，都显示

**规则**：
- 若 env var 存在，跳过所有凭据读取和 HTTP 请求
- 解析成功的 provider → `available = true`，值为解析出的百分比
- 未出现在 env var 中的 provider → `available = false`

---

## FR-07: CMakeLists.txt 集成

**修改**：在 `qt_add_executable(pulse ...)` 中追加：
```cmake
src/SubscriptionMonitor.h
src/SubscriptionMonitor.cpp
```

Qt6::Network 已存在，不重复添加。

---

## FR-08: main.cpp 集成

**修改**（在 `QQmlEngine engine;` 前）：
```cpp
auto *subscriptionMonitor = new SubscriptionMonitor(&app);
```

**修改**（在 `engine.rootContext()->setContextProperty(...)` 处追加）：
```cpp
engine.rootContext()->setContextProperty("subscriptionMonitor", subscriptionMonitor);
```

**修改**（在 `component.completeCreate()` 后）：
```cpp
subscriptionMonitor->start();
```

---

## FR-09: QML HeaderBar 集成

### HeaderBar.qml 新增 property

```qml
property double claudeUtilization: 0
property double codexUtilization:  0
property bool   claudeAvailable:   false
property bool   codexAvailable:    false
```

### 布局重构：新增 usageRow

**位置**：`metaArea` 左侧，两者共同组成右对齐区域

**结构**：
```qml
Row {
    id: usageRow
    anchors.right: metaArea.left
    anchors.rightMargin: spacing
    anchors.verticalCenter: parent.verticalCenter
    spacing: 4
    visible: (pulseState === "idle" || pulseState === "working")

    // CC 指示器
    Rectangle { ... visible: claudeAvailable }
    // CD 指示器
    Rectangle { ... visible: codexAvailable }
}
```

**中间 Column 右锚点**：从 `metaArea.left` 改为 `usageRow.left`（已处理 usageRow 折叠时的回退）

### 颜色规则

```qml
function usageColor(pct) {
    if (pct > 85) return Theme.coral
    if (pct > 60) return Theme.peach
    return Theme.text2
}
```

### 显示格式

每个指示器：标签（"CC" 或 "CD"）+ 空格 + 百分比整数 + "%"  
字体：JetBrains Mono，9px  
高度：18px，与 idleChip 一致

---

## FR-10: main.qml 修改

**HeaderBar 调用处追加**：
```qml
claudeUtilization: subscriptionMonitor.claudeUtilization
codexUtilization:  subscriptionMonitor.codexUtilization
claudeAvailable:   subscriptionMonitor.claudeAvailable
codexAvailable:    subscriptionMonitor.codexAvailable
```

---

## Non-Functional Requirements

- **安全**：token/凭据禁止写入 qDebug/qWarning/qInfo
- **线程**：全部在 GUI 线程（QNetworkAccessManager 内部异步，无需 QThread）
- **启动延迟**：5 秒后首次请求，不阻塞 UI 启动
- **轮询间隔**：300 秒（5 分钟）
- **超时**：每次请求 10 秒
- **Header 高度**：44px 不变，`usageRow` 高度 ≤ 20px
