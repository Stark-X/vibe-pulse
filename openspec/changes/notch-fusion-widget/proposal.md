# Notch Fusion Widget — Proposal

## Summary

macOS 运行时，将当前 3 窗口架构（NotchLeftHUD + NotchRightHUD + MainCard）合并为单一全屏宽度融合 Widget，以 Dynamic Island 风格横跨屏幕顶部刘海区域。Widget 在刘海左右两侧的安全区域外侧显示内容，中间刘海位置保持透明。macOS 上禁止拖动，固定在屏幕顶部。点击切换紧凑/Agent 列表两种模式；有 permission/question/plan 时自动展开详情。

## Motivation

当前 macOS 架构使用 3 个独立窗口，视觉上不连贯——左侧 agent 计数胶囊、右侧用量圆环、右上角主卡片三者之间没有视觉关联。融合设计让 Widget 与刘海区域一体化，类似 iPhone Dynamic Island 的体验，减少视觉碎片感，同时保留核心状态信息的可见性。

## Technical Constraints

### Hard Constraints

1. **刘海物理区域不可覆盖**：macOS 系统控制刘海/摄像头区域，任何窗口都无法在其上方绘制。融合 Widget 必须在刘海两侧绘制，中间保持透明。

2. **NSScreenSaverWindowLevel 必须**：要在 Y=0（刘海频段）定位窗口，必须使用 `NSScreenSaverWindowLevel`（1000）+ `constrainFrameRect` swizzle。否则 macOS 会将窗口推到菜单栏下方。

3. **全宽透明窗口的点击穿透**：全屏宽度的窗口如果接受鼠标事件，会遮挡菜单栏/状态栏图标的交互。必须在原生层实现 hit-testing：仅在 Widget 实际内容区域响应点击，透明区域点击穿透到下层。

4. **Qt6 QML 不支持窗口透明遮罩**：QML Window 无法直接"镂空"中间区域。需通过原生 CALayer mask 或在 QML 中用透明 Item 留白。

5. **QML flags 重置 NSWindow level**：QML `Window.flags` 绑定在 `completeCreate` 后会重置 NSWindow level。必须在 show 后重新强制设置 level（现有 placeNotchHud 已有此逻辑）。

6. **非刘海 Mac 回退**：iMac/Mac mini/外接显示器无刘海，`safeAreaInsets.top == 0`。融合 Widget 应在菜单栏位置以窄条形态显示，或保持当前右上角窗口模式。

7. **Linux 行为不变**：仅 macOS 启用融合模式。Linux/Hyprland 保持现有 layer-shell + 右上角卡片架构。

8. **IPC 兼容**：`pulse-toggle` 的 show/hide/toggle 命令必须同时控制融合 Widget 的可见性。

### Soft Constraints

1. **参考设计**：Apple Dynamic Island（横跨顶部、中间摄像头缺口、左右对称内容区域）、boring.notch、vibe-notch。

2. **刘海几何**：MacBook Pro 14" 刘海约 224×38pt，Air 13" 约 180×38pt。`auxiliaryTopLeftArea` 约 80-100pt，`auxiliaryTopRightArea` 约 80-100pt。

3. **视觉风格**：暗色半透明背景（复用 `Theme.bg0`/`bg2`），圆角，状态色指示器，与现有 Pulse 卡片风格一致。

4. **动画**：模式切换时高度平滑过渡（复用现有 280ms OutQuint），紧凑→展开时从刘海频段向下展开。

5. **当前 NotchLeftHUD/NotchRightHUD 代码可复用**：状态色逻辑、Canvas 圆环、脉冲动画等可直接迁移到融合 Widget 内。

### Dependencies

1. **现有模块**：
   - `AgentModel` — count, globalState, idleCollapsed, activeAgentRow
   - `SubscriptionMonitor` — claudeUtilization, codexUtilization, claudeAvailable
   - `NotchGeometry` — 刘海检测和位置计算
   - `WindowOverlay_macos.mm` — NSWindow level 和安全区域绕过

2. **需扩展的模块**：
   - `NotchGeometry` — 需新增 `screenWidth`、`safeTop`、`leftAreaWidth`、`rightAreaWidth`、`notchLeftX`、`notchRightX` 属性，供 QML 布局使用
   - `WindowOverlay` — 需新增 `NotchFusionWidget` role + 原生 hit-testing 逻辑
   - `OverlayOptions` — 需新增 `NotchFusionWidget` 枚举值

### Risks

1. **全宽窗口遮挡菜单栏**：最关键风险。必须在原生层实现 `hitTest:` override，使透明区域的鼠标事件穿透到菜单栏。
   - 缓解：在 NSWindow 子类中 override `hitTest:`，仅在有内容（非透明像素）的区域返回 NSView，其他区域返回 nil。

2. **多显示器坐标转换**：NotchGeometry 中 `primaryTop` 计算与 `placeNotchHud` 使用不同的 primary screen 获取方式，可能导致 Y 坐标错误。
   - 缓解：统一使用 `[[NSScreen screens] firstObject]`。

3. **屏幕热插拔**：外接显示器连接/断开时可能残留过期窗口。
   - 缓解：在 screenRemoved 事件中清理对应融合 Widget。

4. **QML performance**：全屏宽度窗口即使大部分透明，仍需完整渲染。在 Retina 显示器上宽度可达 3024px。
   - 缓解：窗口高度保持最小（32px 紧凑态），QML 内容仅绘制在左右两侧小区域。

## Design

### 整体架构

```
┌──────────────────────────────────────────────────────────────────────┐
│ ┌──────────┐  ┌──────────────────────┐  ┌──────────┐              │
│ │ Left Zone │  │    Notch (camera)    │  │ Right Zone │              │
│ │ ● 3      │  │    transparent       │  │ ○ 85%    │              │
│ └──────────┘  └──────────────────────┘  └──────────┘              │
│  ← leftAreaW →  ←  notchWidth  →       ← rightAreaW →            │
│                                                                      │
│  点击 → 展开 Agent List:                                             │
│ ┌──────────┐  ┌──────────────────────┐  ┌──────────┐              │
│ │ ● 3      │  │    transparent       │  │ ○ 85%    │              │
│ ├──────────┤  ├──────────────────────┤  ├──────────┤              │
│ │ Agent 1  │  │                      │  │          │              │
│ │ Agent 2  │  │                      │  │          │              │
│ │ Agent 3  │  │                      │  │          │              │
│ └──────────┘  └──────────────────────┘  └──────────┘              │
└──────────────────────────────────────────────────────────────────────┘
```

### 模式设计

**紧凑模式（默认）**：
- 高度：32px（刘海频段高度）
- 左侧区域：状态色圆点 + agent 数量 + 脉冲动画
- 右侧区域：Claude 用量圆环 + 百分比
- 中间刘海位置：完全透明，系统摄像头指示灯可见

**Agent 列表模式（点击切换）**：
- 高度：32px（顶栏）+ agent 列表高度（每个 62px，最大 280px）+ 32px（底栏）
- 顶栏：复用紧凑模式内容
- 列表区域：仅在左侧和中间区域显示（刘海右侧继续显示用量）
- 点击某 agent 行 → focusAgent()

**自动展开模式（permission/question/plan）**：
- 当有 agent 进入 permission/question/plan 状态时，自动展开详情视图
- 详情视图：在左侧和中间区域显示 PermissionView/QuestionView/PlanView
- 用户响应后自动收回

### 模式循环

```
紧凑 ←→ Agent 列表     （用户点击切换）
          ↓                （有 permission/question/plan 时自动展开）
       自动详情视图        （用户响应后自动收回 → 紧凑）
```

### 窗口方案：单窗口 + 原生 hit-testing

**单窗口**：一个全屏宽度透明 `Window`，位于 Y=0（刘海频段）。

**原生 hit-testing**：override NSWindow 的 `hitTest:` 或使用 `NSWindow` subclass：
- 在 Widget 内容区域（左侧 agent 计数、右侧用量圆环）返回 NSView → 接受鼠标事件
- 在透明区域（刘海位置、无内容区域）返回 nil → 点击穿透到下层菜单栏

**不采用多窗口方案**的原因：3 窗口方案视觉不连贯，且事件协调复杂。

### 数据流

```
AgentModel.count ──────────→ NotchFusionWidget (左侧数字)
AgentModel.globalState ────→ NotchFusionWidget (左侧状态色)
SubscriptionMonitor ───────→ NotchFusionWidget (右侧圆环)
NotchGeometry ─────────────→ NotchFusionWidget (布局定位)
NotchGeometry ─────────────→ WindowOverlay (原生窗口定位)

用户点击 ──────────────────→ NotchFusionWidget (模式切换)
AgentModel.globalState ────→ NotchFusionWidget (自动展开 permission/question/plan)
```

### 文件变更清单

| 文件 | 变更类型 | 描述 |
|------|---------|------|
| `qml/NotchFusionWidget.qml` | 新增 | 融合 Widget 主界面 |
| `src/NotchGeometry.h` | 修改 | 新增 screenWidth/safeTop/leftAreaWidth/rightAreaWidth/notchLeftX/notchRightX 属性 |
| `src/NotchGeometry.mm` | 修改 | 计算并暴露新增属性到 QML |
| `src/WindowOverlay.h` | 修改 | 新增 NotchFusionWidget role |
| `src/WindowOverlay_macos.mm` | 修改 | 新增 fusion widget 的 NSWindow setup + hitTest override |
| `src/main.cpp` | 修改 | macOS 上创建融合 Widget 替代 3 窗口，修改 IPC 逻辑 |
| `CMakeLists.txt` | 修改 | 添加 NotchFusionWidget.qml 到资源 |
| `qml/qmldir` | 修改 | 注册 NotchFusionWidget |
| `qml/main.qml` | 修改 | macOS 上隐藏 MainCard（由融合 Widget 替代） |
| `qml/HeaderBar.qml` | 修改 | macOS 上禁用拖动（dragWindow: null） |

### NotchGeometry 扩展

需要在 `ScreenHudPos` 和 QML 暴露属性中新增：

```cpp
struct ScreenHudPos {
    // ... existing fields ...
    qreal screenWidth = 0;      // 屏幕宽度（Qt 逻辑坐标）
    qreal safeTop = 0;          // safeAreaInsets.top
    qreal leftAreaWidth = 0;    // auxiliaryTopLeftArea.width
    qreal rightAreaWidth = 0;   // auxiliaryTopRightArea.width
    qreal notchLeftX = 0;       // 刘海左边缘 X（左安全区右边界）
    qreal notchRightX = 0;      // 刘海右边缘 X（右安全区左边界）
};
```

### 原生 Hit-Testing 方案

在 `WindowOverlay_macos.mm` 中创建自定义 NSView：

```objc
// PulseHitTestView: only accepts hits where alpha > threshold
@interface PulseHitTestView : NSView
@property (nonatomic, assign) CGFloat alphaThreshold;  // default 0.01
@end

@implementation PulseHitTestView
- (NSView *)hitTest:(NSPoint)point {
    NSView *hit = [super hitTest:point];
    if (!hit) return nil;
    // Check pixel alpha at click point
    // If transparent → return nil (pass-through)
    // If opaque → return self (accept click)
    return hit;
}
@end
```

或者更简单的方案：使用 `NSWindow.ignoresMouseEvents = NO` + QML MouseArea 仅覆盖内容区域。透明区域自然不接收事件... 但全宽窗口的背景矩形仍会拦截事件。

**最终方案**：在融合 Widget 的 QML 中，不使用全宽背景矩形。仅在左右内容区域放置 Rectangle，中间保持完全透明（无 Item）。配合 NSWindow 的 `ignoresMouseEvents = NO`，透明区域的事件将自动穿透（因为 QML 没有 Item 接收它们）。

**但 QML Window 本身会拦截事件**。需要原生 hitTest 来确保透明区域的点击穿透。

## Success Criteria

1. MacBook Pro 14"/16"（有刘海）：启动后融合 Widget 自动出现在屏幕顶部刘海频段
2. 无刘海 Mac：融合 Widget 在菜单栏区域以窄条形态显示
3. 紧凑模式正确显示左侧 agent 计数/状态色 + 右侧用量圆环
4. 点击切换紧凑/Agent 列表模式
5. 有 permission/question/plan 时自动展开详情，响应后自动收回
6. macOS 上不可拖动
7. 透明区域点击穿透到菜单栏（系统状态图标可正常交互）
8. 全屏模式下 Widget 仍可见
9. 多显示器只在内置屏显示融合 Widget
10. pulse-toggle show/hide/toggle 正常工作
11. Linux 行为完全不变

## Out of Scope

- 刘海展开/收缩动画（类似 iPhone 的 morphing 动画）
- hover 交互展开详情
- Codex 用量圆环（后续迭代）
- 融合 Widget 与右侧菜单栏图标的精细避让
