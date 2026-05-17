# Notch Fusion Widget — Design

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│                         NotchFusionWidget                            │
│  ┌────────────┐  ┌────────────────────┐  ┌────────────┐            │
│  │  Left Zone  │  │   Notch (camera)   │  │ Right Zone  │            │
│  │   ● 3      │  │   transparent      │  │  ○ 85%     │            │
│  │  (count)   │  │                    │  │  (gauge)   │            │
│  └────────────┘  └────────────────────┘  └────────────┘            │
│  ← leftAreaW →   ←   notchWidth   →       ← rightAreaW →          │
│                                                                      │
│  Expanded (Agent List):                                              │
│  ┌────────────┐  ┌────────────────────┐  ┌────────────┐            │
│  │   ● 3      │  │   transparent      │  │  ○ 85%     │            │
│  ├────────────┤  ├────────────────────┤  ├────────────┤            │
│  │ ● vibe-is  │  │                    │  │            │            │
│  │ ● nova-sdk │  │                    │  │            │            │
│  │ ● deepbank │  │                    │  │            │            │
│  └────────────┘  └────────────────────┘  └────────────┘            │
└──────────────────────────────────────────────────────────────────────┘
```

## Component Design

### 1. NotchFusionWidget.qml

全屏宽度透明窗口，融合所有 macOS 功能。

```qml
Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    width: Screen.width
    height: animatedH

    // Mode state
    property string fusionMode: "compact"  // "compact" | "list" | "detail"
    property string detailMode: ""         // "permission" | "question" | "plan"

    // Geometry from NotchGeometry
    property var geo: notchGeometry ? notchGeometry.screenPositions[0] : null

    // Height management
    property real animatedH: 32
    property real targetH: {
        if (fusionMode === "compact") return 32
        if (fusionMode === "list")    return 32 + Math.min(listContent.implicitHeight, 280) + 32
        if (fusionMode === "detail")  return 32 + detailContent.implicitHeight
        return 32
    }

    // Auto-expand for attention states
    Connections {
        target: agentModel
        function onGlobalStateChanged() {
            var s = agentModel.globalState
            if (s === "permission" || s === "question" || s === "plan") {
                detailMode = s
                fusionMode = "detail"
            } else if (fusionMode === "detail") {
                fusionMode = "compact"
                detailMode = ""
            }
        }
    }

    // Click handler for mode cycling
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (fusionMode === "compact") fusionMode = "list"
            else if (fusionMode === "list") fusionMode = "compact"
            // "detail" mode auto-recovers, no click cycle
        }
    }

    // Left zone content
    Item {
        id: leftZone
        x: geo ? geo.notchLeftX - leftZone.implicitWidth - 4 : 8
        y: 0
        width: 64; height: 32
        // Status dot + agent count (migrated from NotchLeftHUD.qml)
    }

    // Right zone content
    Item {
        id: rightZone
        x: geo ? geo.notchRightX + 4 : parent.width - 68
        y: 0
        width: 60; height: 32
        // Usage arc gauge (migrated from NotchRightHUD.qml)
    }

    // Expanded content below top band
    Loader {
        id: expandedContent
        anchors.top: parent.top
        anchors.topMargin: 32
        anchors.left: parent.left
        anchors.right: parent.right
        active: fusionMode !== "compact"
        sourceComponent: {
            if (fusionMode === "list")   return listComp
            if (fusionMode === "detail") return detailComp
            return null
        }
    }
}
```

### 2. NotchGeometry Extension

**ScreenHudPos 新增字段**:
```cpp
struct ScreenHudPos {
    // Existing
    QString screenName;
    qreal screenX = 0;
    qreal screenY = 0;
    qreal leftX = 0;       // left HUD X (kept for backward compat)
    qreal rightX = 0;      // right HUD X (kept for backward compat)
    qreal y = 0;
    bool  hasNotch = false;

    // New for fusion widget
    qreal screenWidth = 0;
    qreal safeTop = 0;
    qreal leftAreaWidth = 0;
    qreal rightAreaWidth = 0;
    qreal notchLeftX = 0;    // = screenX + leftAreaWidth
    qreal notchRightX = 0;   // = screenX + screenWidth - rightAreaWidth
};
```

**QML 暴露**: 在 `screenPositions()` 的 QVariantMap 中新增所有字段。

**坐标转换统一**: 使用 `[[NSScreen screens] firstObject]` 替代 `[NSScreen mainScreen]`。

### 3. WindowOverlay Modifications

**OverlayOptions 扩展**:
```cpp
struct OverlayOptions {
    enum Role { MainCard, NotchLeftHud, NotchRightHud, NotchFusionWidget };
    Role role = MainCard;
    bool ignoresMouseEvents = false;
};
```

**新增 API — setHitTestRegions**:
```cpp
class WindowOverlay {
public:
    // ... existing ...
    virtual void setHitTestRegions(QWindow *win, const QVector<QRectF> &regions);
};
```

**macOS hitTest 实现**:

使用 method swizzle（与 constrainFrameRect 相同模式）override `hitTest:`:

```objc
// Swizzle hitTest: on the fusion window's content NSView
static void swizzleHitTest(NSView *view) {
    // Associated object: mutable array of CGRectF hit regions
    // When hitTest: is called:
    //   1. Convert AppKit point to Qt coordinates
    //   2. Check if point is inside any registered hit region
    //   3. If yes → return [super hitTest:point] (accept event)
    //   4. If no  → return nil (pass through to menu bar)
}
```

**区域管理**:
- 紧凑模式：左侧 (leftX, 0, 64, 32) + 右侧 (rightX, 0, 60, 32)
- 列表模式：左侧 + 右侧 + 展开区域 (0, 32, screenWidth, expandedHeight)
- 详情模式：左侧 + 右侧 + 详情区域

QML 通过 C++ context method 调用 `overlay->setHitTestRegions()` 更新区域。

### 4. main.cpp Modifications

**关键变更**:
```cpp
#ifdef Q_OS_MACOS
// Create fusion widget instead of MainCard + HUD pair
QQmlComponent fusionComp(&engine, QUrl("qrc:/qml/NotchFusionWidget.qml"));
QObject *fusionRoot = fusionComp.beginCreate(engine.rootContext());
auto *fusionWin = qobject_cast<QWindow *>(fusionRoot);

OverlayOptions fusionOpts;
fusionOpts.role = OverlayOptions::NotchFusionWidget;
overlay->setup(fusionWin, fusionOpts);
fusionComp.completeCreate();
fusionWin->show();

// Position from NotchGeometry
overlay->placeNotchHud(fusionWin, positions[0].screenX, positions[0].y);
fusionWin->setWidth(positions[0].screenWidth);

// Skip MainCard creation on macOS
// Skip NotchLeftHUD/NotchRightHUD creation

// IPC controls fusion window only
auto setAllVisible = [&](bool v) { fusionWin->setVisible(v); };
#else
// Linux/Windows: existing MainCard creation
#endif
```

### 5. Platform Branching Strategy

**编译时**: `#ifdef Q_OS_MACOS` 保护融合 Widget 代码
**运行时**: NotchGeometry 的 `available` 属性决定是否启用融合模式
**QML 层**: macOS 加载 `NotchFusionWidget.qml`，Linux 加载 `main.qml`

### 6. Animation Design

**紧凑 → 列表**:
- 高度：32 → 32 + listHeight + 32，280ms OutQuint
- 宽度：不变（全屏宽度）
- 内容：列表项逐行 fadeIn，200ms OutCubic

**列表 → 紧凑**:
- 高度：反向动画，280ms OutQuint
- 内容：列表 fadeOut 后高度收缩

**自动展开详情**:
- 高度从当前值平滑过渡到目标值
- 详情内容 fadeIn，200ms OutCubic

### 7. Hit-Test Region Updates

QML 在以下时机通过 C++ 调用更新 hit regions:
1. 窗口创建后（紧凑模式区域）
2. fusionMode 变化时（扩展/收缩区域）
3. NotchGeometry geometryChanged 时（重新计算坐标）

### 8. Data Flow Diagram

```
AgentModel.count ────────────→ NotchFusionWidget (左侧计数)
AgentModel.globalState ──────→ NotchFusionWidget (状态色 + 自动展开)
SubscriptionMonitor ─────────→ NotchFusionWidget (右侧圆环)
NotchGeometry.positions ─────→ NotchFusionWidget (布局定位)
NotchGeometry.positions ─────→ WindowOverlay (原生窗口定位)

用户点击 ────────────────────→ NotchFusionWidget (模式切换)
AgentModel 状态变化 ─────────→ NotchFusionWidget (自动展开/收回)
融合模式变化 ────────────────→ WindowOverlay::setHitTestRegions()
```

## PBT Properties

### P-1: Notch Detection Idempotency
**Invariant**: 重复调用 `NotchGeometry::compute()` 产生相同属性值
**Falsification**: 连续调用 compute() 两次，断言所有新旧属性相等

### P-2: Hit-Test Pass-Through
**Invariant**: 在透明区域（alpha=0）的点击不到达融合窗口
**Falsification**: 在刘海区域模拟点击，断言菜单栏图标可交互

### P-3: Mode Cycling Consistency
**Invariant**: 紧凑→列表→紧凑的循环总是回到初始状态
**Falsification**: 连续点击 N 次，断言 N%2==0 时为紧凑，N%2==1 时为列表

### P-4: Auto-Expand/Retract
**Invariant**: globalState 进入 permission/question/plan 时自动展开，离开时自动收回
**Falsification**: 模拟状态变化，断言 fusionMode 正确切换

### P-5: Position Stability
**Invariant**: 融合 Widget 位置仅由 NotchGeometry 驱动，拖动不改变
**Falsification**: 尝试鼠标拖动，断言位置不变

### P-6: IPC Visibility
**Invariant**: toggle 翻转可见性，show 设置可见，hide 设置不可见
**Falsification**: 发送 IPC 命令，断言 visible 属性匹配

### P-7: Multi-Screen Isolation
**Invariant**: 融合 Widget 只出现在 hasNotch=true 的屏幕
**Falsification**: 连接外接显示器，断言外接屏无融合 Widget

### P-8: Linux Preservation
**Invariant**: 非 macOS 编译时，所有融合代码不参与编译
**Falsification**: 检查预处理器输出，断言无 NotchFusionWidget 符号
