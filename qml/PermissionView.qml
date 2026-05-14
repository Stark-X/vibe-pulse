import QtQuick
import QtQuick.Controls.Basic
import qml 1.0

Item {
    id: root
    width: parent ? parent.width : 0
    implicitHeight: innerCol.implicitHeight + 20
    property int agentRow: -1

    readonly property var agentData: agentRow >= 0 ? agentModel.get(agentRow) : {}

    property bool amendMode: false

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

        // No / Amend / Yes  (normal mode)
        Row {
            width: parent.width
            spacing: Theme.s2
            visible: !root.amendMode

            Rectangle {
                id: denyBtn
                width: (parent.width - Theme.s2 * 2) / 3
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
                    onClicked: agentModel.decidePermission(agentRow, false)
                }
            }

            Rectangle {
                id: amendBtn
                width: (parent.width - Theme.s2 * 2) / 3
                height: 36
                radius: 10
                color: amendMa.containsMouse ? Theme.surface2 : Theme.surface1
                border.color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b,
                                      amendMa.containsMouse ? 0.55 : 0.38)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 100 } }
                Behavior on border.color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "Amend"
                    color: Theme.violet
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                MouseArea {
                    id: amendMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.amendMode = true
                        Qt.callLater(function() { amendInput.forceActiveFocus() })
                    }
                }
            }

            Rectangle {
                id: allowBtn
                width: (parent.width - Theme.s2 * 2) / 3
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
                    onClicked: agentModel.decidePermission(agentRow, true)
                }
            }
        }

        // Amend input panel
        Column {
            width: parent.width
            spacing: Theme.s2
            visible: root.amendMode

            Rectangle {
                width: parent.width
                height: 60
                radius: 8
                color: Theme.surface1
                border.color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b,
                                      amendInput.activeFocus ? 0.60 : 0.38)
                border.width: 1

                Behavior on border.color { ColorAnimation { duration: 100 } }

                TextInput {
                    id: amendInput
                    anchors { fill: parent; margins: Theme.s2 }
                    color: Theme.text1
                    font.family: "JetBrains Mono"
                    font.pixelSize: 11
                    wrapMode: TextInput.Wrap
                    clip: true

                    Text {
                        anchors.fill: parent
                        text: "Describe amendment…"
                        color: Theme.text3
                        font: parent.font
                        visible: parent.text.length === 0 && !parent.activeFocus
                    }

                    Keys.onEscapePressed: {
                        root.amendMode = false
                        amendInput.text = ""
                    }
                }
            }

            // Cancel / No with amend / Yes with amend
            Row {
                width: parent.width
                spacing: Theme.s2

                readonly property bool hasText: amendInput.text.trim().length > 0

                Rectangle {
                    width: (parent.width - Theme.s2 * 2) / 3
                    height: 30
                    radius: 8
                    color: cancelMa.containsMouse ? Theme.surface2 : Theme.surface1
                    border.color: Qt.rgba(Theme.text3.r, Theme.text3.g, Theme.text3.b, 0.30)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Cancel"
                        color: Theme.text2
                        font.pixelSize: 10
                        font.weight: Font.Medium
                    }

                    MouseArea {
                        id: cancelMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.amendMode = false
                            amendInput.text = ""
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - Theme.s2 * 2) / 3
                    height: 30
                    radius: 8
                    color: noAmendMa.containsMouse ? Theme.surface2 : Theme.surface1
                    border.color: Qt.rgba(Theme.coral.r, Theme.coral.g, Theme.coral.b,
                                          parent.hasText
                                          ? (noAmendMa.containsMouse ? 0.55 : 0.38)
                                          : 0.15)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Behavior on border.color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text: "No"
                        color: parent.parent.hasText ? Theme.coral : Theme.text3
                        font.pixelSize: 10
                        font.weight: Font.Medium
                    }

                    MouseArea {
                        id: noAmendMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: parent.parent.hasText
                                     ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            const txt = amendInput.text.trim()
                            if (txt.length === 0) return
                            agentModel.decidePermissionAmend(agentRow, false, txt)
                            root.amendMode = false
                            amendInput.text = ""
                        }
                    }
                }

                Rectangle {
                    width: (parent.width - Theme.s2 * 2) / 3
                    height: 30
                    radius: 8
                    color: yesAmendMa.containsMouse ? Theme.surface2 : Theme.surface1
                    border.color: Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b,
                                          parent.hasText
                                          ? (yesAmendMa.containsMouse ? 0.65 : 0.45)
                                          : 0.15)
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Behavior on border.color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text: "Yes"
                        color: parent.parent.hasText ? Theme.green : Theme.text3
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: yesAmendMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: parent.parent.hasText
                                     ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: {
                            const txt = amendInput.text.trim()
                            if (txt.length === 0) return
                            agentModel.decidePermissionAmend(agentRow, true, txt)
                            root.amendMode = false
                            amendInput.text = ""
                        }
                    }
                }
            }
        }

        Item { width: 1; height: Theme.s2 }
    }

    onAgentRowChanged: {
        amendMode = false
        amendInput.text = ""
    }
}
