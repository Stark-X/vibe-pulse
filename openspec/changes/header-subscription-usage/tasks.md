# Tasks: Header Subscription Usage Display

## Phase 1: 后端 C++ — SubscriptionMonitor

- [x] 1.1 新建 `src/SubscriptionMonitor.h`：声明类、4 个 Q_PROPERTY（claudeUtilization/codexUtilization/claudeAvailable/codexAvailable）、start()、refreshAll()、refreshClaude()、refreshCodex()、handleClaudeReply()、handleCodexReply()、parseMockEnv()、m_nam/m_claudeReply/m_codexReply 成员
- [x] 1.2 新建 `src/SubscriptionMonitor.cpp`：实现构造函数（初始化 m_nam、QTimer，不在 ctor 发请求）
- [x] 1.3 实现 `parseMockEnv()`：读取 PULSE_MOCK_SUBSCRIPTION env，解析 CC=/CD= 格式，设置对应 available/utilization，emit dataChanged()，跳过 timer 和网络
- [x] 1.4 实现 `start()`：检查 mock env → 若非 mock 则 singleShot(5000, refreshAll) + QTimer 每 300 秒重复
- [x] 1.5 实现 `loadClaudeCredential()`：读取 ~/.claude/.credentials.json，提取 claudeAiOauth.accessToken/expiresAt，返回 (token, ok)；过期或缺失返回 (empty, false)；禁止日志输出 token
- [x] 1.6 实现 `loadCodexCredential()`：读取 ~/.codex/auth.json，检查 auth_mode=="chatgpt"，提取 tokens.access_token/account_id，返回 (token, accountId, ok)；禁止日志输出 token
- [x] 1.7 实现 `refreshClaude()`：调用 loadClaudeCredential；若 ok 且 m_claudeReply==nullptr 则发 GET 请求（设置 Authorization/anthropic-beta/Accept/10s timeout），连接 finished 到 handleClaudeReply
- [x] 1.8 实现 `handleClaudeReply()`：检查 HTTP status；解析 five_hour.utilization（降级 seven_day.utilization）；clamp [0,100]；设置 m_claudeAvailable/m_claudeUtilization；所有路径 reply->deleteLater()+m_claudeReply=nullptr；emit dataChanged()
- [x] 1.9 实现 `refreshCodex()`：调用 loadCodexCredential；若 ok 且 m_codexReply==nullptr 则发 GET 请求（设置 Authorization/User-Agent: codex-cli/Accept/可选 ChatGPT-Account-Id/10s timeout），连接 finished 到 handleCodexReply
- [x] 1.10 实现 `handleCodexReply()`：检查 HTTP status；解析 rate_limit.primary_window.used_percent；clamp [0,100]；设置 m_codexAvailable/m_codexUtilization；所有路径 deleteLater+nullptr；emit dataChanged()

## Phase 2: CMakeLists + main.cpp 集成

- [x] 2.1 `CMakeLists.txt`：在 qt_add_executable(pulse ...) 中追加 `src/SubscriptionMonitor.h src/SubscriptionMonitor.cpp`
- [x] 2.2 `src/main.cpp`：include `"SubscriptionMonitor.h"`；在 QQmlEngine 前创建 `auto *subscriptionMonitor = new SubscriptionMonitor(&app)`
- [x] 2.3 `src/main.cpp`：在 setContextProperty 块追加 `engine.rootContext()->setContextProperty("subscriptionMonitor", subscriptionMonitor)`
- [x] 2.4 `src/main.cpp`：在 `component.completeCreate()` 后调用 `subscriptionMonitor->start()`

## Phase 3: QML 前端

- [x] 3.1 `qml/HeaderBar.qml`：新增 4 个 property（claudeUtilization: 0.0，codexUtilization: 0.0，claudeAvailable: false，codexAvailable: false）
- [x] 3.2 `qml/HeaderBar.qml`：新增 `function usageColor(pct)`：>85→Theme.coral，>60→Theme.peach，else→Theme.text2
- [x] 3.3 `qml/HeaderBar.qml`：新增 `usageRow`（Row，anchors.right: metaArea.left，rightMargin: 4，spacing: 4）；内含两个 Text 子组件分别显示 CC/CD 百分比，visible 绑定对应 available
- [x] 3.4 `qml/HeaderBar.qml`：将 Column 的 `anchors.right` 从 `metaArea.left` 改为 `usageRow.left`，rightMargin 从 10 改为 6（保持等效间距）
- [x] 3.5 `qml/main.qml`：在 HeaderBar 调用处追加 `claudeUtilization/codexUtilization/claudeAvailable/codexAvailable` 四个绑定到 `subscriptionMonitor.*`

## Phase 4: Mock 与 Makefile

- [x] 4.1 `Makefile`：追加 `mock-subscription` 目标：`PULSE_MOCK=working PULSE_MOCK_SUBSCRIPTION=CC=45,CD=31 build_rel/pulse`

## Phase 5: 验证

- [x] 5.1 编译验证：`cmake --build build_rel --target pulse` 零错误零警告
- [ ] 5.2 Mock 验证：`make mock-subscription` 启动后 Header 显示 "CC 45%  CD 31%"（working 状态）— 屏幕锁定时无法截图，解锁后可验证
- [ ] 5.3 Mock 验证：`PULSE_MOCK=idle PULSE_MOCK_SUBSCRIPTION=CC=87 build_rel/pulse` 只显示 "CC 87%"（coral 色），CD 不显示
- [ ] 5.4 Mock 验证：`PULSE_MOCK=question PULSE_MOCK_SUBSCRIPTION=CC=45,CD=31 build_rel/pulse` 用量指示器不显示（question 状态）
- [ ] 5.5 无凭据验证：无 PULSE_MOCK_SUBSCRIPTION，真实凭据不存在时，Header 高度/布局不变
