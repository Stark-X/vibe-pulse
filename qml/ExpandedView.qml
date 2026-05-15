import QtQuick
import qml 1.0

Item {
    id: root
    width: parent ? parent.width : 0
    implicitHeight: Math.min(content.implicitHeight, 280) + foot.height

    Flickable {
        id: content
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: Math.min(implicitHeight, 280)
        contentWidth: width
        contentHeight: listCol.implicitHeight
        clip: true

        property real implicitHeight: listCol.implicitHeight

        // Subtle scrollbar
        Rectangle {
            visible: content.contentHeight > content.height
            anchors { right: parent.right; rightMargin: 3; top: parent.top; bottom: parent.bottom }
            width: 2; radius: 1
            color: Qt.rgba(1, 1, 1, 0.15)
        }

        Column {
            id: listCol
            width: parent.width

            Repeater {
                model: agentModel
                delegate: Item {
                    id: rowItem
                    width: listCol.width
                    height: 62

                    readonly property bool isActive: model.sessionBusy
                    readonly property color agentColor: Theme.agentAccent(model.name)

                    opacity: isActive ? 1.0 : 0.6
                    Behavior on opacity { NumberAnimation { duration: 200 } }

                    // Row background: flat full-width, active gets subtle tint
                    Rectangle {
                        anchors.fill: parent
                        radius: 0
                        color: rowItem.isActive
                            ? Qt.rgba(rowItem.agentColor.r, rowItem.agentColor.g,
                                      rowItem.agentColor.b, 0.06)
                            : (rowHov.containsMouse ? Qt.rgba(1,1,1,0.03) : "transparent")

                        Behavior on color { ColorAnimation { duration: 120 } }

                        // Left accent pill — vertically centered, clear of bottom context bar
                        Rectangle {
                            visible: rowItem.isActive
                            width: 3
                            height: 34
                            radius: 2
                            anchors { left: parent.left; verticalCenter: parent.verticalCenter }
                            color: rowItem.agentColor
                            opacity: 0.9
                        }

                        // Bottom separator line
                        Rectangle {
                            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                            height: 1
                            color: Theme.borderWeak
                            opacity: 0.5
                        }

                        // Context usage slim bar
                        Item {
                            visible: model.contextUsed > 0 && model.contextLimit > 0
                            anchors { left: parent.left; leftMargin: 3; right: parent.right; bottom: parent.bottom }
                            height: 2

                            readonly property real usageRatio:
                                model.contextLimit > 0
                                    ? Math.min(model.contextUsed / model.contextLimit, 1.0)
                                    : 0

                            Rectangle {
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                                width: parent.width * parent.usageRatio
                                color: parent.usageRatio > 0.9 ? Theme.coral
                                     : parent.usageRatio > 0.7 ? "#f59e0b"
                                     : Theme.accent
                                opacity: 0.75
                                Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                                Behavior on color { ColorAnimation  { duration: 300 } }
                            }
                        }

                        MouseArea {
                            id: rowHov
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: model.canJump ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: if (model.canJump) agentModel.focusAgent(index)
                        }
                    }

                    // Tool icon (brand icon or monogram fallback)
                    Item {
                        id: iconArea
                        width: 32; height: 32
                        anchors { left: parent.left; leftMargin: 22; verticalCenter: parent.verticalCenter }

                        // Brand icon for known tools
                        Image {
                            id: brandIcon
                            visible: model.toolType === "Claude Code" || model.toolType === "Codex"
                            source: model.toolType === "Claude Code" ? "qrc:/assets/claude-code.png"
                                  : model.toolType === "Codex"       ? "qrc:/assets/codex.png" : ""
                            width: 28; height: 28
                            anchors.centerIn: parent
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            opacity: rowItem.isActive ? 1.0 : 0.45
                            Behavior on opacity { NumberAnimation { duration: 200 } }
                        }

                        // Monogram fallback for unknown tools
                        Rectangle {
                            visible: model.toolType !== "Claude Code" && model.toolType !== "Codex"
                            anchors.fill: parent
                            radius: 9
                            color: Qt.rgba(rowItem.agentColor.r, rowItem.agentColor.g,
                                           rowItem.agentColor.b, rowItem.isActive ? 0.18 : 0.08)
                            border.width: 1
                            border.color: Qt.rgba(rowItem.agentColor.r, rowItem.agentColor.g,
                                                  rowItem.agentColor.b, rowItem.isActive ? 0.45 : 0.18)

                            Text {
                                anchors.centerIn: parent
                                text: model.toolType.charAt(0).toUpperCase()
                                font.family: "JetBrains Mono"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                color: Qt.rgba(rowItem.agentColor.r, rowItem.agentColor.g,
                                               rowItem.agentColor.b, rowItem.isActive ? 0.95 : 0.55)
                            }
                        }

                        // Status badge — use bg0 for higher contrast ring
                        Rectangle {
                            anchors { right: parent.right; bottom: parent.bottom; margins: -2 }
                            width: 10; height: 10; radius: 5
                            color: Theme.bg0
                            border.width: 1
                            border.color: Theme.bg0

                            Rectangle {
                                anchors.centerIn: parent
                                width: 7; height: 7; radius: 3.5
                                color: model.sessionBusy ? Theme.running : Theme.idle

                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    running: model.sessionBusy
                                    NumberAnimation { to: 0.35; duration: 1000 }
                                    NumberAnimation { to: 1.0;  duration: 1000 }
                                }
                            }
                        }
                    }

                    // Name + step
                    Column {
                        anchors { left: iconArea.right; leftMargin: Theme.s3
                                  right: parent.right; rightMargin: Theme.s4
                                  verticalCenter: parent.verticalCenter }
                        spacing: 3

                        Text {
                            width: parent.width
                            text: model.name + (model.sessionName ? "  ·  " + model.sessionName : "")
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: Qt.rgba(1, 1, 1, rowItem.isActive ? 0.92 : 0.55)
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: (model.currentStep ?? "") !== ""
                            width: parent.width
                            text: model.currentStep ?? ""
                            font.pixelSize: 11
                            color: Qt.rgba(1, 1, 1, 0.58)
                            elide: Text.ElideRight
                        }

                        Text {
                            visible: (model.currentStep ?? "") === ""
                            width: parent.width
                            text: model.toolType
                            font.pixelSize: 10
                            font.family: "JetBrains Mono"
                            font.letterSpacing: 0.3
                            color: Qt.rgba(1, 1, 1, rowItem.isActive ? 0.38 : 0.28)
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    // Footer
    Item {
        id: foot
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        height: 32

        Rectangle {
            anchors.top: parent.top
            width: parent.width; height: 1
            color: Theme.borderWeak
        }

        Text {
            anchors { left: parent.left; leftMargin: Theme.s4; verticalCenter: parent.verticalCenter }
            text: agentModel.count + " sessions"
            font.family: "JetBrains Mono"
            font.pixelSize: 10
            color: Theme.text3
        }

        Keycap {
            anchors { right: parent.right; rightMargin: Theme.s4; verticalCenter: parent.verticalCenter }
            key: "Super+Space"
        }
    }
}
