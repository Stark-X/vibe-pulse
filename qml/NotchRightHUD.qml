import QtQuick
import qml 1.0

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    width: 32
    height: 32

    function usageColor(pct) {
        if (pct > 85) return Theme.coral
        if (pct > 60) return Theme.peach
        return Theme.text2
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Qt.rgba(0.078, 0.075, 0.11, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        Canvas {
            id: gauge
            anchors.fill: parent
            anchors.margins: 5

            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()

                var cx = width / 2
                var cy = height / 2
                var r = Math.max(1, Math.min(cx, cy) - 2)
                var pct = subscriptionMonitor.claudeUtilization / 100

                // Background arc
                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.06)
                ctx.lineWidth = 2.5
                ctx.stroke()

                // Value arc
                if (pct > 0.001) {
                    var startAngle = -Math.PI / 2
                    var endAngle = startAngle + pct * 2 * Math.PI
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, startAngle, endAngle)
                    ctx.strokeStyle = root.usageColor(subscriptionMonitor.claudeUtilization)
                    ctx.lineWidth = 2.5
                    ctx.lineCap = "round"
                    ctx.stroke()
                }
            }

            Text {
                anchors.centerIn: parent
                text: Math.round(subscriptionMonitor.claudeUtilization) + "%"
                font.family: "JetBrains Mono"
                font.pixelSize: 7
                font.weight: Font.DemiBold
                color: root.usageColor(subscriptionMonitor.claudeUtilization)
            }

            Connections {
                target: subscriptionMonitor
                function onDataChanged() { gauge.requestPaint() }
            }
        }
    }
}
