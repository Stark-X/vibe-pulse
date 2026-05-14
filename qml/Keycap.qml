import QtQuick
import qml 1.0

Rectangle {
    property string key: ""
    implicitWidth:  lbl.implicitWidth + 10
    implicitHeight: 18
    radius: 5
    color: Qt.rgba(1, 1, 1, 0.06)
    border.color: Qt.rgba(1, 1, 1, 0.14)
    border.width: 1

    Text {
        id: lbl
        anchors.centerIn: parent
        text: key
        font.family: "JetBrains Mono"
        font.pixelSize: 10
        color: Qt.rgba(1, 1, 1, 0.72)
    }
}
