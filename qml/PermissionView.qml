import QtQuick
import QtQuick.Controls.Basic
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

        Rectangle { width: parent.width; height: 1; color: Theme.borderWeak }

        Item { width: 1; height: Theme.s1 }

        // Warn icon + tool · target
        Row {
            width: parent.width
            spacing: Theme.s2

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 22; height: 22; radius: 6
                color: Qt.rgba(Theme.peach.r, Theme.peach.g, Theme.peach.b, 0.12)
                border.color: Qt.rgba(Theme.peach.r, Theme.peach.g, Theme.peach.b, 0.35)
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "!"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    color: Theme.peach
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 30
                text: {
                    const tool   = agentData.permissionTool   ?? ""
                    const target = agentData.permissionTarget ?? ""
                    if (tool && target) return tool + "  " + target
                    return target || "File operation"
                }
                font.family: "JetBrains Mono"
                font.pixelSize: 11
                color: Theme.text1
                elide: Text.ElideMiddle
            }
        }

        // No / Yes
        Row {
            id: btnRow
            width: parent.width
            spacing: Theme.s2

            property bool anyHovered: denyMa.containsMouse || allowMa.containsMouse

            Rectangle {
                id: denyBtn
                width: (parent.width - Theme.s2) / 2
                height: 36
                radius: 10
                color: denyMa.containsMouse ? Theme.surface2 : Theme.surface1
                border.color: Qt.rgba(Theme.coral.r, Theme.coral.g, Theme.coral.b,
                                      denyMa.containsMouse ? 0.55 : 0.38)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 100 } }
                Behavior on border.color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "No"
                    color: Theme.coral
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                MouseArea {
                    id: denyMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const txt = noteInput.text.trim()
                        if (txt.length > 0)
                            agentModel.decidePermissionAmend(agentRow, false, txt)
                        else
                            agentModel.decidePermission(agentRow, false)
                        noteInput.text = ""
                    }
                }
            }

            Rectangle {
                id: allowBtn
                width: (parent.width - Theme.s2) / 2
                height: 36
                radius: 10
                color: allowMa.containsMouse ? Theme.surface2 : Theme.surface1
                border.color: Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b,
                                      allowMa.containsMouse ? 0.60 : 0.38)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 100 } }
                Behavior on border.color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "Yes"
                    color: Theme.green
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                MouseArea {
                    id: allowMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const txt = noteInput.text.trim()
                        if (txt.length > 0)
                            agentModel.decidePermissionAmend(agentRow, true, txt)
                        else
                            agentModel.decidePermission(agentRow, true)
                        noteInput.text = ""
                    }
                }
            }
        }

        // Hover-expanded note input
        Column {
            id: noteArea
            width: parent.width
            spacing: Theme.s2
            visible: btnRow.anyHovered || noteInput.activeFocus || noteInput.text.length > 0
            clip: true

            Rectangle {
                width: parent.width
                height: 54
                radius: 8
                color: Theme.surface1
                border.color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b,
                                      noteInput.activeFocus ? 0.55 : 0.30)
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: 100 } }

                TextInput {
                    id: noteInput
                    anchors { fill: parent; margins: Theme.s2 }
                    color: Theme.text1
                    font.family: "JetBrains Mono"
                    font.pixelSize: 11
                    wrapMode: TextInput.Wrap
                    clip: true

                    Text {
                        anchors.fill: parent
                        text: "Add a note… (optional)"
                        color: Theme.text3
                        font: parent.font
                        visible: parent.text.length === 0 && !parent.activeFocus
                    }

                    Keys.onReturnPressed: {
                        const txt = noteInput.text.trim()
                        if (txt.length > 0)
                            agentModel.decidePermissionAmend(agentRow, true, txt)
                        else
                            agentModel.decidePermission(agentRow, true)
                        noteInput.text = ""
                    }
                }
            }
        }

        Item { width: 1; height: Theme.s2 }
    }

    onAgentRowChanged: {
        noteInput.text = ""
    }
}
