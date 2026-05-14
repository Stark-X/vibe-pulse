pragma Singleton
import QtQuick

QtObject {
    id: root

    property string name: "midnight"
    property string shape: "round"

    // Resolved colors
    readonly property color bg0:         d.bg0
    readonly property color bg1:         d.bg1
    readonly property color bg2:         d.bg2
    readonly property color bg3:         d.bg3
    
    // Existing aliases for backward compatibility
    readonly property color bg:          bg2
    readonly property color border:      d.border
    readonly property color borderWeak:  d.border
    readonly property color borderStrong: d.borderStrong
    readonly property color borderColor: borderStrong
    
    readonly property color text1:       d.text1
    readonly property color text2:       d.text2
    readonly property color text3:       d.text3
    
    readonly property color textPrimary: text1
    readonly property color textMuted:   text2
    readonly property color textDim:     text3

    readonly property color violet:      "#c4a0ff"
    readonly property color green:       "#10b981"
    readonly property color peach:       "#fbc19d"
    readonly property color coral:       "#f87171"

    // Spacing grid (4px base)
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 24

    // Agent name → deterministic accent color
    function agentAccent(name) {
        const colors = ["#22d3ee", "#a78bfa", "#f59e0b", "#10b981", "#f472b6", "#60a5fa"]
        let h = 0
        for (let i = 0; i < name.length; i++) h = (h * 31 + name.charCodeAt(i)) & 0xfffff
        return colors[h % colors.length]
    }

    readonly property color accent:      d.accent
    readonly property color running:     d.running
    readonly property color idle:        "#6b7280"
    readonly property color surface1:    d.surface1
    readonly property color surface2:    d.surface2

    readonly property real pulseRadius: shape === "pill" ? 999 : shape === "sharp" ? 4 : 14

    // Private palette switcher
    property QtObject d: midnight

    onNameChanged: {
        if      (name === "aurora")  d = aurora
        else if (name === "carbon")  d = carbon
        else                         d = midnight
    }

    property QtObject midnight: QtObject {
        readonly property color bg0:         Qt.rgba(0.04, 0.05, 0.09, 0.94)
        readonly property color bg1:         Qt.rgba(0.05, 0.06, 0.11, 0.94)
        readonly property color bg2:         Qt.rgba(0.06, 0.07, 0.12, 0.92)
        readonly property color bg3:         Qt.rgba(0.12, 0.13, 0.20, 0.92)
        readonly property color surface1:    Qt.rgba(0.10, 0.11, 0.16, 0.98)
        readonly property color surface2:    Qt.rgba(0.13, 0.14, 0.20, 1.00)
        readonly property color border:      Qt.rgba(1, 1, 1, 0.06)
        readonly property color borderStrong: Qt.rgba(1, 1, 1, 0.12)
        readonly property color text1:       "#ebe7f7"
        readonly property color text2:       "#9d9ab5"
        readonly property color text3:       "#6b6885"
        readonly property color accent:      "#7c8cff"
        readonly property color running:     "#10b981"
    }

    property QtObject aurora: QtObject {
        readonly property color bg0:         Qt.rgba(0.05, 0.03, 0.11, 0.94)
        readonly property color bg1:         Qt.rgba(0.06, 0.03, 0.13, 0.94)
        readonly property color bg2:         Qt.rgba(0.07, 0.04, 0.14, 0.92)
        readonly property color bg3:         Qt.rgba(0.14, 0.08, 0.25, 0.92)
        readonly property color surface1:    Qt.rgba(0.12, 0.07, 0.20, 0.98)
        readonly property color surface2:    Qt.rgba(0.15, 0.09, 0.25, 1.00)
        readonly property color border:      Qt.rgba(1, 0.6, 1, 0.08)
        readonly property color borderStrong: Qt.rgba(1, 0.6, 1, 0.16)
        readonly property color text1:       "#f0e8ff"
        readonly property color text2:       "#9080b0"
        readonly property color text3:       "#665884"
        readonly property color accent:      "#c084fc"
        readonly property color running:     "#34d399"
    }

    property QtObject carbon: QtObject {
        readonly property color bg0:         Qt.rgba(0.06, 0.06, 0.06, 0.94)
        readonly property color bg1:         Qt.rgba(0.08, 0.08, 0.08, 0.95)
        readonly property color bg2:         Qt.rgba(0.09, 0.09, 0.09, 0.94)
        readonly property color bg3:         Qt.rgba(0.15, 0.15, 0.15, 0.94)
        readonly property color surface1:    Qt.rgba(0.14, 0.14, 0.14, 0.98)
        readonly property color surface2:    Qt.rgba(0.18, 0.18, 0.18, 1.00)
        readonly property color border:      Qt.rgba(1, 1, 1, 0.08)
        readonly property color borderStrong: Qt.rgba(1, 1, 1, 0.15)
        readonly property color text1:       "#e0e0e0"
        readonly property color text2:       "#707070"
        readonly property color text3:       "#505050"
        readonly property color accent:      "#a0a0a0"
        readonly property color running:     "#60c060"
    }
}
