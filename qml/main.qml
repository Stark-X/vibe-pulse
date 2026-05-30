import QtQuick
import qml 1.0
import QtQuick.Window

Window {
    id: root
    visible: true
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    readonly property int dotD:  52
    readonly property int listW: 320

    property bool isExpanded:  typeof mockForceExpanded !== "undefined" && mockForceExpanded
    property bool isDragging:  false

    // Only show usage for tool types that have at least one running agent
    property bool hasClaudeAgents: false
    property bool hasCodexAgents:  false

    function refreshAgentTypes() {
        var cc = false, cd = false
        for (var i = 0; i < agentModel.count; i++) {
            var t = agentModel.get(i).toolType
            if (t === "Claude Code") cc = true
            else if (t === "Codex")  cd = true
        }
        hasClaudeAgents = cc
        hasCodexAgents  = cd
    }

    Connections {
        target: agentModel
        function onCountChanged()       { root.refreshAgentTypes() }
        function onGlobalStateChanged() { root.refreshAgentTypes() }
    }

    // Window is always full listW wide so right edge stays fixed on screen.
    // The card animates its own width from dotD to listW (right-anchored),
    // so the dot sits in the top-right corner and expands leftward.
    width: listW

    property real animH: dotD
    property real animCardW: isExpanded ? listW : dotD
    height: Math.round(animH)

    Behavior on animH     { NumberAnimation { duration: 300; easing.type: Easing.OutQuint } }
    Behavior on animCardW { NumberAnimation { duration: 300; easing.type: Easing.OutQuint } }

    readonly property color stateColor: {
        var s = agentModel.globalState
        if (s === "idle")    return "#10b981"
        if (s === "working") return "#22d3ee"
        return "#f87171"
    }

    // Morphing card — right-anchored, expands leftward
    Rectangle {
        id: card
        anchors { top: parent.top; right: parent.right }
        width:  Math.round(root.animCardW)
        height: parent.height
        color: Qt.rgba(0.078, 0.075, 0.11, 0.93)
        border.color: "#3a384e"
        border.width: 1
        radius: isExpanded ? Theme.pulseRadius : root.dotD / 2
        clip: true

        Behavior on radius { NumberAnimation { duration: 300; easing.type: Easing.OutQuint } }

        // Top sheen (expanded only)
        Rectangle {
            visible: isExpanded
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 1 }
            height: 1
            radius: card.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.08) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        // ── Dot content ────────────────────────────────────────
        Item {
            anchors.fill: parent
            opacity: isExpanded ? 0 : 1
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Rectangle {
                anchors.centerIn: parent
                width: 22; height: 22; radius: 11
                color: "transparent"
                border.color: root.stateColor
                border.width: 1.5
                opacity: 0

                Behavior on border.color { ColorAnimation { duration: 300 } }

                SequentialAnimation on scale {
                    loops: Animation.Infinite
                    NumberAnimation { from: 1.0; to: 2.4; duration: 2200; easing.type: Easing.OutCubic }
                }
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.6; to: 0; duration: 2200; easing.type: Easing.OutCubic }
                }
            }

            Text {
                anchors.centerIn: parent
                text: agentModel.workingCount + "/" + agentModel.count
                font.family: "JetBrains Mono"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: root.stateColor
                Behavior on color { ColorAnimation { duration: 300 } }
            }
        }

        // ── List content ────────────────────────────────────────
        Item {
            anchors.fill: parent
            opacity: isExpanded ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 200 } }

            // Title bar
            Item {
                id: listHeader
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 36

                Text {
                    anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                    text: "Pulse"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: "#ebe7f7"
                }

                Row {
                    anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    spacing: 6

                    // Claude Code usage
                    Row {
                        visible: root.hasClaudeAgents && subscriptionMonitor.claudeUtilization > 0
                        spacing: 3
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            source: "qrc:/assets/claude-code.png"
                            width: 12; height: 12
                            fillMode: Image.PreserveAspectFit
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: 0.75
                        }
                        Text {
                            text: Math.round(subscriptionMonitor.claudeUtilization) + "%"
                            font.family: "JetBrains Mono"
                            font.pixelSize: 10
                            color: subscriptionMonitor.claudeUtilization > 85 ? "#f87171"
                                 : subscriptionMonitor.claudeUtilization > 60 ? "#fbc19d"
                                 : "#a09cc0"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    // Codex usage
                    Row {
                        visible: root.hasCodexAgents && subscriptionMonitor.codexUtilization > 0
                        spacing: 3
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            source: "qrc:/assets/codex.png"
                            width: 12; height: 12
                            fillMode: Image.PreserveAspectFit
                            anchors.verticalCenter: parent.verticalCenter
                            opacity: 0.75
                        }
                        Text {
                            text: Math.round(subscriptionMonitor.codexUtilization) + "%"
                            font.family: "JetBrains Mono"
                            font.pixelSize: 10
                            color: subscriptionMonitor.codexUtilization > 85 ? "#f87171"
                                 : subscriptionMonitor.codexUtilization > 60 ? "#fbc19d"
                                 : "#a09cc0"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                Rectangle {
                    anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                    height: 1
                    color: "#3a384e"
                }

                // Drag handle for expanded mode
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: false
                    cursorShape: Qt.SizeAllCursor
                    onPressed: {
                        root.isDragging = true
                        collapseTimer.stop()
                        root.startSystemMove()
                    }
                    onReleased: root.isDragging = false
                }
            }

            ExpandedView {
                id: expandedView
                anchors { top: listHeader.bottom; left: parent.left; right: parent.right }
                width: parent.width

                onImplicitHeightChanged: {
                    if (root.isExpanded)
                        root.animH = Math.max(implicitHeight + listHeader.height, 80)
                }
            }
        }

        // ── Hover: dot → expand ──────────────────────────────────
        HoverHandler {
            enabled: !root.isExpanded && !root.isDragging
            onHoveredChanged: {
                if (hovered) expandTimer.start()
                else         expandTimer.stop()
            }
        }

        // ── Hover: list → collapse ────────────────────────────────
        HoverHandler {
            enabled: root.isExpanded && !root.isDragging
            onHoveredChanged: {
                if (!hovered) collapseTimer.start()
                else          collapseTimer.stop()
            }
        }

        // ── Drag (dot mode): full card as drag handle ─────────────
        MouseArea {
            anchors.fill: parent
            visible: !root.isExpanded
            // Let hover events pass through to HoverHandler
            hoverEnabled: false
            onPressed: {
                root.isDragging = true
                expandTimer.stop()
                root.startSystemMove()
            }
            onReleased: root.isDragging = false
        }
    }

    Timer {
        id: expandTimer
        interval: 300
        onTriggered: root.isExpanded = true
    }

    Timer {
        id: collapseTimer
        interval: 500
        onTriggered: root.isExpanded = false
    }

    onIsExpandedChanged: {
        if (isExpanded) {
            animH = Math.max(expandedView.implicitHeight + 36, 80)
        } else {
            animH = dotD
        }
    }

    Connections {
        target: appSettings
        function onThemeChanged() { Theme.name = appSettings.theme }
        function onShapeChanged() { Theme.shape = appSettings.shape }
    }

    Component.onCompleted: {
        Theme.name = appSettings.theme
        Theme.shape = appSettings.shape
        if (isExpanded) animH = 280
        refreshAgentTypes()
    }
}
