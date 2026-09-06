import QtQuick
import Keys.Ui

/// A square icon button with hover feedback and a keyboard focus ring.
///
/// Focus visibility is a requirement, not a nicety: every control reachable by
/// Tab must show where focus is. The ring is drawn outside the button's own
/// bounds so it never shifts the layout when it appears.
Item {
    id: root

    property string source: ""
    property string tooltip: ""
    property bool active: false
    property int size: Metrics.iconButtonSize
    property int iconSize: 17

    signal clicked()

    implicitWidth: size
    implicitHeight: size

    // Disabled buttons stay visible but dim and unfocusable: the layout does not
    // shift when a feature lands, and nothing invites a click that will not work.
    opacity: enabled ? 1.0 : 0.38

    scale: mouse.pressed && root.enabled ? 0.92 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: App.fastAnimationDuration
            easing.type: Easing.OutQuad
        }
    }

    activeFocusOnTab: enabled
    Accessible.role: Accessible.Button
    Accessible.name: tooltip
    Accessible.onPressAction: root.clicked()

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusMedium

        // Pressed is its own state. Without it the button lit up when the
        // pointer arrived and then looked identical while being clicked, so
        // every icon button in the rail, the tabs and the terminal registered a
        // press without acknowledging one.
        color: (mouse.pressed && root.enabled) ? Theme.bgElevated
             : root.active ? Theme.selection
             : (mouse.containsMouse && root.enabled) ? Theme.bgHover
             : "transparent"

        Behavior on color {
            ColorAnimation { duration: App.fastAnimationDuration }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -2
        radius: Metrics.radiusMedium + 2
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        visible: root.activeFocus
    }

    Icon {
        anchors.centerIn: parent
        source: root.source
        size: root.iconSize
        color: root.active ? Theme.accent
             : (mouse.containsMouse && root.enabled) ? Theme.textPrimary
             : Theme.textSecondary

        Behavior on color {
            ColorAnimation { duration: App.fastAnimationDuration }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()
}
