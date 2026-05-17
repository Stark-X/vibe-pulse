# Proposal: Cross-Platform Support (macOS + Windows)

## Overview

将 Pulse 从 Linux-only (Ubuntu + Hyprland/Wayland) 扩展为跨平台应用，支持 macOS（优先）和 Windows。保持完整功能对等：进程检测、浮层窗口、tmux 集成、PPID 链路追踪。

## Enhanced Requirement

**目标**：Pulse 在 macOS 和 Windows 上完整运行，具备与 Linux 等价的核心功能：
- AI agent 进程检测（Claude Code、Codex、OpenCode）
- 浮层覆盖窗口（始终置顶，定位屏幕右上角）
- tmux 集成（macOS 原生 tmux；Windows via WSL + Windows Terminal）
- PPID 链路追踪（进程亲缘关系解析）
- 订阅状态监控（网络请求，跨平台无需修改）

**优先级**：macOS 先行，Windows 后跟进。

**窗口浮层机制**：非 Linux 平台使用原生 API（macOS: NSWindow level；Windows: SetWindowPos HWND_TOPMOST）。

## Discovered Constraints

### Hard Constraints（必须解决）

1. **`/proc` 文件系统**：`ProcScanner.cpp`、`TmuxResolver.cpp`、`HyprlandClient.cpp` 均读取 `/proc/<pid>/status|comm|exe|cwd|cmdline`，macOS/Windows 无此接口。
2. **wlr-layer-shell（Wayland 协议）**：`WaylandLayerShell.cpp`、`LayerShell.cpp`、`protocols/` 目录仅在 Linux/Wayland 存在，必须用 `#ifdef Q_OS_LINUX` 隔离。
3. **CMakeLists.txt 中 wayland-client 强依赖**：`pkg_check_modules(WAYLAND_CLIENT REQUIRED ...)` 和 `find_program(WAYLAND_SCANNER_EXECUTABLE REQUIRED)` 在非 Linux 下会直接失败，需改为条件编译。
4. **Qt6::GuiPrivate（Wayland 内部接口）**：`nativeResourceForWindow`/`nativeResourceForIntegration` 是 Wayland-specific，必须移入 Linux-only 代码路径。
5. **pulse-toggle Unix socket**：macOS 支持 POSIX Unix socket（可复用）；Windows 需用 QLocalSocket（Qt 跨平台抽象）替代或用命名管道。

### Soft Constraints（已有良好基础）

1. **WindowManager 工厂模式已存在**：`WindowManager::create()` 工厂 + `NullWindowManager` 已抽象 Hyprland IPC；只需添加 macOS/Windows 实现。
2. **CMake 现代风格**：已用 target-based CMake，添加平台条件块容易。
3. **Qt6 本身跨平台**：QProcess、QFile、QTimer、QML、Qt6::Network 等均原生支持三平台。
4. **SubscriptionMonitor**：纯 HTTP 请求（Qt6::Network），无需修改。
5. **MiniMd、ResponseWriter、Settings、AgentModel**：平台无关，无需修改。

### Dependencies（实施顺序约束）

```
CMakeLists.txt 条件化
    ↓
ProcScanner 平台抽象
    ↓
TmuxResolver 平台抽象（依赖 ProcScanner PPID 接口）
    ↓
WaylandLayerShell / 窗口浮层抽象
    ↓
WindowManager 添加 macOS/Windows 实现
    ↓
HyprlandClient PPID 路径（移入 Linux-only）
```

## Platform-Specific Approach

### macOS 进程扫描（libproc / sysctl）

| 需求 | Linux | macOS |
|------|-------|-------|
| 进程列表 | `readdir /proc` | `proc_listallpids()` from `<libproc.h>` |
| comm（进程名） | `/proc/<pid>/comm` | `proc_pidinfo(pid, PROC_PIDTBSDINFO)` → `pbi_comm` |
| exe 路径 | `/proc/<pid>/exe` symlink | `proc_pidpath(pid, buf, size)` |
| CWD | `/proc/<pid>/cwd` symlink | `proc_pidvnodepathinfo()` → `pvi_cdir.vip_path` |
| cmdline | `/proc/<pid>/cmdline` | `sysctl([CTL_KERN, KERN_PROCARGS2, pid])` + 解析 |
| PPID | `/proc/<pid>/status` PPid 字段 | `proc_pidinfo(pid, PROC_PIDTBSDINFO)` → `pbi_ppid` |

### macOS 窗口浮层（Objective-C++）

使用 `.mm` 文件（Objective-C++ 混合模式）：
- `[window setLevel: NSStatusWindowLevel + 1]` — 浮于所有普通窗口之上
- `[window setCollectionBehavior: NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary]` — 跨 Space 可见，不参与 Mission Control
- 位置：`NSScreen.mainScreen.frame` 右上角，减去窗口尺寸 + 边距

### Windows 进程扫描（Win32 API）

| 需求 | Windows API |
|------|------------|
| 进程列表 + PPID | `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` + `PROCESSENTRY32` |
| exe 路径 | `OpenProcess() + QueryFullProcessImageName()` |
| CWD | `NtQueryInformationProcess` 读 PEB（复杂，降级为空字符串） |
| cmdline | `NtQueryInformationProcess` 读 PEB `ProcessCommandLine`（Windows 10+）|

### Windows 窗口浮层（Win32）

Qt 创建窗口后，通过 `QWindow::winId()` 获取 HWND：
```cpp
SetWindowPos(hwnd, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
// 隐藏任务栏图标
LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TOOLWINDOW);
```

### tmux（macOS + Windows WSL）

macOS：`/proc` 换为 libproc PPID 链 + `comm` 匹配 `tmux`，其余 `tmux list-panes` IPC 调用不变。

Windows（WSL）：`wsl.exe -- tmux list-panes -a -F "..."` 透传调用；PPID 链跨 WSL 边界不连续，仅 best-effort。

## File Change Map

```
CMakeLists.txt          — 条件化 Wayland 依赖；添加 macOS libproc、Windows psapi/ntdll
src/ProcScanner.h       — 接口不变（或提取抽象基类）
src/ProcScanner_linux.cpp  — 当前实现重命名
src/ProcScanner_macos.cpp  — 新建：libproc/sysctl 实现
src/ProcScanner_win.cpp    — 新建：Win32 PSAPI 实现
src/TmuxResolver.h      — 接口不变
src/TmuxResolver_linux.cpp — 当前实现重命名
src/TmuxResolver_macos.cpp — 新建：macOS PPID via libproc
src/TmuxResolver_win.cpp   — 新建：WSL best-effort
src/WaylandLayerShell.h/cpp — 包裹 #ifdef Q_OS_LINUX
src/LayerShell.h/cpp    — 包裹 #ifdef Q_OS_LINUX
src/WindowOverlay.h     — 新建：跨平台浮层接口
src/WindowOverlay_linux.cpp  — WaylandLayerShell wrapper
src/WindowOverlay_macos.mm   — 新建：NSWindow level 实现
src/WindowOverlay_win.cpp    — 新建：Win32 SetWindowPos 实现
src/WindowManager.cpp   — NullWindowManager 已够用；可添加 MacOSWindowManager 用于 focus
src/HyprlandClient.cpp  — ppidOf() 移入 Linux-only ifdef
src/main.cpp            — 平台条件初始化 WindowOverlay
```

## Risks & Mitigations

| 风险 | 概率 | 缓解 |
|------|------|------|
| macOS TCC 限制：访问其他进程信息 | 中 | 开发者模式下 `proc_pidinfo` 无需授权；分发版本需 Hardened Runtime + 临时例外权限 |
| macOS Sequoia 窗口层级变化 | 低 | 使用 `NSScreenSaverWindowLevel` 或 `kCGStatusWindowLevel` 作备选 |
| Windows CWD 难以读取 | 高 | 降级：CWD 显示空字符串，agent 名称仍可显示（通过 cmdline 解析） |
| WSL tmux PPID 链不连续 | 高 | best-effort：WSL 进程下 tmux 检测标记为 optional，失败时 tmuxTarget 为空 |
| Qt6 Wayland plugin 冲突 | 低 | 确保非 Linux 平台不链接 Qt6::WaylandClient |

## Success Criteria（可验证）

1. `cmake -S . -B build` 在 macOS 和 Windows 上无错误完成配置
2. `cmake --build build --target pulse` 编译成功，产出可执行二进制
3. 启动 `pulse` 后，浮层窗口出现在屏幕右上角，始终置顶
4. 同机运行 `claude` 命令行工具，5 秒内 Pulse 列表出现该 agent
5. `PULSE_MOCK=working` 环境变量注入 demo agent，UI 正常渲染
6. macOS 上：agent 在 tmux pane 内运行时，sessionName 字段正确解析
7. `pulse-toggle show/hide/toggle` 指令通过 IPC 正常工作

## Out of Scope

- macOS App Store 分发（需 Sandbox entitlement，另立 PR）
- Windows MSI 打包
- Windows 下 Hyprland（不存在）
- ARM64 交叉编译（单独处理）
