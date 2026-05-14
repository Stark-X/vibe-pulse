import QtQuick
import QtQuick.Controls.Basic
import qml 1.0

Item {
    id: root
    width: parent ? parent.width : 0
    // Height driven by parent (Loader anchors.fill); buttons pinned to bottom.
    // Minimum height so layout never collapses.
    implicitHeight: scrollCol.implicitHeight + btnRow.height + Theme.s3 * 2 + Theme.s2

    property int agentRow: -1
    readonly property var agentData: agentRow >= 0 ? agentModel.get(agentRow) : {}

    // ── scrollable info area ──────────────────────────────────────────────────
    Flickable {
        id: flick
        anchors {
            left: parent.left; right: parent.right
            top: parent.top
            // leave room for btn row + spacing
            bottom: btnRow.top; bottomMargin: Theme.s3
        }
        contentHeight: scrollCol.implicitHeight
        clip: true

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
            contentItem: Rectangle {
                implicitWidth: 4
                radius: 2
                color: Qt.rgba(1, 1, 1, 0.25)
            }
            background: Item {}
        }

        Column {
            id: scrollCol
            anchors { left: parent.left; right: parent.right
                      leftMargin: Theme.s4; rightMargin: Theme.s4; topMargin: Theme.s2 }
            spacing: Theme.s2

            Rectangle { width: parent.width; height: 1; color: Theme.borderWeak }

            // Tool badge + "permission required"
            Row {
                spacing: Theme.s2

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: toolLbl.implicitWidth + 12; height: 20; radius: 4
                    color: Qt.rgba(Theme.peach.r, Theme.peach.g, Theme.peach.b, 0.14)
                    border.color: Qt.rgba(Theme.peach.r, Theme.peach.g, Theme.peach.b, 0.42)
                    border.width: 1
                    Text {
                        id: toolLbl
                        anchors.centerIn: parent
                        text: (agentData.permissionTool ?? "TOOL").toUpperCase()
                        font.family: "JetBrains Mono"; font.pixelSize: 9; font.weight: Font.Bold
                        color: Theme.peach
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "permission required"
                    font.pixelSize: 11; color: Theme.text3
                }
            }

            // Command / file path  — prominent, full text, wraps
            Text {
                width: parent.width
                text: agentData.permissionTarget ?? ""
                font.family: "JetBrains Mono"; font.pixelSize: 11; font.weight: Font.Medium
                color: Theme.text1
                wrapMode: Text.Wrap
                visible: text !== ""
            }

            // Description (optional)
            Text {
                width: parent.width
                text: agentData.permissionDesc ?? ""
                font.pixelSize: 10; color: Theme.text3
                wrapMode: Text.Wrap
                visible: text !== ""
            }

            Item { width: 1; height: Theme.s1 }

            // Note input
            Rectangle {
                width: parent.width
                height: Math.max(44, Math.min(noteInput.contentHeight + Theme.s2 * 2, 80))
                radius: 8; color: Theme.surface1
                border.color: Qt.rgba(Theme.violet.r, Theme.violet.g, Theme.violet.b,
                                      noteInput.activeFocus ? 0.55 : 0.22)
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: 100 } }
                Behavior on height       { NumberAnimation  { duration: 100 } }

                TextEdit {
                    id: noteInput
                    anchors { fill: parent; margins: Theme.s2 }
                    color: Theme.text1
                    font.family: "JetBrains Mono"; font.pixelSize: 11
                    wrapMode: TextEdit.Wrap; clip: true; selectByMouse: true

                    Text {
                        anchors.fill: parent
                        text: "Add a note… (Enter = confirm, Shift+Enter = newline)"
                        color: Theme.text3; font: parent.font
                        visible: parent.text.length === 0 && !parent.activeFocus
                        wrapMode: Text.Wrap
                    }

                    Keys.onReturnPressed: event => {
                        if (event.modifiers & Qt.ShiftModifier || event.modifiers & Qt.AltModifier) {
                            event.accepted = false; return
                        }
                        event.accepted = true
                        _decide(true)
                    }
                }
            }

            Item { width: 1; height: Theme.s1 }
        }
    }

    // ── No / Yes — anchored to bottom, always visible ─────────────────────────
    Row {
        id: btnRow
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom
                  leftMargin: Theme.s4; rightMargin: Theme.s4; bottomMargin: Theme.s3 }
        spacing: Theme.s2

        Rectangle {
            width: (parent.width - Theme.s2) / 2; height: 34; radius: 10
            color: denyMa.containsMouse ? Theme.surface2 : Theme.surface1
            border.color: Qt.rgba(Theme.coral.r, Theme.coral.g, Theme.coral.b,
                                  denyMa.containsMouse ? 0.55 : 0.38)
            border.width: 1
            Behavior on color        { ColorAnimation { duration: 100 } }
            Behavior on border.color { ColorAnimation { duration: 100 } }
            Text { anchors.centerIn: parent; text: "No"
                   color: Theme.coral; font.pixelSize: 12; font.weight: Font.Medium }
            MouseArea {
                id: denyMa; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: _decide(false)
            }
        }

        Rectangle {
            width: (parent.width - Theme.s2) / 2; height: 34; radius: 10
            color: allowMa.containsMouse ? Theme.surface2 : Theme.surface1
            border.color: Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b,
                                  allowMa.containsMouse ? 0.60 : 0.38)
            border.width: 1
            Behavior on color        { ColorAnimation { duration: 100 } }
            Behavior on border.color { ColorAnimation { duration: 100 } }
            Text { anchors.centerIn: parent; text: "Yes"
                   color: Theme.green; font.pixelSize: 12; font.weight: Font.DemiBold }
            MouseArea {
                id: allowMa; anchors.fill: parent
                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                onClicked: _decide(true)
            }
        }
    }

    function _decide(allow) {
        const txt = noteInput.text.trim()
        if (txt.length > 0) agentModel.decidePermissionAmend(agentRow, allow, txt)
        else                 agentModel.decidePermission(agentRow, allow)
        noteInput.text = ""
    }

    onAgentRowChanged: { noteInput.text = "" }
}
