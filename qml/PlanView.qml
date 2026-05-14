import QtQuick
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

        // Header: subtitle only (no duplicate plan chip)
        Text {
            text: "Review before approving"
            font.pixelSize: 11
            color: Theme.text2
        }

        // Markdown content box
        Rectangle {
            width: parent.width
            height: Math.min(mdFlick.contentHeight + 20, 180)
            radius: 8
            color: Theme.surface1
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            clip: true

            Flickable {
                id: mdFlick
                anchors { fill: parent; margins: Theme.s3 }
                contentWidth: width
                contentHeight: mdText.implicitHeight
                clip: true

                Text {
                    id: mdText
                    width: parent.width
                    text: agentData.planHtml ?? ""
                    textFormat: Text.RichText
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Qt.rgba(1, 1, 1, 0.72)
                    lineHeight: 1.55
                }
            }
        }

        // Action buttons — equal weight
        Row {
            width: parent.width
            spacing: Theme.s2

            // Comment — neutral ghost
            Rectangle {
                width: (parent.width - Theme.s2) / 2
                height: 36
                radius: 10
                color: commentMa.containsMouse ? Theme.surface2 : Theme.surface1
                border.color: Qt.rgba(1, 1, 1, commentMa.containsMouse ? 0.22 : 0.12)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 100 } }

                Text {
                    anchors.centerIn: parent
                    text: "Comment"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Qt.rgba(1, 1, 1, 0.78)
                }

                MouseArea {
                    id: commentMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const txt = commentInput.text.trim()
                        if (txt.length > 0) {
                            agentModel.commentPlan(agentRow, txt)
                            commentInput.text = ""
                        } else {
                            commentInput.forceActiveFocus()
                        }
                    }
                }
            }

            // Approve — ghost tinted green
            Rectangle {
                width: (parent.width - Theme.s2) / 2
                height: 36
                radius: 10
                color: approveMa.containsMouse
                    ? Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b, 0.18)
                    : Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b, 0.08)
                border.color: Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b, approveMa.containsMouse ? 0.60 : 0.38)
                border.width: 1

                Behavior on color { ColorAnimation { duration: 100 } }
                Behavior on border.color { ColorAnimation { duration: 100 } }

                Row {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Approve"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: Theme.green
                    }

                    Keycap {
                        anchors.verticalCenter: parent.verticalCenter
                        key: "↵"
                    }
                }

                MouseArea {
                    id: approveMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: agentModel.approvePlan(agentRow)
                }
            }
        }

        // Comment input area — shown when Comment is hovered or has focus/text
        Column {
            id: commentArea
            width: parent.width
            spacing: 0
            visible: commentMa.containsMouse || commentInput.activeFocus || commentInput.text.length > 0

            Rectangle {
                width: parent.width
                height: 54
                radius: 8
                color: Theme.surface1
                border.color: Qt.rgba(1, 1, 1, commentInput.activeFocus ? 0.22 : 0.12)
                border.width: 1
                Behavior on border.color { ColorAnimation { duration: 100 } }

                TextInput {
                    id: commentInput
                    anchors { fill: parent; margins: Theme.s2 }
                    color: Theme.text1
                    font.family: "JetBrains Mono"
                    font.pixelSize: 11
                    wrapMode: TextInput.Wrap
                    clip: true

                    Text {
                        anchors.fill: parent
                        text: "Add a comment…"
                        color: Theme.text3
                        font: parent.font
                        visible: parent.text.length === 0 && !parent.activeFocus
                    }

                    Keys.onReturnPressed: {
                        const txt = commentInput.text.trim()
                        if (txt.length > 0) {
                            agentModel.commentPlan(agentRow, txt)
                            commentInput.text = ""
                        }
                    }
                }
            }

            Item { width: 1; height: Theme.s2 }
        }

        Item { width: 1; height: Theme.s1 }
    }
}
