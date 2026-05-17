# Notch Fusion Widget — Specifications

## S-1: Single Fusion Window Architecture

**Constraint**: macOS 上使用单一全屏宽度透明 QML Window 替代现有的 3 窗口架构（NotchLeftHUD + NotchRightHUD + MainCard）。

**Details**:
- 窗口尺寸：`width = Screen.width`，`height` 根据模式动态变化（紧凑 32px，展开最大 380px）
- 窗口位置：Y=0（刘海频段），X=0（左边缘）
- 窗口层级：`NSScreenSaverWindowLevel`（1000），需 `constrainFrameRect` swizzle
- 窗口 flags：`Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint`
- 透明区域通过原生 `hitTest:` override 实现点击穿透

**Invariant**: macOS 上不存在独立的 NotchLeftHUD、NotchRightHUD 或 MainCard 窗口——融合 Widget 是唯一的顶层窗口。

**Falsification**: 启动后在 macOS 上枚举所有 QWindow，断言仅存在 1 个可见窗口。

## S-2: NotchGeometry Extension

**Constraint**: NotchGeometry 必须向 QML 暴露完整的刘海频段布局信息。

**新增属性**:
| 属性 | 类型 | 描述 |
|------|------|------|
| `screenWidth` | qreal | 屏幕宽度（Qt 逻辑坐标） |
| `safeTop` | qreal | safeAreaInsets.top 值 |
| `leftAreaWidth` | qreal | auxiliaryTopLeftArea.width |
| `rightAreaWidth` | qreal | auxiliaryTopRightArea.width |
| `notchLeftX` | qreal | 刘海左边缘 X（= screenX + leftAreaWidth） |
| `notchRightX` | qreal | 刘海右边缘 X（= screenX + screenWidth - rightAreaWidth） |

**Invariant**: 对同一屏幕配置，连续调用 `compute()` 产生完全相同的属性值。

**Falsification**: 调用 `compute()` 两次，断言所有属性值相等。

## S-3: Native Hit-Testing

**Constraint**: 融合窗口的透明区域点击必须穿透到菜单栏。

**Implementation**:
1. 创建 `PulseFusionView`（NSView 子类），override `hitTest:`
2. 在 `hitTest:` 中获取点击位置的像素 alpha 值
3. alpha > 0.01 → 返回 self（接受事件）
4. alpha ≤ 0.01 → 返回 nil（穿透到下层）

**Alternative approach**: 使用 `NSWindow.ignoresMouseEvents:NO` + QML 局部 MouseArea，但 QML Window 本身会拦截事件，需要原生 hitTest 才能保证穿透。

**Invariant**: 在透明区域（alpha=0）的鼠标事件不会到达融合窗口，而是传递给下层菜单栏。

**Falsification**: 在融合窗口透明区域模拟点击，断言菜单栏图标（如时钟）可正常触发。

## S-4: Mode Cycling

**Constraint**: 点击融合 Widget 切换两种模式，attention 状态自动展开。

**状态机**:
```
紧凑 ←(点击)→ Agent列表
  ↓ (有 permission/question/plan 时自动展开)
详情视图
  ↓ (用户响应后自动收回)
紧凑
```

**规则**:
- 紧凑模式：显示 agent 计数 + 状态色圆点（左）、用量圆环（右）
- Agent 列表模式：显示所有 agent 行，每行可点击 focusAgent()
- 详情模式：显示 PermissionView/QuestionView/PlanView
- 自动展开：`globalState` 变为 permission/question/plan 时自动进入详情模式
- 自动收回：用户响应后（decidePermission/answerQuestion/approvePlan），`globalState` 回到 idle/working 时收回

**Invariant**: 用户点击只影响紧凑/列表切换，不影响详情模式的自动展开/收回逻辑。

**Falsification**: 在紧凑模式下点击，断言进入列表模式；在列表模式下点击，断言回到紧凑模式。

## S-5: Dragging Disabled on macOS

**Constraint**: macOS 上融合 Widget 不可拖动，固定在刘海频段位置。

**Implementation**:
- 不传递 `dragWindow` 属性给融合 Widget
- 窗口位置由 NotchGeometry 驱动，不接受鼠标拖动
- `WindowOverlay::setup()` 对 `NotchFusionWidget` role 不设置默认位置（由 placeNotchHud 定位）

**Invariant**: 融合 Widget 的窗口位置仅由 NotchGeometry 计算，鼠标拖动不改变位置。

**Falsification**: 尝试拖动窗口，断言位置不变。

## S-6: Non-Notch Mac Fallback

**Constraint**: 无刘海 Mac 在菜单栏位置显示融合 Widget 条状。

**规则**:
- `safeAreaInsets.top == 0` → 无物理刘海
- 融合 Widget 仍显示，但 Y=0 定位在菜单栏上方
- `leftAreaWidth` 和 `rightAreaWidth` 为 0 时，左侧内容从 X=8 开始，右侧内容从 X=screenWidth-68 开始
- 刘海区域不透明（因为没有刘海缺口），整个条状显示

**Invariant**: 融合 Widget 在有无刘海的 Mac 上均可显示，仅在刘海位置的处理不同（有刘海→中间透明，无刘海→中间也显示内容或保持透明）。

**Falsification**: 在 `safeAreaInsets.top == 0` 的 Mac 上运行，断言融合 Widget 在菜单栏位置可见。

## S-7: Linux Compatibility

**Constraint**: Linux/Hyprland 平台行为完全不变。

**规则**:
- `Q_OS_MACOS` 宏保护所有融合 Widget 代码
- Linux 继续使用 layer-shell + 右上角 MainCard + 现有 drag 机制
- NotchLeftHUD/NotchRightHUD 代码保留（Linux 不使用）

**Invariant**: 编译无 `Q_OS_MACOS` 宏时，所有融合 Widget 代码不参与编译。

**Falsification**: 在 Linux 上编译运行，断言功能与修改前完全一致。

## S-8: IPC Compatibility

**Constraint**: `pulse-toggle` 的 show/hide/toggle 命令正确控制融合 Widget。

**规则**:
- `toggle`：切换融合 Widget 可见性
- `show`：显示融合 Widget
- `hide`：隐藏融合 Widget
- macOS 上只控制融合 Widget（不再有独立的 MainCard + HUD 窗口）

**Invariant**: IPC 命令后，融合 Widget 的 `visible` 属性与命令语义一致。

**Falsification**: 发送 toggle 命令，断言可见性翻转；发送 show 后断言可见；发送 hide 后断言不可见。

## S-9: Multi-Screen Behavior

**Constraint**: 融合 Widget 仅在内置显示器（有刘海的屏幕）上创建。

**规则**:
- 使用 `CGDisplayIsBuiltin()` 检测内置屏幕
- 外接显示器不创建融合 Widget
- 屏幕热插拔时刷新 NotchGeometry 并重新创建/移除窗口

**Invariant**: 融合 Widget 只出现在 `hasNotch == true` 的屏幕上。

**Falsification**: 连接外接显示器，断言融合 Widget 仅在内置屏可见。

## S-10: Visual Content Layout

**Constraint**: 内容严格位于辅助安全区域外侧。

**左区域**（auxiliaryTopLeftArea 内侧）:
- 状态色圆点（8×8，脉冲动画）
- Agent 计数数字
- 水平排列，左对齐

**右区域**（auxiliaryTopRightArea 内侧）:
- Claude 用量圆环（Canvas 16×16）
- 百分比数字
- 水平排列，右对齐

**中间区域**（刘海位置）:
- 紧凑模式：完全透明
- 展开模式：Agent 列表从左侧延伸到中间（避开刘海区域），刘海位置保持透明

**Invariant**: 左区域内容不超出 `leftAreaWidth`，右区域内容不超出 `rightAreaWidth`，刘海位置始终透明。

**Falsification**: 检查所有内容元素的 X 坐标，断言左区域 ≤ leftAreaWidth，右区域 ≥ screenWidth - rightAreaWidth。
