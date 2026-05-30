import QtQuick
import QtQuick.Window
import qml 1.0

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus

    // Geometry from NotchGeometry (first screen with notch)
    readonly property var geo: {
        if (!notchGeometry || !notchGeometry.screenPositions || notchGeometry.screenPositions.length === 0)
            return null
        return notchGeometry.screenPositions[0]
    }
    readonly property bool hasNotch: geo ? geo.hasNotch : false

    // Mode: "compact" | "list" | "detail"
    property string fusionMode: "compact"
    // Detail sub-mode: "permission" | "question" | "plan"
    property string detailMode: ""

    // Layout constants
    readonly property int compactH: 32
    readonly property int bandH: 32
    readonly property int leftW: 64
    readonly property int rightW: 60
    readonly property int hMargin: 4

    // Dynamic width: compact = left + gap + right, expanded = content width
    readonly property real compactW: {
        if (!geo) return leftW + rightW + hMargin * 2
        if (hasNotch) {
            // Span from left edge of left pill to right edge of right pill
            // leftX = screenX + leftAreaW - leftW - hMargin
            // rightX = screenX + screenWidth - rightAreaW + hMargin
            // rightEdge = rightX + rightW
            // totalW = rightEdge - leftX
            var lx = geo.leftAreaWidth - leftW - hMargin
            var rx = geo.screenWidth - geo.rightAreaWidth + hMargin + rightW
            return rx - lx
        }
        // No notch: side by side with gap
        return leftW + 12 + rightW
    }
    readonly property real expandedW: 380

    width: fusionMode === "compact" ? compactW : expandedW
    height: animatedH

    property real listContentH: 0
    readonly property real targetH: {
        if (fusionMode === "compact") return compactH
        if (fusionMode === "list")    return bandH + Math.min(listContentH, 280) + 32
        if (fusionMode === "detail")  return bandH + detailLoader.implicitHeight
        return compactH
    }
    property real animatedH: compactH

    Behavior on height { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }
    Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }
    onTargetHChanged: animatedH = targetH

    // Auto-expand for attention states
    Connections {
        target: agentModel
        function onGlobalStateChanged() {
            var s = agentModel.globalState
            if (s === "permission" || s === "question" || s === "plan") {
                root.detailMode = s
                root.fusionMode = "detail"
            } else if (root.fusionMode === "detail") {
                root.fusionMode = "compact"
                root.detailMode = ""
            }
        }
    }

    // Handle initial state (model may already be populated before QML loads)
    function initFromModelState() {
        var s = agentModel.globalState
        if (s === "permission" || s === "question" || s === "plan") {
            root.detailMode = s
            root.fusionMode = "detail"
        }
    }

    // Hit region update — whole window is content, no pass-through needed
    onFusionModeChanged: { updateHitRegions(); positionWindow() }
    onHeightChanged: updateHitRegions()
    onWidthChanged: { updateHitRegions(); positionWindow() }

    function updateHitRegions() {
        if (!overlayProxy) return
        // Whole window is interactive content
        overlayProxy.setHitTestRegions([{x: 0, y: 0, width: width, height: height}])
    }

    // State color from global state
    readonly property color stateColor: {
        if (agentModel.globalState === "idle")    return Theme.green
        if (agentModel.globalState === "working") return "#22d3ee"
        return Theme.coral
    }

    function usageColor(pct) {
        if (pct > 85) return Theme.coral
        if (pct > 60) return Theme.peach
        return "#8b88a8"
    }

    // Background pill shape
    Rectangle {
        id: band
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: bandH
        color: "#0a0a0c"
        radius: height / 2

        // Click to toggle compact/list
        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (root.fusionMode === "detail") return
                root.fusionMode = (root.fusionMode === "compact") ? "list" : "compact"
            }
        }

        // Left zone: status dot + agent count
        Item {
            id: leftZone
            anchors.left: parent.left
            anchors.leftMargin: hMargin
            anchors.verticalCenter: parent.verticalCenter
            width: leftW - hMargin * 2
            height: 28

            Row {
                anchors.centerIn: parent
                spacing: 6

                Rectangle {
                    width: 7; height: 7; radius: 3.5
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.stateColor

                    Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }

                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width + 4; height: parent.height + 4
                        radius: width / 2
                        color: "transparent"
                        border.color: root.stateColor
                        border.width: 1
                        opacity: agentModel.globalState !== "idle" ? 0.4 : 0
                        visible: opacity > 0

                        SequentialAnimation on scale {
                            running: agentModel.globalState !== "idle"
                            loops: Animation.Infinite
                            NumberAnimation { from: 0.8; to: 1.6; duration: 1800; easing.type: Easing.OutCubic }
                            NumberAnimation { from: 1.6; to: 0.8; duration: 0 }
                        }

                        Behavior on opacity { NumberAnimation { duration: 300 } }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: agentModel.count
                    font.family: "JetBrains Mono"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: "#e8e8f0"
                }
            }
        }

        // Separator dot (only in compact mode when notch is present)
        Rectangle {
            visible: root.fusionMode === "compact" && root.hasNotch
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: 3; height: 3; radius: 1.5
            color: Qt.rgba(1, 1, 1, 0.15)
        }

        // Right zone: usage arc gauge
        Item {
            id: rightZone
            anchors.right: parent.right
            anchors.rightMargin: hMargin
            anchors.verticalCenter: parent.verticalCenter
            width: rightW - hMargin * 2
            height: 28

            Row {
                anchors.centerIn: parent
                spacing: 5
                visible: subscriptionMonitor.claudeAvailable

                Canvas {
                    id: gauge
                    width: 16; height: 16
                    anchors.verticalCenter: parent.verticalCenter

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()

                        var cx = width / 2
                        var cy = height / 2
                        var r = Math.min(cx, cy) - 1.5
                        var pct = subscriptionMonitor.claudeUtilization / 100

                        ctx.beginPath()
                        ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                        ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.10)
                        ctx.lineWidth = 2
                        ctx.stroke()

                        if (pct > 0.001) {
                            var startAngle = -Math.PI / 2
                            var endAngle   = startAngle + pct * 2 * Math.PI
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, startAngle, endAngle)
                            ctx.strokeStyle = root.usageColor(subscriptionMonitor.claudeUtilization)
                            ctx.lineWidth = 2
                            ctx.lineCap = "round"
                            ctx.stroke()
                        }
                    }

                    Connections {
                        target: subscriptionMonitor
                        function onDataChanged() { gauge.requestPaint() }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: Math.round(subscriptionMonitor.claudeUtilization) + "%"
                    font.family: "JetBrains Mono"
                    font.pixelSize: 10
                    font.weight: Font.Medium
                    color: root.usageColor(subscriptionMonitor.claudeUtilization)
                }
            }
        }
    }

    // Expanded content below band
    Loader {
        id: expandedLoader
        anchors.top: band.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        active: fusionMode === "list"
        sourceComponent: listComp

        onItemChanged: {
            if (item) {
                item.implicitHeightChanged.connect(function() {
                    root.listContentH = item.implicitHeight
                })
                root.listContentH = item.implicitHeight
            }
        }

        Component {
            id: listComp
            Item {
                id: listContent
                implicitHeight: Math.min(agentList.implicitHeight, 280) + 32

                // Rounded bottom corners
                Rectangle {
                    anchors.fill: parent
                    radius: 16
                    color: "#0a0a0c"
                }

                Column {
                    id: agentList
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    topPadding: 4
                    Repeater {
                        model: agentModel
                        delegate: Item {
                            width: agentList.width
                            height: 52

                            readonly property color agentColor: Theme.agentAccent(model.name)

                            Rectangle {
                                anchors.fill: parent
                                color: model.sessionBusy
                                    ? Qt.rgba(agentColor.r, agentColor.g, agentColor.b, 0.06)
                                    : (ma.containsMouse ? Qt.rgba(1,1,1,0.03) : "transparent")

                                Rectangle {
                                    visible: model.sessionBusy
                                    width: 3; height: 28; radius: 2
                                    anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                                    color: agentColor; opacity: 0.9
                                }

                                Row {
                                    anchors.left: parent.left; anchors.leftMargin: 16
                                    anchors.right: parent.right; anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 8

                                    Rectangle {
                                        width: 7; height: 7; radius: 3.5
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: model.sessionBusy ? Theme.running : Theme.idle
                                        SequentialAnimation on opacity {
                                            running: model.sessionBusy
                                            loops: Animation.Infinite
                                            NumberAnimation { to: 0.35; duration: 1000 }
                                            NumberAnimation { to: 1.0; duration: 1000 }
                                        }
                                    }

                                    Text {
                                        text: model.name
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        color: Qt.rgba(1, 1, 1, model.sessionBusy ? 0.92 : 0.55)
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        visible: (model.currentStep ?? "") !== ""
                                        text: model.currentStep ?? ""
                                        font.pixelSize: 11
                                        color: Qt.rgba(1, 1, 1, 0.58)
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    id: ma
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: model.canJump ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    onClicked: if (model.canJump) agentModel.focusAgent(index)
                                }
                            }
                        }
                    }
                }

                Item {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left; anchors.right: parent.right
                    height: 32

                    Rectangle {
                        anchors.top: parent.top
                        width: parent.width; height: 1
                        color: Theme.border
                    }

                    Text {
                        anchors.left: parent.left; anchors.leftMargin: Theme.s4
                        anchors.verticalCenter: parent.verticalCenter
                        text: agentModel.count + " sessions"
                        font.family: "JetBrains Mono"
                        font.pixelSize: 10
                        color: Theme.text3
                    }
                }
            }
        }
    }

    // Detail view (permission/question/plan)
    Loader {
        id: detailLoader
        anchors.top: band.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        active: fusionMode === "detail"
        sourceComponent: {
            if (detailMode === "permission") return permComp
            if (detailMode === "question")  return questComp
            if (detailMode === "plan")      return planComp
            return null
        }
    }

    Component { id: permComp;  PermissionView { agentRow: agentModel.activeAgentRow } }
    Component { id: questComp; QuestionView { agentRow: agentModel.activeAgentRow } }
    Component { id: planComp;  PlanView { agentRow: agentModel.activeAgentRow } }

    // Position: centered on notch area in compact, centered on screen in expanded
    onGeoChanged: positionWindow()
    onCompactWChanged: positionWindow()

    function positionWindow() {
        if (!overlayProxy) return
        if (!geo) {
            root.x = 0
            root.y = 0
            return
        }
        var w = root.width
        var x, y
        if (root.fusionMode === "compact" && hasNotch) {
            // x = left edge of left pill content
            x = geo.leftAreaWidth - leftW - hMargin
            y = geo.y || 0
        } else if (root.fusionMode === "compact") {
            // No notch: position near left of menu bar area
            x = 8
            y = geo.y || 0
        } else {
            // Expanded: center on screen
            x = (geo.screenWidth - w) / 2
            y = geo.y || 0
        }
        root.x = x
        root.y = y
        overlayProxy.placeFusionWindow(x, y, w, targetH)
        updateHitRegions()
    }

    Component.onCompleted: {
        initFromModelState()
        positionWindow()
        Qt.callLater(positionWindow)
    }
}
