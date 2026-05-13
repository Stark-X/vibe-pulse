import QtQuick
import QtQuick.Window

Window {
    id: root
    visible: true
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    width:  440
    height: Math.min(maxContentHeight, Math.max(90, agentList.contentHeight + 57))

    readonly property int maxContentHeight: 321
    readonly property int contentHeight: agentList.contentHeight + 57
    readonly property bool listClipped: contentHeight > maxContentHeight

    // ── theme ─────────────────────────────────────────────────────────────────
    QtObject {
        id: theme
        property string name: "midnight"

        readonly property color bg:          name === "aurora" ? Qt.rgba(0.06,0.03,0.13,0.94)
                                           : name === "carbon" ? Qt.rgba(0.08,0.08,0.08,0.95)
                                           :                     Qt.rgba(0.05,0.06,0.11,0.94)
        readonly property color surface:     name === "aurora" ? Qt.rgba(1,1,1,0.04)
                                           : name === "carbon" ? Qt.rgba(1,1,1,0.05)
                                           :                     Qt.rgba(1,1,1,0.04)
        readonly property color borderColor: name === "aurora" ? Qt.rgba(0.8,0.5,1,0.12)
                                           : name === "carbon" ? Qt.rgba(1,1,1,0.10)
                                           :                     Qt.rgba(0.5,0.6,1,0.10)
        readonly property color divider:     Qt.rgba(1,1,1,0.05)
        readonly property color textPrimary: name === "aurora" ? "#ede8ff"
                                           : name === "carbon" ? "#e8e8e8" : "#e4e8f4"
        readonly property color textMuted:   name === "aurora" ? "#8878aa"
                                           : name === "carbon" ? "#686868" : "#6a7499"
        readonly property color textDim:     name === "aurora" ? "#6658884"
                                           : name === "carbon" ? "#505050" : "#4a5270"
        readonly property color accent:      name === "aurora" ? "#b06ef8"
                                           : name === "carbon" ? "#909090" : "#6c8eff"
        readonly property color accentBg:    name === "aurora" ? Qt.rgba(0.69,0.43,0.97,0.12)
                                           : name === "carbon" ? Qt.rgba(1,1,1,0.08)
                                           :                     Qt.rgba(0.42,0.55,1,0.12)
        readonly property color running:     name === "carbon" ? "#5ab85a" : "#3dd68c"
        readonly property color idle:        "#3d4460"
        readonly property int   radius:      name === "aurora" ? 16
                                           : name === "carbon" ? 10 : 14
    }

    // ── background card ───────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        radius:       theme.radius
        color:        theme.bg
        border.color: theme.borderColor
        border.width: 1

        // subtle inner glow at top edge
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 1
            color: theme.borderColor
            opacity: 0.6
        }

        // ── header ────────────────────────────────────────────────────────────
        Item {
            id: header
            anchors { top: parent.top; left: parent.left; right: parent.right }
            anchors.leftMargin:  16
            anchors.rightMargin: 16
            height: 44

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                // pulsing logo dot
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: theme.accent
                    anchors.verticalCenter: parent.verticalCenter
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.5; duration: 1800; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1.0; duration: 1800; easing.type: Easing.InOutSine }
                    }
                }

                Text {
                    text: "PULSE"
                    font.pixelSize: 11
                    font.letterSpacing: 2.5
                    font.weight: Font.Bold
                    color: theme.textMuted
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // agent count badge
            Rectangle {
                anchors { right: parent.right; verticalCenter: parent.verticalCenter }
                visible: agentList.count > 0
                width:  countText.width + 14
                height: 18
                radius: 9
                color:  theme.accentBg

                Text {
                    id: countText
                    anchors.centerIn: parent
                    text: agentList.count + " active"
                    font.pixelSize: 10
                    font.letterSpacing: 0.5
                    font.weight: Font.Medium
                    color: theme.accent
                }
            }
        }

        // header divider
        Rectangle {
            id: headerDivider
            anchors { top: header.bottom; topMargin: 2; left: parent.left; right: parent.right }
            anchors.leftMargin: 16; anchors.rightMargin: 16
            height: 1
            color: theme.divider
        }

        // ── agent list ────────────────────────────────────────────────────────
        ListView {
            id: agentList
            anchors { top: headerDivider.bottom; topMargin: 4; left: parent.left; right: parent.right; bottom: parent.bottom; bottomMargin: root.listClipped ? 0 : 8 }
            clip:  true
            model: agentModel

            delegate: Item {
                id: delegateItem
                width:  agentList.width
                height: 66

                // hover + click layer
                Rectangle {
                    anchors { fill: parent; leftMargin: 8; rightMargin: 8; topMargin: 2; bottomMargin: 2 }
                    radius: 10
                    color: hov.containsMouse ? theme.surface : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                    MouseArea {
                        id: hov
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape:  model.canJump ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked:    if (model.canJump) agentModel.focusAgent(model.index)
                    }
                }

                // ── LEFT: tool icon with status badge ─────────────────────
                Item {
                    id: iconArea
                    width:  36
                    height: 36
                    anchors { left: parent.left; leftMargin: 18; verticalCenter: parent.verticalCenter }

                    // icon (known tools)
                    Image {
                        visible: model.toolType === "Claude Code" || model.toolType === "Codex"
                        source:  model.toolType === "Claude Code" ? "qrc:/assets/claude-code.png"
                               : model.toolType === "Codex"       ? "qrc:/assets/codex.png" : ""
                        width: 28; height: 28
                        anchors.centerIn: parent
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    // fallback: initials circle
                    Rectangle {
                        visible: model.toolType !== "Claude Code" && model.toolType !== "Codex"
                        anchors.centerIn: parent
                        width: 28; height: 28; radius: 14
                        color: theme.accentBg
                        Text {
                            anchors.centerIn: parent
                            text: model.toolType.charAt(0)
                            font.pixelSize: 13; font.weight: Font.Bold
                            color: theme.accent
                        }
                    }

                    // status badge: bottom-right corner of icon
                    Rectangle {
                        anchors { right: parent.right; bottom: parent.bottom }
                        width: 10; height: 10; radius: 5
                        color: theme.bg
                        // outer ring (bg colour creates a border effect)

                        Rectangle {
                            anchors.centerIn: parent
                            width: 8; height: 8; radius: 4
                            color: model.sessionBusy ? theme.running : theme.idle
                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                running: model.sessionBusy
                                NumberAnimation { to: 0.35; duration: 1000; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0;  duration: 1000; easing.type: Easing.InOutSine }
                            }
                        }
                    }
                }

                // ── RIGHT: name + session + step ──────────────────────────
                Column {
                    anchors {
                        left:           iconArea.right
                        leftMargin:     12
                        right:          parent.right
                        rightMargin:    16
                        verticalCenter: parent.verticalCenter
                    }
                    spacing: 3

                    Text {
                        width: parent.width
                        text:  model.name + (model.sessionName ? "  ·  " + model.sessionName : "")
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: theme.textPrimary
                        elide: Text.ElideRight
                    }

                    Text {
                        visible: (model.currentStep ?? "") !== ""
                        width:   parent.width
                        text:    model.currentStep ?? ""
                        font.pixelSize: 12
                        color: theme.textMuted
                        elide: Text.ElideRight
                    }

                    // show tool name when no step (idle agents)
                    Text {
                        visible: (model.currentStep ?? "") === ""
                        width:   parent.width
                        text:    model.toolType
                        font.pixelSize: 11
                        font.letterSpacing: 0.3
                        color: theme.textDim
                        elide: Text.ElideRight
                    }
                }
            }

            // empty state
            Column {
                anchors.centerIn: parent
                visible: agentList.count === 0
                spacing: 8

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 32; height: 32; radius: 16
                    color: Qt.rgba(1,1,1,0.04)
                    Text {
                        anchors.centerIn: parent
                        text: "◦"
                        font.pixelSize: 18
                        color: theme.textDim
                    }
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "No agents running"
                    color: theme.textMuted
                    font.pixelSize: 13
                }
            }
        }

        // ── clipped indicator ─────────────────────────────────────────────────
        Item {
            visible: root.listClipped
            z: 1
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 72

            // fade gradient
            Rectangle {
                anchors.fill: parent
                anchors.bottomMargin: 0
                radius: theme.radius
                gradient: Gradient {
                    GradientStop { position: 0.3; color: Qt.rgba(theme.bg.r, theme.bg.g, theme.bg.b, 0.0) }
                    GradientStop { position: 0.7; color: Qt.rgba(theme.bg.r, theme.bg.g, theme.bg.b, 0.92) }
                    GradientStop { position: 1.0; color: Qt.rgba(theme.bg.r, theme.bg.g, theme.bg.b, 1.0) }
                }
            }

            Text {
                anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 10 }
                text: "↓  " + Math.ceil((agentList.contentHeight - agentList.height) / 66) + " more hidden"
                font.pixelSize: 10
                font.letterSpacing: 0.5
                font.weight: Font.Medium
                color: theme.textMuted
            }
        }
    }

    Component.onCompleted: theme.name = appSettings.theme
    Connections {
        target: appSettings
        function onThemeChanged() { theme.name = appSettings.theme }
    }
}
