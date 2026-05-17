# Notch Fusion Widget — Tasks

## Phase 1: Backend Extensions

- [x] 1.1 Extend `ScreenHudPos` in `src/NotchGeometry.h` — Add fields: screenWidth, safeTop, leftAreaWidth, rightAreaWidth, notchLeftX, notchRightX
- [x] 1.2 Populate new fields in `src/NotchGeometry.mm` `compute()` — Compute from NSScreen properties, add to QVariantMap in screenPositions()
- [x] 1.3 Unify AppKit coordinate baseline in `NotchGeometry.mm` — Replace `[NSScreen mainScreen]` with `[[NSScreen screens] firstObject]` for primaryTop
- [x] 1.4 Add `NotchFusionWidget` to `OverlayOptions::Role` in `src/WindowOverlay.h`
- [x] 1.5 Add `setHitTestRegions(QWindow*, QVector<QRectF>)` virtual method to `src/WindowOverlay.h`
- [x] 1.6 Implement `NotchFusionWidget` setup in `src/WindowOverlay_macos.mm` — NSScreenSaverWindowLevel, constrainFrameRect swizzle, no ignoresMouseEvents, opaque=NO
- [x] 1.7 Implement native `hitTest:` override in `src/WindowOverlay_macos.mm` — Swizzle content NSView, check hit regions, return nil for transparent areas
- [x] 1.8 Implement `setHitTestRegions()` in `src/WindowOverlay_macos.mm` — Store regions as associated object on NSView, called from QML on mode change
- [x] 1.9 Add stub `setHitTestRegions()` in `src/WindowOverlay_linux.cpp` and `src/WindowOverlay_win.cpp` — Covered by base class defaults
- [x] 1.10 Add `placeFusionWindow(QWindow*, qreal x, qreal y, qreal width)` to `src/WindowOverlay.h` — Sets position + width + level in one call

## Phase 2: QML Implementation

- [x] 2.1 Create `qml/NotchFusionWidget.qml` — Full-width transparent Window with mode state machine (compact/list/detail)
- [x] 2.2 Implement compact mode layout — Left zone (status dot + agent count), Right zone (usage arc gauge), center transparent
- [x] 2.3 Implement list mode — Agent list below top band, reuse ExpandedView delegate pattern, Flickable with max 280px
- [x] 2.4 Implement detail mode — PermissionView/QuestionView/PlanView below top band
- [x] 2.5 Implement mode cycling — Click toggles compact↔list; auto-expand for permission/question/plan; auto-retract on resolve
- [x] 2.6 Implement height animation — 280ms OutQuint, compact=32, list=32+content+32, detail=32+content
- [x] 2.7 Disable drag on macOS — No dragWindow property, MouseArea only handles click
- [x] 2.8 Implement hit region update calls — On mode change, call overlayProxy.setHitTestRegions() via context property
- [x] 2.9 Handle non-notch Mac fallback — When hasNotch=false, position at menu bar strip, no center gap
- [x] 2.10 Implement geometry binding — Bind x/y/width from notchGeometry.screenPositions[0]

## Phase 3: Integration

- [x] 3.1 Modify `src/main.cpp` — On macOS, create NotchFusionWidget instead of main.qml + HUD pair; expose OverlayProxy as context property
- [x] 3.2 Modify `src/main.cpp` — Update IPC toggle/show/hide to control fusion window only on macOS
- [x] 3.3 Modify `src/main.cpp` — Connect NotchGeometry::geometryChanged to reposition and update QML geometry
- [x] 3.4 Add screen lifecycle handling — screenAdded refresh with debounce
- [x] 3.5 Add `qml/NotchFusionWidget.qml` to `qml/qmldir`
- [x] 3.6 Add `qml/NotchFusionWidget.qml` to `CMakeLists.txt` qt_add_resources
- [x] 3.7 OverlayProxy class in `src/WindowOverlay.h` — No separate .mm file needed, QML type registration via context property

## Phase 4: Cleanup & Compatibility

- [ ] 4.1 Guard all fusion code with `#ifdef Q_OS_MACOS` — Ensure Linux/Windows compile without fusion symbols
- [ ] 4.2 Keep NotchLeftHUD.qml and NotchRightHUD.qml in build — Still used by non-fusion path (future fallback)
- [ ] 4.3 Update `demo_notch/` if needed — Verify demo still works with new NotchGeometry fields
- [ ] 4.4 Remove debug log files (`/tmp/notch_*.log`) from production code — Replace with qCDebug or remove

## Phase 5: Testing

- [ ] 5.1 Test on MacBook Pro 14"/16" with notch — Verify fusion widget appears at Y=0, content in left/right auxiliary areas
- [ ] 5.2 Test click-through — Verify menu bar icons behind transparent areas are clickable
- [ ] 5.3 Test mode cycling — Click toggles compact↔list, auto-expand/collapse for permission/question/plan
- [ ] 5.4 Test non-notch Mac — Verify menu-bar strip mode works correctly
- [ ] 5.5 Test multi-screen — Verify fusion widget only on built-in display
- [ ] 5.6 Test IPC — Verify toggle/show/hide commands work correctly
- [ ] 5.7 Test fullscreen — Verify widget stays visible in fullscreen apps
- [ ] 5.8 Test Linux build — Verify no regression, MainCard works as before
- [ ] 5.9 Test screen hotplug — Verify correct behavior when external display connected/disconnected
- [ ] 5.10 Test 0 agents — Verify compact mode shows empty/hidden state correctly
