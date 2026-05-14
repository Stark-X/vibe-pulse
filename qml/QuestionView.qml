import QtQuick
import QtQuick.Layouts
import qml 1.0

Item {
    id: root
    width: parent ? parent.width : 0
    implicitHeight: innerCol.implicitHeight + 20
    property int agentRow: -1

    readonly property var agentData: agentRow >= 0 ? agentModel.get(agentRow) : {}

    Column {
        id: innerCol
        anchors { left: parent.left; right: parent.right; top: parent.top
                  leftMargin: Theme.s4; rightMargin: Theme.s4; topMargin: 0 }
        spacing: Theme.s3

        // Top divider
        Rectangle { width: parent.width; height: 1; color: Theme.borderWeak }

        Item { width: 1; height: Theme.s2 }

        // Prompt text
        Text {
            width: parent.width
            text: agentData.questionPrompt ?? ""
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.text1
            wrapMode: Text.WordWrap
            lineHeight: 1.45
        }

        // Options list
        Column {
            width: parent.width
            spacing: Theme.s2

            Repeater {
                model: agentData.questionOptions ?? []

                delegate: Item {
                    id: optItem
                    width: parent.width
                    height: 40

                    property bool hovered: optMa.containsMouse

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        // surface1 base eliminates background bleed-through
                        color: optItem.hovered ? Theme.surface2 : Theme.surface1
                        border.color: optItem.hovered ? Qt.rgba(1,1,1,0.18) : Qt.rgba(1,1,1,0.08)
                        border.width: 1

                        Behavior on color { ColorAnimation { duration: 100 } }
                        Behavior on border.color { ColorAnimation { duration: 100 } }

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.s3
                            spacing: Theme.s3

                            Keycap {
                                anchors.verticalCenter: parent.verticalCenter
                                key: "❖" + (index + 1)
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                                font.pixelSize: 12
                                color: Qt.rgba(1, 1, 1, optItem.hovered ? 0.92 : 0.78)
                            }
                        }

                        MouseArea {
                            id: optMa
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: agentModel.answerQuestion(agentRow, index)
                            cursorShape: Qt.PointingHandCursor
                        }
                    }

                    transform: Translate {
                        x: optItem.hovered ? 2 : 0
                        Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutQuad } }
                    }
                }
            }
        }

        // Footer
        Item {
            width: parent.width
            height: 28

            Rectangle {
                anchors.top: parent.top
                width: parent.width; height: 1
                color: Theme.borderWeak
            }

            Text {
                anchors { left: parent.left; bottom: parent.bottom }
                text: (agentData.toolType ?? "Agent")
                font.family: "JetBrains Mono"
                font.pixelSize: 10
                color: Theme.text3
            }

            Row {
                anchors { right: parent.right; bottom: parent.bottom }
                spacing: 5

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "dismiss"
                    font.family: "JetBrains Mono"
                    font.pixelSize: 10
                    color: Theme.text3
                }

                Keycap {
                    anchors.verticalCenter: parent.verticalCenter
                    key: "Esc"
                }
            }
        }

        Item { width: 1; height: Theme.s1 }
    }
}
