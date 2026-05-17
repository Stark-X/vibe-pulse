# Design: Cross-Platform Support (macOS + Windows)

## Architecture Overview

```
src/
├── ProcessTree.h                 ← NEW: 共享 PPID/comm/exe/cwd 接口
├── ProcessTree_linux.cpp         ← /proc 实现（从现有代码提取）
├── ProcessTree_macos.cpp         ← libproc/sysctl 实现（新建）
├── ProcessTree_win.cpp           ← Win32 Toolhelp32 + QueryFullProcessImageName（新建）
│
├── ProcScanner.h                 ← 接口不变（static scanAll()）
├── ProcScanner.cpp               ← 平台无关扫描逻辑 + dedup（共享）
├── ProcScanner_linux.cpp         ← /proc 目录遍历（从 ProcScanner.cpp 拆出）
├── ProcScanner_macos.cpp         ← proc_listallpids 遍历（新建）
├── ProcScanner_win.cpp           ← CreateToolhelp32Snapshot 遍历（新建）
│
├── TmuxResolver.h                ← 接口不变
├── TmuxResolver_linux.cpp        ← /proc 路径（从 TmuxResolver.cpp 拆出）
├── TmuxResolver_macos.cpp        ← libproc PPID 链（新建）
├── TmuxResolver_win.cpp          ← wsl.exe best-effort（新建）
│
├── WindowOverlay.h               ← NEW: 跨平台浮层接口
├── WindowOverlay_linux.cpp       ← WaylandLayerShell wrapper（新建）
├── WindowOverlay_macos.mm        ← NSWindow level (Objective-C++)（新建）
├── WindowOverlay_win.cpp         ← HWND SetWindowPos TOPMOST（新建）
│
├── WaylandLayerShell.h/cpp       ← 保留，仅 Linux CMake 编译
├── LayerShell.h/cpp              ← 保留，仅 Linux CMake 编译
│
├── WindowManager.h               ← 接口不变
├── WindowManager.cpp             ← factory：Linux→Hyprland/Null, macOS→MacOS, Win→Null
├── HyprlandWindowManager.h/cpp   ← 仅 Linux 编译（无变化）
├── MacOSWindowManager.h          ← NEW: NSRunningApplication activate
├── MacOSWindowManager.mm         ← NEW: Objective-C++ 实现
├── NullWindowManager.h           ← 保留（Windows 默认）
│
├── HyprlandClient.h/cpp          ← 仅 Linux 编译；ppidOf() 改用 ProcessTree
│
└── main.cpp                      ← WindowOverlay::create() 替代直接 WaylandLayerShell
```

## ProcessTree Interface

```cpp
// src/ProcessTree.h
#pragma once
#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QVector>

class ProcessTree {
public:
    // Returns empty string if pid doesn't exist or access denied
    static QString comm(quint32 pid);
    static QString exe(quint32 pid);
    static QString cwd(quint32 pid);     // best-effort; may return empty
    static QStringList cmdline(quint32 pid);
    static quint32 ppid(quint32 pid);   // returns 0 if unknown

    // Walk PPID chain up to maxDepth; returns chain including agentPid
    // Stops at pid==0 or pid==1; never loops (cycle detection via visited set)
    static QVector<quint32> ancestorChain(quint32 agentPid, int maxDepth = 32);

    // Returns list of all active PIDs (main threads only, no kernel threads)
    static QVector<quint32> listAll();
};
```

## WindowOverlay Interface

```cpp
// src/WindowOverlay.h
#pragma once
#include <QRect>
class QWindow;

class WindowOverlay {
public:
    virtual ~WindowOverlay() = default;

    // Called once after QWindow is created and shown; platform sets level/layer
    virtual void setup(QWindow *win) = 0;

    // Called when overlay should reposition/resize (e.g., screen geometry change)
    virtual void updateGeometry(const QRect &rect) = 0;

    // Factory: returns platform-appropriate implementation
    static std::unique_ptr<WindowOverlay> create();
};
```

### macOS Implementation (WindowOverlay_macos.mm)

```objc
#import <AppKit/AppKit.h>
#include "WindowOverlay.h"
#include <QWindow>
#include <QGuiApplication>
#include <QScreen>

class MacOSWindowOverlay : public WindowOverlay {
    NSWindow *m_nswin = nil;
public:
    void setup(QWindow *win) override {
        m_nswin = reinterpret_cast<NSWindow *>(win->winId());
        [m_nswin setLevel: NSStatusWindowLevel + 1];
        [m_nswin setCollectionBehavior:
            NSWindowCollectionBehaviorCanJoinAllSpaces |
            NSWindowCollectionBehaviorStationary |
            NSWindowCollectionBehaviorIgnoresCycle];
        [m_nswin setHidesOnDeactivate: NO];
        // Position: top-right of primary screen
        QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        int margin = 12;
        win->setPosition(avail.right() - win->width() - margin,
                         avail.top() + margin);
    }
    void updateGeometry(const QRect &) override {
        // NSWindow level persists; reposition only if screen changes
    }
};
```

### Windows Implementation (WindowOverlay_win.cpp)

```cpp
#include "WindowOverlay.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <QWindow>
#include <QGuiApplication>
#include <QScreen>

class WindowsWindowOverlay : public WindowOverlay {
public:
    void setup(QWindow *win) override {
        HWND hwnd = reinterpret_cast<HWND>(win->winId());
        // Always-on-top + no taskbar icon
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
        SetWindowLong(hwnd, GWL_EXSTYLE,
                      (ex | WS_EX_TOOLWINDOW) & ~WS_EX_APPWINDOW);
        // Position top-right
        QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        int margin = 12;
        int x = avail.right() - win->width() - margin;
        int y = avail.top() + margin;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    void updateGeometry(const QRect &) override {}
};
```

## MacOSWindowManager (focus-on-click)

```objc
// MacOSWindowManager.mm
#import <AppKit/AppKit.h>
#include "MacOSWindowManager.h"

QString MacOSWindowManager::findWindowByPid(quint32 pid) const {
    return QString::number(pid);  // PID is the "window ID" on macOS
}

void MacOSWindowManager::focusWindow(const QString &windowId) {
    quint32 pid = windowId.toUInt();
    if (pid == 0) return;
    NSRunningApplication *app =
        [NSRunningApplication runningApplicationWithProcessIdentifier: pid];
    if (app) {
        [app activateWithOptions: NSApplicationActivateIgnoringOtherApps];
    }
}

void MacOSWindowManager::resizeWindow(const QString &, int, int) {
    // Not applicable on macOS
}
```

## pulse-toggle IPC 修复

```cpp
// 当前代码（需要替换）
QString socketPath = QStringLiteral("/tmp/pulse-%1.sock").arg(getuid());

// 替换为（跨平台）
QString serverName() {
    // Qt maps this to /tmp/pulse-ipc on Unix, \\.\pipe\pulse-ipc on Windows
    return QStringLiteral("pulse-ipc");
}
```

移除 `#include <unistd.h>` 和 `getuid()` 调用（两处：`src/main.cpp` 和 `pulse-toggle/main.cpp`）。

## CMakeLists.txt 结构

```cmake
cmake_minimum_required(VERSION 3.16)
project(pulse VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Network)

# ── Common sources ─────────────────────────────────────────────
set(PULSE_COMMON_SOURCES
    src/main.cpp
    src/AgentInfo.h
    src/AgentModel.h      src/AgentModel.cpp
    src/ProcessTree.h
    src/ProcScanner.h     src/ProcScanner.cpp      # shared logic (dedup, rules)
    src/TmuxResolver.h
    src/WindowManager.h   src/WindowManager.cpp
    src/NullWindowManager.h
    src/WindowOverlay.h
    src/ClaudeMetaReader.h src/ClaudeMetaReader.cpp
    src/CodexMetaReader.h  src/CodexMetaReader.cpp
    src/Settings.h        src/Settings.cpp
    src/MiniMd.h          src/MiniMd.cpp
    src/ResponseWriter.h  src/ResponseWriter.cpp
    src/SubscriptionMonitor.h src/SubscriptionMonitor.cpp
)

set(PULSE_PLATFORM_SOURCES)
set(PULSE_PLATFORM_LIBS)

# ── Linux / Wayland ────────────────────────────────────────────
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
    find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner
                 HINTS /home/linuxbrew/.linuxbrew/bin REQUIRED)

    set(PULSE_PROTOCOL_XML   ${CMAKE_CURRENT_SOURCE_DIR}/protocols/wlr-layer-shell-unstable-v1.xml)
    set(PULSE_PROTOCOL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-client-protocol.h)
    set(PULSE_PROTOCOL_CODE   ${CMAKE_CURRENT_BINARY_DIR}/wlr-layer-shell-unstable-v1-protocol.c)
    add_custom_command(
        OUTPUT ${PULSE_PROTOCOL_HEADER} ${PULSE_PROTOCOL_CODE}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${PULSE_PROTOCOL_XML} ${PULSE_PROTOCOL_HEADER}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code   ${PULSE_PROTOCOL_XML} ${PULSE_PROTOCOL_CODE}
        DEPENDS ${PULSE_PROTOCOL_XML} VERBATIM)

    list(APPEND PULSE_PLATFORM_SOURCES
        src/ProcessTree_linux.cpp
        src/ProcScanner_linux.cpp
        src/TmuxResolver_linux.cpp
        src/WaylandLayerShell.h   src/WaylandLayerShell.cpp
        src/WindowOverlay_linux.cpp
        src/HyprlandClient.h      src/HyprlandClient.cpp
        src/HyprlandWindowManager.h src/HyprlandWindowManager.cpp
        ${PULSE_PROTOCOL_CODE} ${PULSE_PROTOCOL_HEADER})
    list(APPEND PULSE_PLATFORM_LIBS Qt6::GuiPrivate PkgConfig::WAYLAND_CLIENT)

# ── macOS ──────────────────────────────────────────────────────
elseif(APPLE)
    enable_language(OBJCXX)
    find_library(APPKIT_FW   AppKit    REQUIRED)
    find_library(FOUNDATION_FW Foundation REQUIRED)

    list(APPEND PULSE_PLATFORM_SOURCES
        src/ProcessTree_macos.cpp
        src/ProcScanner_macos.cpp
        src/TmuxResolver_macos.cpp
        src/WindowOverlay_macos.mm
        src/MacOSWindowManager.h
        src/MacOSWindowManager.mm)
    list(APPEND PULSE_PLATFORM_LIBS ${APPKIT_FW} ${FOUNDATION_FW})

# ── Windows ────────────────────────────────────────────────────
elseif(WIN32)
    list(APPEND PULSE_PLATFORM_SOURCES
        src/ProcessTree_win.cpp
        src/ProcScanner_win.cpp
        src/TmuxResolver_win.cpp
        src/WindowOverlay_win.cpp)
    list(APPEND PULSE_PLATFORM_LIBS psapi)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# ── Targets ────────────────────────────────────────────────────
qt_add_executable(pulse ${PULSE_COMMON_SOURCES} ${PULSE_PLATFORM_SOURCES})

target_link_libraries(pulse PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network
    ${PULSE_PLATFORM_LIBS})

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_include_directories(pulse PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

# pulse-toggle (QLocalSocket-only, no platform libs needed)
qt_add_executable(pulse-toggle pulse-toggle/main.cpp)
target_link_libraries(pulse-toggle PRIVATE Qt6::Core Qt6::Network)

qt_add_resources(pulse pulse_qml PREFIX "/" FILES
    qml/qmldir qml/main.qml qml/Theme.qml qml/AgentRow.qml
    qml/HeaderBar.qml qml/QuestionView.qml qml/PlanView.qml
    qml/PermissionView.qml qml/ExpandedView.qml qml/Keycap.qml
    assets/claude-code.png assets/codex.png)
```

## ProcessTree macOS 实现关键代码

```cpp
// src/ProcessTree_macos.cpp
#include "ProcessTree.h"
#include <libproc.h>
#include <sys/sysctl.h>
#include <sys/proc_info.h>

QVector<quint32> ProcessTree::listAll() {
    int n = proc_listallpids(nullptr, 0);
    if (n <= 0) return {};
    QVector<pid_t> buf(n + 16);
    n = proc_listallpids(buf.data(), buf.size() * sizeof(pid_t));
    QVector<quint32> result;
    for (int i = 0; i < n; ++i)
        if (buf[i] > 0) result.append(static_cast<quint32>(buf[i]));
    return result;
}

QString ProcessTree::comm(quint32 pid) {
    struct proc_bsdinfo info{};
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) <= 0)
        return {};
    return QString::fromLocal8Bit(info.pbi_comm);
}

quint32 ProcessTree::ppid(quint32 pid) {
    struct proc_bsdinfo info{};
    if (proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) <= 0)
        return 0;
    return static_cast<quint32>(info.pbi_ppid);
}

QString ProcessTree::exe(quint32 pid) {
    char buf[PROC_PIDPATHINFO_MAXSIZE] = {};
    if (proc_pidpath(pid, buf, sizeof(buf)) <= 0) return {};
    return QString::fromLocal8Bit(buf);
}

QString ProcessTree::cwd(quint32 pid) {
    struct proc_vnodepathinfo vpi{};
    if (proc_pidinfo(pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi)) <= 0)
        return {};
    return QString::fromLocal8Bit(vpi.pvi_cdir.vip_path);
}
```

## ProcessTree Windows 实现关键代码

```cpp
// src/ProcessTree_win.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include "ProcessTree.h"

QVector<quint32> ProcessTree::listAll() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return {};
    QVector<quint32> result;
    PROCESSENTRY32W pe{ .dwSize = sizeof(PROCESSENTRY32W) };
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        result.append(pe.th32ProcessID);
    CloseHandle(snap);
    return result;
}

quint32 ProcessTree::ppid(quint32 pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ .dwSize = sizeof(PROCESSENTRY32W) };
    quint32 result = 0;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (pe.th32ProcessID == pid) { result = pe.th32ParentProcessID; break; }
    CloseHandle(snap);
    return result;
}

QString ProcessTree::exe(quint32 pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD sz = MAX_PATH;
    bool ok = QueryFullProcessImageNameW(h, 0, buf, &sz);
    CloseHandle(h);
    return ok ? QString::fromWCharArray(buf, sz) : QString{};
}

QString ProcessTree::comm(quint32 pid) {
    QString e = exe(pid);
    return e.isEmpty() ? QString{} : QFileInfo(e).completeBaseName();
}

// cwd: graceful omission
QString ProcessTree::cwd(quint32) { return {}; }
```

## TmuxResolver_win.cpp (WSL best-effort)

```cpp
std::optional<TmuxPaneInfo> TmuxResolver::findPaneInfo(quint32 agentPid) {
    // Check if agent is in WSL ancestry (parent chain includes wsl.exe)
    auto chain = ProcessTree::ancestorChain(agentPid);
    bool inWsl = false;
    for (quint32 p : chain)
        if (ProcessTree::comm(p).toLower() == "wsl.exe") { inWsl = true; break; }
    if (!inWsl) return std::nullopt;

    // Query tmux via wsl.exe proxy
    QProcess panes;
    panes.start(QStringLiteral("wsl.exe"),
        {"--", "tmux", "list-panes", "-a", "-F",
         "#{pane_pid}\t#{session_id}\t#{session_name}\t#{window_index}\t#{pane_index}"});
    // ... same matching logic as Linux version ...
}
```

## Makefile 更新

```makefile
# 平台检测
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(UNAME),Darwin)
    BUILD_DIR := build_mac
    RUN_BIN   := ./$(BUILD_DIR)/pulse
    CMAKE_OPTS :=
else ifeq ($(UNAME),Linux)
    BUILD_DIR := build_rel
    RUN_BIN   := ./$(BUILD_DIR)/pulse
    CMAKE_OPTS :=
else
    BUILD_DIR := build_win
    RUN_BIN   := ./$(BUILD_DIR)/Release/pulse.exe
    CMAKE_OPTS := -G "Visual Studio 17 2022"
endif

configure:
    cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release $(CMAKE_OPTS)

build: configure
    cmake --build $(BUILD_DIR) --target pulse

run: build
    $(RUN_BIN)
```
