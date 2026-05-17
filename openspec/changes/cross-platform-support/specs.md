# Specs: Cross-Platform Support (macOS + Windows)

## Resolved Constraints

| # | 约束 | 决定 |
|---|------|------|
| C1 | macOS 发布方式 | 非沙盒直接运行；libproc/sysctl 可完整访问同用户进程，无需额外 entitlement |
| C2 | macOS focus-on-click | 必须支持；使用 `NSRunningApplication activateWithOptions` 激活 terminal（无需 Accessibility 权限） |
| C3 | Windows agents | Native Win32（Toolhelp32）+ WSL 内进程（best-effort 通过 wsl.exe 代理）均支持 |
| C4 | Windows CWD | Graceful omission；CWD 为空时 agent name 降级为 exe basename |
| C5 | WindowOverlay 架构 | 独立于 WindowManager；接口 `setup(QWindow*)` + `updateGeometry(QRect)` + static factory |
| C6 | 共享 PPID 模块 | 新增 `ProcessTree.{h,cpp}`（平台各一份）；ProcScanner/TmuxResolver/HyprlandClient 均使用 |
| C7 | pulse-toggle IPC | 替换 `getuid()+unistd.h+/tmp` 为 `QLocalSocket`（Qt 自动映射：Unix socket / Windows named pipe） |
| C8 | ProcScanner 分割 | suffix-based 平台文件（`_linux`/`_macos`/`_win`），不用大块 `#ifdef` |
| C9 | macOS proc APIs | `proc_listallpids`→PID list；`proc_pidinfo(PROC_PIDTBSDINFO)`→comm/PPID；`proc_pidpath`→exe；`proc_pidinfo(PROC_PIDVNODEPATHINFO)`→CWD(best-effort)；`sysctl(KERN_PROCARGS2)`→cmdline |
| C10 | Windows proc APIs | `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)`→list+PPID；`QueryFullProcessImageName`→exe；CWD omit |
| C11 | WSL agent 检测 | 在 Win32 进程链中识别祖先为 `wsl.exe` 的进程；通过 `wsl.exe -- cat /proc/<wsl_pid>/comm` 等代理查询内部信息 |

## Verification Conditions（可机械验证）

### Build

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` 在 macOS 上退出码 = 0，无 WAYLAND/PkgConfig 相关错误
- [ ] `cmake --build build --target pulse` 在 macOS 上退出码 = 0，产出可执行二进制
- [ ] 同上两条在 Windows 上成立，且无 `<unistd.h>` 编译错误

### Runtime - macOS

- [ ] `./pulse` 启动后，浮层窗口出现在主屏幕右上角（距边缘 ≤ 20 pt），窗口始终置于所有普通窗口之上
- [ ] `PULSE_MOCK=working ./pulse` 注入 demo agent，UI 正常渲染（不依赖 /proc）
- [ ] `PULSE_MOCK=permission ./pulse`、`PULSE_MOCK=question ./pulse`、`PULSE_MOCK=plan ./pulse` 均正常渲染
- [ ] 同机运行 `claude` CLI，5 秒内 Pulse 列表显示该 agent，toolType 为 "Claude Code"
- [ ] agent 在 tmux pane 内运行时，`sessionName` 字段非空，正确显示 tmux session 名称
- [ ] 点击 agent 行，终端窗口获得焦点（如果 `NSRunningApplication activateWithOptions` 成功）
- [ ] `pulse-toggle show` / `pulse-toggle hide` / `pulse-toggle toggle` 命令正常控制窗口可见性
- [ ] 连续发送 100 次 toggle 命令无 crash

### Runtime - Windows

- [ ] 运行 `pulse.exe`，浮层窗口出现在主屏幕右上角，`WS_EX_TOPMOST` 置顶
- [ ] `PULSE_MOCK=working pulse.exe` 注入 demo agent，UI 正常渲染
- [ ] native Win32 环境下运行 `claude.exe`（若存在），5 秒内 Pulse 检测到
- [ ] WSL 内运行 `claude`，best-effort 检测（失败不影响 native 功能）

## PBT Properties（不变量）

| 属性 | 不变量 | 边界条件 | 伪造策略 |
|------|--------|----------|----------|
| ProcessTree.comm 幂等 | 对同一 pid 连续调用返回相同字符串（进程存活期间）| pid=0, pid=1, 不存在的 pid | 随机 pid → 结果不崩溃，要么有效字符串要么空 |
| ProcessTree.ancestorChain 深度有界 | 链路长度 ≤ 32 层，遇到 pid=0/1 停止 | 环形 ppid（理论不存在但实现须防止）| 构造深度 > 32 的链 → 函数在 32 步内返回 |
| ProcScanner.scanAll 稳定性 | 同一进程存活时，每次扫描均包含该 pid | 进程在扫描中途退出 | 并发 kill + scan，结果不崩溃 |
| ProcScanner.dedup 收敛 | 同 toolType 下，子进程不出现在父进程已存在时 | 无父进程时 dedup 不删除唯一进程 | 构造父子 agent 对 → dedup 删除子进程 |
| WindowOverlay 位置约束 | 窗口矩形完全在 `QScreen::availableGeometry()` 内 | 多显示器切换、分辨率变化 | 极小/极大 DPI → 位置仍合法 |
| pulse-toggle IPC 幂等 | 连续发送相同命令（show/show/show）结果等价于一次 | 并发两个 toggle 进程 | 随机发送顺序的命令序列 → 最终状态一致 |
| TmuxResolver 无 tmux 时安全 | agent 不在 tmux 内时返回 nullopt，不挂起 | tmux 二进制不存在、tmux daemon 未运行 | timeout 1s 后 findPaneInfo 必须返回 nullopt |

## 不在本次 Scope 内

- macOS App Store / Sandbox 支持
- Windows MSI/安装包打包
- ARM64 交叉编译
- macOS Accessibility TCC 权限引导 UI（focus-on-click 失败时静默降级）
- WSL2 tmux focus（仅检测，不 focus）
