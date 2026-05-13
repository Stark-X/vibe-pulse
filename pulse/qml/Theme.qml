pragma Singleton
import QtQuick

QtObject {
    id: root

    property string name: "midnight"

    // Resolved colors
    readonly property color bg:          d.bg
    readonly property color border:      d.border
    readonly property color textPrimary: d.textPrimary
    readonly property color textMuted:   d.textMuted
    readonly property color accent:      d.accent
    readonly property color running:     d.running
    readonly property color idle:        "#6b7280"
    readonly property int   radius:      d.radius

    // Private palette switcher
    property QtObject d: midnight

    onNameChanged: {
        if      (name === "aurora")  d = aurora
        else if (name === "carbon")  d = carbon
        else                         d = midnight
    }

    property QtObject midnight: QtObject {
        readonly property color bg:          Qt.rgba(0.06, 0.07, 0.12, 0.92)
        readonly property color border:      Qt.rgba(1, 1, 1, 0.06)
        readonly property color textPrimary: "#e8eaf0"
        readonly property color textMuted:   "#7880a0"
        readonly property color accent:      "#7c8cff"
        readonly property color running:     "#4ade80"
        readonly property int   radius:      12
    }

    property QtObject aurora: QtObject {
        readonly property color bg:          Qt.rgba(0.07, 0.04, 0.14, 0.92)
        readonly property color border:      Qt.rgba(1, 0.6, 1, 0.08)
        readonly property color textPrimary: "#f0e8ff"
        readonly property color textMuted:   "#9080b0"
        readonly property color accent:      "#c084fc"
        readonly property color running:     "#34d399"
        readonly property int   radius:      14
    }

    property QtObject carbon: QtObject {
        readonly property color bg:          Qt.rgba(0.09, 0.09, 0.09, 0.94)
        readonly property color border:      Qt.rgba(1, 1, 1, 0.08)
        readonly property color textPrimary: "#e0e0e0"
        readonly property color textMuted:   "#707070"
        readonly property color accent:      "#a0a0a0"
        readonly property color running:     "#60c060"
        readonly property int   radius:      8
    }
}
