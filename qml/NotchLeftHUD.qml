import QtQuick
import qml 1.0

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    width: 64
    height: 32

    readonly property color stateColor: {
        if (agentModel.globalState === "idle")    return Theme.green
        if (agentModel.globalState === "working") return "#22d3ee"
        return Theme.coral
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: "#0a0a0c"

        Row {
            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                width: 7; height: 7; radius: 3.5
                anchors.verticalCenter: parent.verticalCenter
                color: root.stateColor

                Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }

                // Pulse ring on working/permission state
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width + 4; height: parent.height + 4
                    radius: width / 2
                    color: "transparent"
                    border.color: root.stateColor
                    border.width: 1
                    opacity: agentModel.globalState !== "idle" ? 0.4 : 0
                    visible: opacity > 0

                    SequentialAnimation on scale {
                        running: agentModel.globalState !== "idle"
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.8; to: 1.6; duration: 1800; easing.type: Easing.OutCubic }
                        NumberAnimation { from: 1.6; to: 0.8; duration: 0 }
                    }

                    Behavior on opacity { NumberAnimation { duration: 300 } }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: agentModel.count
                font.family: "JetBrains Mono"
                font.pixelSize: 12
                font.weight: Font.Medium
                color: "#e8e8f0"
            }
        }
    }
}
