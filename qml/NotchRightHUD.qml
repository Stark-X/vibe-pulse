import QtQuick
import qml 1.0

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    width: 60
    height: 32

    function usageColor(pct) {
        if (pct > 85) return Theme.coral
        if (pct > 60) return Theme.peach
        return "#8b88a8"
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: "#0a0a0c"

        Row {
            anchors.centerIn: parent
            spacing: 5

            Canvas {
                id: gauge
                width: 16; height: 16
                anchors.verticalCenter: parent.verticalCenter

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()

                    var cx = width / 2
                    var cy = height / 2
                    var r = Math.min(cx, cy) - 1.5
                    var pct = subscriptionMonitor.claudeUtilization / 100

                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                    ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.10)
                    ctx.lineWidth = 2
                    ctx.stroke()

                    if (pct > 0.001) {
                        var startAngle = -Math.PI / 2
                        var endAngle   = startAngle + pct * 2 * Math.PI
                        ctx.beginPath()
                        ctx.arc(cx, cy, r, startAngle, endAngle)
                        ctx.strokeStyle = root.usageColor(subscriptionMonitor.claudeUtilization)
                        ctx.lineWidth = 2
                        ctx.lineCap = "round"
                        ctx.stroke()
                    }
                }

                Connections {
                    target: subscriptionMonitor
                    function onDataChanged() { gauge.requestPaint() }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: Math.round(subscriptionMonitor.claudeUtilization) + "%"
                font.family: "JetBrains Mono"
                font.pixelSize: 10
                font.weight: Font.Medium
                color: root.usageColor(subscriptionMonitor.claudeUtilization)
            }
        }
    }
}
