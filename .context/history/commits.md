# Commit History

## 2026-05-13 — fix(pulse): read Codex session name from session_index.jsonl
**Decisions:**
- Extract session ID from JSONL filename (rollout-<ts>-<UUID>.jsonl) instead of parsing first line — avoids 22KB+ line read and is resilient to future file format changes
- Read thread_name from ~/.codex/session_index.jsonl by matching session ID — append-only file, last match wins for renamed sessions
- Watch session_index.jsonl via QFileSystemWatcher so renames reflect in real time
**Bugs:**
- Codex session name set by user not shown in Pulse widget → Parse session ID from filename; look up thread_name in session_index.jsonl

## 2026-05-14 — feat(pulse): PulseState 交互系统 + QML 组件库 + 后端解析扩展
**Decisions:**
- PulseState 在 C++ 端推导，QML 通过 agentModel.globalState(string) 消费
- 用户决策走 $XDG_RUNTIME_DIR/pulse/<sid>/<iid>.response.json sidecar 文件，原子写入无需终端依赖
- QML module 通过 qmldir + addImportPath 注册，Theme singleton 与多组件一起声明
- MiniMd 轻量 Markdown→HTML 转换器内嵌 C++，避免引入外部依赖

## 2026-05-14 — style(ui): 扁平化高亮 item + Header 无副标题时单行显示
**Decisions:**
- AgentRow 高亮改为扁平色块，去除渐变/阴影，视觉更干净
- Header 无副标题时折叠为单行，减少 padding 浪费

## 2026-05-14 — 🔧 chore(makefile): 新增 mock-* 快捷目标，修复 Makefile 被 .gitignore 排除
**Decisions:**
- Makefile 新增 mock-idle/working/permission/question/plan/expanded 六个 PULSE_MOCK 场景目标
- .gitignore 末尾追加 !/Makefile 例外，覆盖 Qt 模板中的 Makefile* 忽略规则

## 2026-05-14 — 🐛 fix(resize): 乘以 devicePixelRatio 修复 QT_SCALE_FACTOR 下窗口无限收缩
**Decisions:**
- Hyprland resizewindowpixel exact 接受物理像素，而 Qt QWindow::width()/height() 在 QT_SCALE_FACTOR!=1 时返回逻辑像素；两处 resizeWindow 调用均需乘以 devicePixelRatio 转换
- 选用 qRound 而非截断，避免累积误差导致 off-by-one
**Bugs:**
- QT_SCALE_FACTOR=1.2 时 widget 持续收缩直到消失 → 在两处 resizeWindow 调用点乘以 window->devicePixelRatio()

## 2026-05-14 — chore(docs): 移除 Tauri/bun 残留，更新文档至 Qt6/CMake 技术栈
**Decisions:**
- 项目已从 Tauri 2 + Rust 迁移至 C++ + Qt6 + QML + CMake，CLAUDE.md 和 workflow.md 仍记录旧技术栈
- 移除 .claude/settings.local.json 中 bun run * / bunx * 权限条目
- 保留 openspec/changes/ 历史文档作为技术迁移决策归档

## 2026-05-14 — ♻️ refactor(ui): 移除 Flickable，input 随 widget 高度自动伸展
**Decisions:**
- 移除 Flickable + ScrollBar：Flickable.contentHeight 始终 >= height 导致滚动条无条件显示
- 改为 anchor-based 布局：infoCol 固定顶部，noteBox 通过 anchors.top/bottom 填满剩余空间
- TextEdit 随 widget 高度同步扩展，widget 越高 input 越大，符合用户调整窗口的直觉

## 2026-05-15 — ✨ feat(ui): 在 ExpandedView 每行底部显示 context 使用量细进度条
**Decisions:**
- contextUsed = input_tokens + cache_creation_input_tokens + cache_read_input_tokens（来自 JSONL transcript 最近 assistant 行的 usage 字段）
- Claude Code tail 从 8192 升至 32768：实测当前 busy session 最后 usage 行距 EOF 8757B，8192 会 miss
- Codex 从 token_count event_msg 解析 last_token_usage.input_tokens / model_context_window，tail 从 4096 升至 16384
- 进度条颜色阈值：>90% → Theme.coral，>70% → #f59e0b，其余 → Theme.accent；width/color 各有 Behavior 动画
- contextLimit 在 ClaudeMetaReader 中硬编码为 kClaudeContextLimit=200000（所有当前 Claude 模型均为 200k）
- bar 叠加在 bottom separator 之上（height:2，visible 仅在 contextUsed>0），行高保持 62px 不变

## 2026-05-15 — 🐛 fix(ui): 修复 left indicator 与 context 进度条重叠——改为居中短 pill
**Decisions:**
- 左侧 active rail 从全高 (top/bottom anchored) 改为垂直居中短胶囊 (3×34px, radius:2)，与底部 2px context bar 彻底脱离
- context bar 追加 leftMargin:3，确保不覆盖 left pill 区域
- 方案来自 Codex 推荐：浮动短胶囊比全高 rail 在紧凑深色 UI 中视觉更轻、语义更清晰
**Bugs:**
- left accent rail (3px) 与底部 context usage bar (2px) 在左下角 3×2px 区域重叠，颜色混淆 → 将 rail 改为 verticalCenter 锚定的定高矩形，脱离 bottom anchor；context bar 加 leftMargin:3

## 2026-05-15 — 🐛 fix(data): context usage 值在刷新时不再被零值覆盖
**Decisions:**
- enrichAgents 中改为 guard 写入：只在 m.contextUsed > 0 时才覆盖 a.contextUsed/a.contextLimit
- 同样修复 Codex 分支的相同问题
- idle 状态 early return 不读 transcript，ClaudeMeta.contextUsed 默认 0，之前无条件赋值导致已缓存值丢失
**Bugs:**
- agent 行刷新后 context 进度条短暂消失，直到下次含 usage 的更新才重新出现 → if (m.contextUsed > 0) 才覆盖，保留上次已知非零值

## 2026-05-15 — ✨ feat(ui): 为 HeaderBar 状态切换添加过渡动画
**Decisions:**
- 指示点与光晕颜色变化使用 ColorAnimation 300ms，避免硬切换闪烁
- 右侧 chip Loader 组件切换时先淡出（opacity→0），16ms 后淡入，200ms Behavior
- 副标题文字变化时同样做淡出→淡入过渡，颜色变化 250ms ColorAnimation
- 标题临时固定为 Pulse 以隔离闪烁来源，确认闪烁来自状态切换动画缺失而非文字内容变化

## 2026-05-15 — ✨ feat(ui): 在 HeaderBar 展示 Claude Code 和 Codex 官方订阅用量
**Decisions:**
- 新增独立 SubscriptionMonitor C++ 类，不并入 AgentModel，保持关注点分离
- 仅在官方 OAuth 模式下显示（Claude: claudeAiOauth.accessToken；Codex: auth_mode==chatgpt）
- 每次 poll 前重读凭据文件，支持 CLI 自动 token 刷新后无需重启 Pulse
- API 失败立即隐藏（available=false），不保留 stale 旧值
- 启动延迟 5 秒 + 5 分钟定时轮询，避免阻塞启动
- 使用 qEnvironmentVariableIsSet 而非 isEmpty 判断 mock 模式，正确处理空值 env var
- usageRow 通过 Row child visible 控制折叠，空时 width=0，自动让出 Column 空间

## 2026-05-17 — refactor(arch): 跨平台支持 macOS + Windows
**Decisions:**
- ProcessTree 共享接口抽象 /proc、libproc、Win32 三套进程信息 API，ProcScanner/TmuxResolver/HyprlandClient 统一使用
- ProcScanner.cpp 改用 ProcessTree::listAll()/comm()/exe()/cwd()/cmdline()，dedup() 改用 ProcessTree::ppid()，无平台特定代码
- TmuxResolver.cpp 改用 ProcessTree::ancestorChain()，同一实现在 Linux/macOS 复用；Windows 单独 TmuxResolver_win.cpp 通过 wsl.exe 代理 tmux
- WindowOverlay 新抽象层：Linux 包裹 WaylandLayerShell，macOS 设 NSWindow level+collectionBehavior，Windows 设 HWND_TOPMOST+WS_EX_TOOLWINDOW
- MacOSWindowManager 用 NSRunningApplication activateWithOptions 实现 focus-on-click，无需 Accessibility 权限
- IPC socket 从 getuid()+/tmp/pulse-*.sock 改为 QLocalSocket("pulse-ipc")，跨平台自动映射到 Unix socket 或 Windows named pipe
- CMakeLists.txt 三段条件块隔离 Wayland/libproc/Win32 依赖，macOS 启用 OBJCXX 编译 .mm 文件
- Makefile 新增平台检测（Darwin/Linux/Windows），BUILD/BIN 变量化

## 2026-05-16 — chore(build): 构建路径改为标准 build/，新环境 make run 自动 configure
**Decisions:**
- 将 BUILD 从 build_rel 改为标准 build 目录，与 README 保持一致
- 新增 $(BUILD)/CMakeCache.txt 文件依赖作为 configure 触发器——build 目录不存在时自动运行 cmake -B build -S .，无需手动初始化
- clean 目标改为 rm -rf $(BUILD) 彻底清除构建目录，而非调用 cmake clean（后者要求目录已存在）
