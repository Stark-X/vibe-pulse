import QtQuick
import qml 1.0

Item {
    id: root
    height: 44
    property string pulseState: "idle"
    property string agentName: ""
    property string agentSub: ""
    property string agentTool: ""
    property int agentCount: 0
    property string liveTime: ""
    property double claudeUtilization: 0
    property double codexUtilization:  0
    property bool   claudeAvailable:   false
    property bool   codexAvailable:    false

    signal clicked()

    function usageColor(pct) {
        if (pct > 85) return Theme.coral
        if (pct > 60) return Theme.peach
        return Theme.text2
    }

    // State → indicator color: idle=green, working=cyan, others=coral
    readonly property color stateColor: {
        if (pulseState === "idle")     return Theme.green
        if (pulseState === "working")  return "#22d3ee"
        if (pulseState === "expanded") return Theme.violet
        return Theme.coral
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.clicked()
        cursorShape: Qt.PointingHandCursor
    }

    // Heart dot + ring container (20×20 gives ring room to expand)
    Item {
        id: heartArea
        width: 20; height: 20
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter

        Rectangle {
            id: ring
            anchors.centerIn: parent
            width: 8; height: 8; radius: 4
            color: "transparent"
            border.color: stateColor
            border.width: 1.5
            opacity: 0
            transformOrigin: Item.Center

            Behavior on border.color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }

            SequentialAnimation on scale {
                loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 2.6; duration: 2000; easing.type: Easing.OutCubic }
            }
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { from: 0.65; to: 0; duration: 2000; easing.type: Easing.OutCubic }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: 8; height: 8; radius: 4
            color: stateColor
            Behavior on color { ColorAnimation { duration: 300; easing.type: Easing.InOutQuad } }
        }
    }

    // Headline + sub
    Column {
        anchors.left: heartArea.right
        anchors.leftMargin: 4
        anchors.right: usageRow.left
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            width: parent.width
            text: "Pulse"
            font.pixelSize: 13
            font.weight: Font.DemiBold
            color: Theme.text1
            elide: Text.ElideRight
        }

        // Sub-text fades when it changes
        Text {
            id: subText
            width: parent.width
            visible: text !== ""
            text: {
                if (pulseState === "idle") return liveTime
                if (pulseState === "working") return agentSub
                if (pulseState === "question") return "waiting on your answer"
                if (pulseState === "plan") return "shared a plan for review"
                if (pulseState === "permission") return "approve " + (agentTool || "edit")
                return ""
            }
            font.family: "JetBrains Mono"
            font.pixelSize: 10
            color: pulseState === "working" ? Qt.rgba(1,1,1,0.72) : Theme.text2
            elide: Text.ElideRight

            Behavior on color { ColorAnimation { duration: 250; easing.type: Easing.InOutQuad } }
            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

            onTextChanged: {
                opacity = 0
                fadeInTimer.restart()
            }
            Timer {
                id: fadeInTimer
                interval: 16
                onTriggered: subText.opacity = 1
            }
        }
    }

    // Subscription usage indicators (idle/working only, optional)
    Row {
        id: usageRow
        anchors.right: metaArea.left
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        Text {
            visible: claudeAvailable && (pulseState === "idle" || pulseState === "working")
            text: "CC " + Math.round(claudeUtilization) + "%"
            font.family: "JetBrains Mono"
            font.pixelSize: 9
            color: root.usageColor(claudeUtilization)
            verticalAlignment: Text.AlignVCenter
        }

        Text {
            visible: codexAvailable && (pulseState === "idle" || pulseState === "working")
            text: "CD " + Math.round(codexUtilization) + "%"
            font.family: "JetBrains Mono"
            font.pixelSize: 9
            color: root.usageColor(codexUtilization)
            verticalAlignment: Text.AlignVCenter
        }
    }

    // Right meta chip / dots
    Item {
        id: metaArea
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        width: metaLoader.implicitWidth
        height: metaLoader.implicitHeight

        Loader {
            id: metaLoader
            anchors.centerIn: parent
            sourceComponent: {
                if (pulseState === "idle")     return idleChip
                if (pulseState === "working")  return dotsComp
                if (pulseState === "expanded") return null
                return stateChip
            }

            Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

            onSourceComponentChanged: {
                opacity = 0
                metaFadeIn.restart()
            }
            Timer {
                id: metaFadeIn
                interval: 16
                onTriggered: metaLoader.opacity = 1
            }
        }
    }

    // ── chip components ──────────────────────────────────────────────────────

    Component {
        id: idleChip
        Rectangle {
            implicitWidth: lbl.implicitWidth + 14
            implicitHeight: 18
            radius: 5
            color: Theme.bg3
            border.color: Theme.borderWeak
            border.width: 1
            Text {
                id: lbl
                anchors.centerIn: parent
                text: agentCount + " live"
                font.family: "JetBrains Mono"
                font.pixelSize: 10
                color: Theme.text2
            }
        }
    }

    // Colored state chip — semi-transparent fill + matching border, dark text
    Component {
        id: stateChip
        Rectangle {
            readonly property color chipColor: {
                if (pulseState === "question" || pulseState === "plan") return Theme.violet
                return Theme.peach
            }
            implicitWidth: chipLbl.implicitWidth + 14
            implicitHeight: 18
            radius: 5
            color: Qt.rgba(chipColor.r, chipColor.g, chipColor.b, 0.12)
            border.color: Qt.rgba(chipColor.r, chipColor.g, chipColor.b, 0.38)
            border.width: 1

            Behavior on color { ColorAnimation { duration: 250; easing.type: Easing.InOutQuad } }
            Behavior on border.color { ColorAnimation { duration: 250; easing.type: Easing.InOutQuad } }

            Text {
                id: chipLbl
                anchors.centerIn: parent
                text: pulseState === "question" ? "asking"
                    : pulseState === "plan"     ? "plan"
                    :                             "permission"
                font.family: "JetBrains Mono"
                font.pixelSize: 10
                font.weight: Font.Medium
                color: chipColor
                Behavior on color { ColorAnimation { duration: 250; easing.type: Easing.InOutQuad } }
            }
        }
    }

    // Three-dot loader
    Component {
        id: dotsComp
        Row {
            spacing: 4
            Repeater {
                model: 3
                Rectangle {
                    width: 3; height: 3; radius: 2
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#22d3ee"
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PauseAnimation   { duration: index * 180 }
                        NumberAnimation  { from: 1; to: 0.15; duration: 550 }
                        NumberAnimation  { from: 0.15; to: 1; duration: 550 }
                    }
                }
            }
        }
    }
}
