import QtQuick
import qml 1.0
import QtQuick.Window
import QtQuick.Layouts

Window {
    id: root
    visible: true
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    property string backendState: agentModel.globalState
    property bool userExpanded: (typeof mockForceExpanded !== "undefined" && mockForceExpanded)
    property string pulseState: userExpanded ? "expanded" : (agentModel.idleCollapsed ? "idle" : backendState)

    readonly property var widthByState: {
        "idle": 280,
        "working": 380,
        "permission": 440,
        "question": 420,
        "plan": 460,
        "expanded": 420
    }

    width: widthByState[pulseState] ?? 380
    height: header.height + (body.active && body.item ? body.item.implicitHeight + 8 : 8)

    Behavior on width { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }
    Behavior on height { NumberAnimation { duration: 280; easing.type: Easing.OutQuint } }

    Rectangle {
        id: card
        anchors.fill: parent
        color: Qt.rgba(0.078, 0.075, 0.11, 0.93)
        border.color: "#3a384e"
        border.width: 1
        radius: Theme.pulseRadius

        Behavior on radius { NumberAnimation { duration: 280 } }

        // Top sheen
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; margins: 1 }
            height: 1
            radius: parent.radius
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.08) }
                GradientStop { position: 1.0; color: "transparent" }
            }
        }

        HeaderBar {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            pulseState: root.pulseState
            agentName: agentModel.activeAgentRow >= 0 ? (agentModel.get(agentModel.activeAgentRow).name ?? "") : ""
            agentSub: agentModel.activeAgentRow >= 0 ? (agentModel.get(agentModel.activeAgentRow).currentStep ?? "") : ""
            agentCount: agentModel.count
            liveTime: clockTimer.timeStr
            onClicked: root.userExpanded = !root.userExpanded
        }

        // Clock tick — updates liveTime every 30 seconds
        Timer {
            id: clockTimer
            interval: 30000
            repeat: true
            running: true
            property string timeStr: Qt.formatTime(new Date(), "HH:mm")
            onTriggered: timeStr = Qt.formatTime(new Date(), "HH:mm")
        }

        Loader {
            id: body
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            active: pulseState !== "idle" && pulseState !== "working"
            sourceComponent: {
                if (pulseState === "permission") return permComp
                if (pulseState === "question") return questComp
                if (pulseState === "plan") return planComp
                if (pulseState === "expanded") return expandedComp
                return null
            }

            onLoaded: {
                item.opacity = 0
                item.y = -6
                bodyAnim.start()
            }
        }

        ParallelAnimation {
            id: bodyAnim
            NumberAnimation { target: body.item; property: "opacity"; to: 1; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { target: body.item; property: "y"; to: 0; duration: 200; easing.type: Easing.OutCubic }
        }
    }

    Component { id: permComp; PermissionView { agentRow: agentModel.activeAgentRow } }
    Component { id: questComp; QuestionView { agentRow: agentModel.activeAgentRow } }
    Component { id: planComp; PlanView { agentRow: agentModel.activeAgentRow } }
    Component { id: expandedComp; ExpandedView { width: root.width } }

    Connections {
        target: appSettings
        function onThemeChanged() { Theme.name = appSettings.theme }
        function onShapeChanged() { Theme.shape = appSettings.shape }
    }

    Component.onCompleted: {
        Theme.name = appSettings.theme
        Theme.shape = appSettings.shape
    }
}
