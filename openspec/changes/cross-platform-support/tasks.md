# Tasks: Cross-Platform Support (macOS + Windows)

## Phase 1: ProcessTree 共享模块

- [x] 1.1 新建 `src/ProcessTree.h`：声明 `comm(pid)` / `exe(pid)` / `cwd(pid)` / `cmdline(pid)` / `ppid(pid)` / `ancestorChain(pid, maxDepth=32)` / `listAll()` 六个静态函数；添加 cycle-detection（visited set）保证 ancestorChain 不循环
- [x] 1.2 新建 `src/ProcessTree_linux.cpp`：从 `ProcScanner.cpp`、`TmuxResolver.cpp`、`HyprlandClient.cpp` 提取 `/proc/<pid>/` 读取逻辑，实现全部六个函数；不删除原文件（重构步骤在 1.5–1.7）
- [x] 1.3 新建 `src/ProcessTree_macos.cpp`：用 `proc_listallpids`/`proc_pidinfo(PROC_PIDTBSDINFO)`/`proc_pidpath`/`proc_pidinfo(PROC_PIDVNODEPATHINFO)`/`sysctl(KERN_PROCARGS2)` 实现全部六个函数；cwd/cmdline 失败时返回空值，不 crash
- [x] 1.4 新建 `src/ProcessTree_win.cpp`：用 `CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS)` + `QueryFullProcessImageName` 实现 listAll/ppid/exe/comm；cwd 返回空字符串；cmdline 返回空 QStringList（graceful omission）

## Phase 2: ProcScanner 统一实现（使用 ProcessTree）

- [x] 2.1 修改 `src/ProcScanner.cpp`：移除直接读取 `/proc` 的 readStatus/readComm/readExe/readCwd/readCmdline/ppidOf；scanAll() 改用 ProcessTree::listAll()/comm()/exe()/cwd()/cmdline()；dedup() 改用 ProcessTree::ppid()
- [x] 2.2 修改 `src/ProcScanner.h`：移除 private 的 readStatus/readComm/readExe/readCwd/readCmdline/ppidOf 声明；保留 dedup() 私有声明

## Phase 3: TmuxResolver 重构

- [x] 3.1 修改 `src/TmuxResolver.cpp`：将 commOf(pid)/ppidOf(pid) 替换为 ProcessTree::comm(pid)/ancestorChain()；去除 `/proc` 直接读取；Linux 和 macOS 共用此实现
- [x] 3.2 新建 `src/TmuxResolver_win.cpp`：检测祖先链是否包含 `wsl.exe`；通过 `wsl.exe -- tmux list-panes ...` 代理；best-effort

## Phase 4: HyprlandClient 依赖清理

- [x] 4.1 修改 `src/HyprlandClient.cpp`：移除局部 ppidOf()，改用 ProcessTree::ppid()；移除 `/proc` 文件读取
- [x] 4.2 修改 `src/WindowManager.cpp`：HyprlandClient/HyprlandWindowManager include 放入 `#ifdef Q_OS_LINUX` 块；添加 `#ifdef Q_OS_MACOS` 返回 MacOSWindowManager

## Phase 5: WindowOverlay 抽象层

- [x] 5.1 新建 `src/WindowOverlay.h`：声明抽象基类，`virtual void setup(QWindow*)` 纯虚函数，以及 `static std::unique_ptr<WindowOverlay> create()` 工厂
- [x] 5.2 新建 `src/WindowOverlay_linux.cpp`：包裹 WaylandLayerShell 的实现
- [x] 5.3 新建 `src/WindowOverlay_macos.mm`：NSWindow level + collectionBehavior + 右上角定位
- [x] 5.4 新建 `src/WindowOverlay_win.cpp`：HWND TOPMOST + WS_EX_TOOLWINDOW + 右上角定位
- [x] 5.5 修改 `src/main.cpp`：用 WindowOverlay::create()+setup() 替代直接 WaylandLayerShell；移除 `<unistd.h>`；IPC 改用 "pulse-ipc" 名称

## Phase 6: macOS WindowManager (focus-on-click)

- [x] 6.1 新建 `src/MacOSWindowManager.h`：继承 WindowManager；findWindowByPid() 返回 pid 字符串；focusWindow() 接收 pid 字符串
- [x] 6.2 新建 `src/MacOSWindowManager.mm`：NSRunningApplication activateWithOptions

## Phase 7: pulse-toggle IPC 跨平台化

- [x] 7.1 修改 `src/main.cpp`：QLocalServer::listen("pulse-ipc")；移除 getuid()/unistd.h
- [x] 7.2 修改 `pulse-toggle/main.cpp`：QLocalSocket connectToServer("pulse-ipc")；移除 unistd.h/getuid()；新增 cmd 参数支持（show/hide/toggle）

## Phase 8: CMakeLists.txt 重构

- [x] 8.1 Wayland 依赖（PkgConfig/wayland-client/wayland-scanner/协议生成）移入 Linux 条件块
- [x] 8.2 PULSE_COMMON_SOURCES + PULSE_PLATFORM_SOURCES 分离；平台特定源文件各进各块
- [x] 8.3 添加 macOS 条件块：enable_language(OBJCXX)；AppKit/Foundation framework 链接；.mm 文件
- [x] 8.4 添加 Windows 条件块：psapi 链接；WIN32_LEAN_AND_MEAN/NOMINMAX 定义
- [x] 8.5 target_include_directories BINARY_DIR 仅 Linux 块保留

## Phase 9: Makefile 更新

- [x] 9.1 平台检测：Darwin/Linux/Windows 各设 BUILD/BIN/TOGGLE
- [x] 9.2 configure/build/run/toggle/mock-* 均使用 $(BUILD)/$(BIN) 变量

## Phase 10: 验证

- [x] 10.1 Linux: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` 成功（无 configure 错误）
- [x] 10.2 Linux: `cmake --build build --target pulse` 编译成功
- [x] 10.3 Linux: `PULSE_MOCK=working ./build/pulse` 启动（timeout 124 = 正常运行被 kill）
- [x] 10.4 Linux: `cmake --build build --target pulse-toggle` 编译成功
- [ ] 10.5 macOS: cmake configure + build 验证（需 macOS 环境）
- [ ] 10.6 Windows: cmake configure + build 验证（需 Windows 环境）
