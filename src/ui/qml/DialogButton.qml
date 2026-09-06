import QtQuick
import Keys.Ui

/// A dialog footer button.
///
/// Two roles: the confirming action is filled with the accent, everything else
/// is a quiet outline. Basic's default is a grey block that matches nothing in
/// the design and gives confirm and cancel identical weight.
Rectangle {
    id: root

    property alias text: label.text

    /// The action the dialog exists to perform. Exactly one per dialog.
    property bool primary: false

    /// Marks an action that destroys something. Painted in the theme's red so
    /// the button does not look like every other confirmation - the colour is
    /// the warning, and it costs the user nothing to notice.
    property bool destructive: false

    readonly property color fill: destructive ? Theme.red : Theme.accent
    readonly property color fillHover: destructive ? Theme.red : Theme.accentHover
    readonly property color fillPressed: destructive ? Theme.red : Theme.accentPressed

    signal clicked()

    implicitWidth: Math.max(88, label.implicitWidth + Metrics.spacingLarge * 2)
    implicitHeight: 32
    radius: Metrics.radiusSmall

    // Pressed goes darker than resting. A button that only lightens on hover
    // gives no feedback at the moment of the click, which is the one moment
    // the user is looking for it.
    color: primary ? (mouse.pressed ? fillPressed
                      : mouse.containsMouse ? fillHover : fill)
                   : (mouse.pressed ? Theme.bgElevated
                      : mouse.containsMouse ? Theme.bgHover : "transparent")
    border.width: primary ? 0 : 1
    border.color: Theme.border

    Behavior on color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: label.text
    Accessible.onPressAction: root.clicked()

    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: parent.radius + 3
        color: "transparent"
        border.width: 2
        border.color: root.fill
        visible: root.activeFocus
    }

    Text {
        id: label
        anchors.centerIn: parent
        color: root.primary ? "white" : Theme.textSecondary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        font.weight: root.primary ? Font.Medium : Font.Normal
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }

    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()
}
