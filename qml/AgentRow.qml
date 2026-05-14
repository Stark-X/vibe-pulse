import QtQuick
import qml 1.0

Item {
    id: root
    width: parent.width
    height: 52

    required property string  name
    required property string  toolType
    required property string  status
    required property bool    sessionBusy
    required property string  sessionName
    required property string  currentStep
    required property bool    canJump
    required property int     index

    signal jumpRequested(int idx)

    // ── background ────────────────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        anchors.leftMargin:  8
        anchors.rightMargin: 8
        radius: 8
        color: hov.containsMouse ? Qt.rgba(1,1,1,0.04) : "transparent"

        MouseArea {
            id: hov
            anchors.fill: parent
            hoverEnabled: true
            onClicked: if (root.canJump) root.jumpRequested(root.index)
            cursorShape: root.canJump ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    // ── heartbeat dot ─────────────────────────────────────────────────────────
    Rectangle {
        id: dot
        x: 20; y: (root.height - 8) / 2
        width: 8; height: 8; radius: 4
        color: root.sessionBusy ? Theme.running : Theme.idle

        SequentialAnimation on opacity {
            loops: Animation.Infinite
            running: root.sessionBusy
            NumberAnimation { to: 0.35; duration: 900; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1.0;  duration: 900; easing.type: Easing.InOutSine }
        }
    }

    // ── name + tooltype ───────────────────────────────────────────────────────
    Column {
        anchors.left:  dot.right
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            width: parent.width
            text: root.toolType
            font.pixelSize: 11
            font.letterSpacing: 0.6
            color: Theme.textMuted
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: root.name + (root.sessionName ? "  ·  " + root.sessionName : "")
            font.pixelSize: 13
            font.weight: Font.Medium
            color: Theme.textPrimary
            elide: Text.ElideRight
        }
        Text {
            visible: root.currentStep !== ""
            width: parent.width
            text: root.currentStep
            font.pixelSize: 11
            color: Theme.accent
            elide: Text.ElideRight
        }
    }
}
