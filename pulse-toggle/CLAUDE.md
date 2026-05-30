[根目录](../CLAUDE.md) > **pulse-toggle**

# pulse-toggle/ — IPC 控制工具

命令行客户端，通过 QLocalSocket IPC 控制正在运行的 pulse 进程（显示/隐藏/切换）。

---

## 变更记录 (Changelog)

| 日期 | 变更 |
|------|------|
| 2026-05-30 | 初次生成模块文档 |

---

## 模块职责

向 `pulse` 主进程发送 JSON 指令，控制窗口可见性。适合绑定为全局快捷键。

---

## 入口

**`pulse-toggle/main.cpp`**

```
pulse-toggle [toggle|show|hide]
```

默认命令为 `toggle`。

---

## 对外接口

IPC 协议：

- 连接 `QLocalSocket("pulse-ipc")`（Linux: Unix socket，Windows: named pipe）
- 发送 JSON：`{"cmd": "toggle"}` / `{"cmd": "show"}` / `{"cmd": "hide"}`
- pulse 主进程接收后控制 `window->setVisible()`

---

## 关键依赖

- Qt6::Core、Qt6::Network（仅 QLocalSocket，无 GUI 依赖）
- 跨平台：Linux、macOS、Windows 使用相同代码（QLocalSocket 透明跨平台）

---

## 构建

```bash
make toggle    # cmake --build build[_mac] --target pulse-toggle && ./build/pulse-toggle toggle
```

CMakeLists.txt 中独立 target：

```cmake
qt_add_executable(pulse-toggle pulse-toggle/main.cpp)
target_link_libraries(pulse-toggle PRIVATE Qt6::Core Qt6::Network)
```

---

## 相关文件清单

```
pulse-toggle/
└── main.cpp    IPC 客户端，连接 pulse-ipc socket，发送命令
```
