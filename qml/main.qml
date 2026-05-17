import QtQuick
import qml 1.0
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: root
    visible: true
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property string backendState: agentModel.globalState
    property bool userExpanded: (typeof mockForceExpanded !== "undefined" && mockForceExpanded)
    property string pulseState: userExpanded ? "expanded" : (agentModel.idleCollapsed ? "idle" : backendState)

    readonly property var widthByState: {
        "idle": 280,
        "working": 380,
        "permission": 420,
        "question": 420,
        "plan": 460
    }

    // When expanded, inherit the underlying backend state's width so clicking the
    // header doesn't change the window width.
    width: widthByState[pulseState === "expanded"
        ? (agentModel.idleCollapsed ? "idle" : backendState)
        : pulseState] ?? 380

    // Height management:
    //   _stableBodyH — last real implicitHeight from the loaded item (> 60 → valid).
    //                  Reset to 0 each time body activates so the fresh item drives
    //                  the animation, not the previous item's stale value.
    //   _holdH       — animatedH snapshot taken when body activates.
    //                  While _stableBodyH == 0 (item not yet laid out), targetH returns
    //                  _holdH so no spurious animation fires before the item is ready.
    //
    // Why this matters: QML Column/positioner implicitHeight is lazy (polished on first
    // render cycle). ExpandedView reports implicitH=32 (footer-only) at onLoaded time,
    // then fires implicitHeightChanged with the real value (~280) one frame later.
    // Without this guard the window would stutter 52→76→324 on each expand.
    property real _stableBodyH: 0
    property real _holdH:       52   // set in onActiveChanged(active=true)

    property real targetH: {
        if (!body.active)         return header.height + 8
        if (_stableBodyH <= 0)    return _holdH          // hold until layout settles
        return header.height + _stableBodyH + (pulseState === "expanded" ? 0 : 8)
    }
    property real animatedH: 52
    height: animatedH

    Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }

    // Animate animatedH (a plain QML real), not Window.height directly.
    // Expand: smooth 280ms; Collapse: instant (avoids blank-card flash).
    NumberAnimation {
        id: heightAnim
        target: root
        property: "animatedH"
        duration: 280
        easing.type: Easing.OutQuint
    }

    onTargetHChanged: {
        if (body.active) {
            heightAnim.stop()
            heightAnim.from = root.animatedH
            heightAnim.to = targetH
            heightAnim.start()
        } else {
            heightAnim.stop()
            root.animatedH = targetH
        }
    }

    // Hyprland's resizewindowpixel IPC can arrive after a QML collapse, overriding
    // Window.height and breaking the "height: animatedH" binding.  When we detect
    // a mismatch we restore the binding via Qt.binding() so animatedH stays in
    // control.  The C++ resizeTimer will then read the corrected height and send
    // the right resizewindowpixel call to Hyprland within 100 ms.
    onHeightChanged: {
        if (Math.abs(height - animatedH) > 2)
            root.height = Qt.binding(function() { return root.animatedH; })
    }

    Rectangle {
        id: card
        anchors.fill: parent
        color: Qt.rgba(0.078, 0.075, 0.11, 0.93)
        border.color: "#3a384e"
        border.width: 1
        radius: Theme.pulseRadius

        Behavior on radius { NumberAnimation { duration: 280 } }

        // Top sheen
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 1 }
            height: 1
            radius: parent.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.08) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        HeaderBar {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            dragWindow: root
            pulseState: root.pulseState
            agentName: agentModel.activeAgentRow >= 0 ? (agentModel.get(agentModel.activeAgentRow).name ?? "") : ""
            agentSub: agentModel.activeAgentRow >= 0 ? (agentModel.get(agentModel.activeAgentRow).currentStep ?? "") : ""
            agentTool: agentModel.activeAgentRow >= 0 ? (agentModel.get(agentModel.activeAgentRow).permissionTool ?? "") : ""
            agentCount: agentModel.count
            liveTime: clockTimer.timeStr
            claudeUtilization: subscriptionMonitor.claudeUtilization
            codexUtilization:  subscriptionMonitor.codexUtilization
            claudeAvailable:   subscriptionMonitor.claudeAvailable
            codexAvailable:    subscriptionMonitor.codexAvailable
            onClicked: root.userExpanded = !root.userExpanded
        }

        // Clock tick — updates liveTime every 30 seconds
        Timer {
            id: clockTimer
            interval: 30000
            repeat: true
            running: true
            property string timeStr: Qt.formatTime(new Date(), "HH:mm")
            onTriggered: timeStr = Qt.formatTime(new Date(), "HH:mm")
        }

        Loader {
            id: body
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            // expanded: fill card so footer sticks to bottom during manual resize
            height: pulseState === "expanded"
                ? parent.height - header.height
                : (active && item ? item.implicitHeight : 0)
            active: pulseState !== "idle" && pulseState !== "working"
            sourceComponent: {
                if (pulseState === "permission") return permComp
                if (pulseState === "question") return questComp
                if (pulseState === "plan") return planComp
                if (pulseState === "expanded") return expandedComp
                return null
            }

            onActiveChanged: {
                if (!active) {
                    // Collapse: instant snap, reset tracking
                    heightAnim.stop()
                    root.animatedH = header.height + 8
                    root._stableBodyH = 0
                } else {
                    // Capture current animatedH as hold value before item loads.
                    root._holdH = root.animatedH
                    root._stableBodyH = 0
                }
            }

            onLoaded: {
                if (item.implicitHeight > 60)
                    root._stableBodyH = item.implicitHeight

                item.implicitHeightChanged.connect(function() {
                    if (body.item && body.active && body.item.implicitHeight > 0)
                        root._stableBodyH = body.item.implicitHeight
                })

                item.opacity = 0
                item.y = 0
                bodyOpacAnim.target = item
                bodyAnim.start()
            }
        }

        ParallelAnimation {
            id: bodyAnim
            NumberAnimation { id: bodyOpacAnim; property: "opacity"; to: 1; duration: 200; easing.type: Easing.OutCubic }
        }
    }

    Component { id: permComp; PermissionView { agentRow: agentModel.activeAgentRow } }
    Component { id: questComp; QuestionView { agentRow: agentModel.activeAgentRow } }
    Component { id: planComp; PlanView { agentRow: agentModel.activeAgentRow } }
    Component { id: expandedComp; ExpandedView { width: root.width } }

    Connections {
        target: appSettings
        function onThemeChanged() { Theme.name = appSettings.theme }
        function onShapeChanged() { Theme.shape = appSettings.shape }
    }

    Component.onCompleted: {
        Theme.name = appSettings.theme
        Theme.shape = appSettings.shape
        root.animatedH = targetH
        root._holdH = root.animatedH
    }
}
