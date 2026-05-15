# Commit History

## 2026-05-13 — fix(pulse): read Codex session name from session_index.jsonl
**Branch**: master
**Decisions**:
- Extract session ID from JSONL filename (rollout-<ts>-<UUID>.jsonl) instead of parsing first line — avoids 22KB+ line read and is resilient to future file format changes
- Read thread_name from ~/.codex/session_index.jsonl by matching session ID — append-only file, last match wins for renamed sessions
- Watch session_index.jsonl via QFileSystemWatcher so renames reflect in real time
**Bugs**:
- {'symptom': 'Codex session name set by user not shown in Pulse widget', 'root_cause': 'CodexMeta had no sessionName field; even after adding it, initial fix read only 2048 bytes from file head, truncating the 22KB session_meta line and failing JSON parse', 'fix': 'Parse session ID from filename; look up thread_name in session_index.jsonl'}
**Files**: pulse/src/CodexMetaReader.h, pulse/src/CodexMetaReader.cpp, pulse/src/main.cpp

## 2026-05-14 — feat(pulse): PulseState 交互系统 + QML 组件库 + 后端解析扩展
**Branch**: master
**Decisions**:
- PulseState 在 C++ 端推导，QML 通过 agentModel.globalState(string) 消费
- 用户决策走 $XDG_RUNTIME_DIR/pulse/<sid>/<iid>.response.json sidecar 文件，原子写入无需终端依赖
- QML module 通过 qmldir + addImportPath 注册，Theme singleton 与多组件一起声明
- MiniMd 轻量 Markdown→HTML 转换器内嵌 C++，避免引入外部依赖
**Files**: src/AgentInfo.h, src/AgentModel.cpp, src/AgentModel.h, src/ClaudeMetaReader.cpp, src/ClaudeMetaReader.h, src/CodexMetaReader.cpp, src/CodexMetaReader.h, src/ProcScanner.cpp, src/WaylandLayerShell.cpp, src/WaylandLayerShell.h, src/main.cpp, src/MiniMd.cpp, src/MiniMd.h, src/ResponseWriter.cpp, src/ResponseWriter.h, qml/ExpandedView.qml, qml/HeaderBar.qml, qml/Keycap.qml, qml/PermissionView.qml, qml/PlanView.qml, qml/QuestionView.qml, qml/Theme.qml, qml/qmldir, CMakeLists.txt, .gitignore

## 2026-05-14 — style(ui): 扁平化高亮 item + Header 无副标题时单行显示
**Branch**: master
**Decisions**:
- AgentRow 高亮改为扁平色块，去除渐变/阴影，视觉更干净
- Header 无副标题时折叠为单行，减少 padding 浪费
**Files**: qml/AgentRow.qml, qml/main.qml

## 2026-05-14 — 🔧 chore(makefile): 新增 mock-* 快捷目标，修复 Makefile 被 .gitignore 排除
**Branch**: master
**Decisions**:
- Makefile 新增 mock-idle/working/permission/question/plan/expanded 六个 PULSE_MOCK 场景目标
- .gitignore 末尾追加 !/Makefile 例外，覆盖 Qt 模板中的 Makefile* 忽略规则
**Files**: .gitignore, Makefile

## 2026-05-14 — 🐛 fix(resize): 乘以 devicePixelRatio 修复 QT_SCALE_FACTOR 下窗口无限收缩
**Branch**: master
**Decisions**:
- Hyprland resizewindowpixel exact 接受物理像素，而 Qt QWindow::width()/height() 在 QT_SCALE_FACTOR!=1 时返回逻辑像素；两处 resizeWindow 调用均需乘以 devicePixelRatio 转换
- 选用 qRound 而非截断，避免累积误差导致 off-by-one
**Bugs**:
- {'symptom': 'QT_SCALE_FACTOR=1.2 时 widget 持续收缩直到消失', 'root_cause': '逻辑像素传给 Hyprland 物理像素接口，导致窗口实际变小，Qt 读回更小的逻辑值，heightChanged 再次触发，形成正反馈收缩循环', 'fix': '在两处 resizeWindow 调用点乘以 window->devicePixelRatio()'}
**Files**: src/main.cpp

## 2026-05-14 — chore(docs): 移除 Tauri/bun 残留，更新文档至 Qt6/CMake 技术栈
**Branch**: master
**Decisions**:
- 项目已从 Tauri 2 + Rust 迁移至 C++ + Qt6 + QML + CMake，CLAUDE.md 和 workflow.md 仍记录旧技术栈
- 移除 .claude/settings.local.json 中 bun run * / bunx * 权限条目
- 保留 openspec/changes/ 历史文档作为技术迁移决策归档
**Files**: CLAUDE.md, .context/prefs/workflow.md, .claude/settings.local.json

## 2026-05-14 — ♻️ refactor(ui): 移除 Flickable，input 随 widget 高度自动伸展
**Branch**: master
**Decisions**:
- 移除 Flickable + ScrollBar：Flickable.contentHeight 始终 >= height 导致滚动条无条件显示
- 改为 anchor-based 布局：infoCol 固定顶部，noteBox 通过 anchors.top/bottom 填满剩余空间
- TextEdit 随 widget 高度同步扩展，widget 越高 input 越大，符合用户调整窗口的直觉
**Files**: qml/PermissionView.qml

## 2026-05-15 — ✨ feat(ui): 在 ExpandedView 每行底部显示 context 使用量细进度条
**Branch**: master
**Decisions**:
- contextUsed = input_tokens + cache_creation_input_tokens + cache_read_input_tokens（来自 JSONL transcript 最近 assistant 行的 usage 字段）
- Claude Code tail 从 8192 升至 32768：实测当前 busy session 最后 usage 行距 EOF 8757B，8192 会 miss
- Codex 从 token_count event_msg 解析 last_token_usage.input_tokens / model_context_window，tail 从 4096 升至 16384
- 进度条颜色阈值：>90% → Theme.coral，>70% → #f59e0b，其余 → Theme.accent；width/color 各有 Behavior 动画
- contextLimit 在 ClaudeMetaReader 中硬编码为 kClaudeContextLimit=200000（所有当前 Claude 模型均为 200k）
- bar 叠加在 bottom separator 之上（height:2，visible 仅在 contextUsed>0），行高保持 62px 不变
**Files**: src/AgentInfo.h, src/ClaudeMetaReader.h, src/ClaudeMetaReader.cpp, src/CodexMetaReader.h, src/CodexMetaReader.cpp, src/AgentModel.h, src/AgentModel.cpp, src/main.cpp, qml/ExpandedView.qml
