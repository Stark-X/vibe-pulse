import QtQuick
import qml 1.0

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    width: 48
    height: 28

    readonly property color stateColor: {
        if (agentModel.globalState === "idle")     return Theme.green
        if (agentModel.globalState === "working")  return "#22d3ee"
        return Theme.coral
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Qt.rgba(0.078, 0.075, 0.11, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        Row {
            anchors.centerIn: parent
            spacing: 4

            Rectangle {
                width: 6; height: 6; radius: 3
                anchors.verticalCenter: parent.verticalCenter
                color: root.stateColor

                Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: agentModel.count
                font.family: "JetBrains Mono"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: Theme.text1
            }
        }
    }
}
