import QtQuick
import Keys.Ui

/// The accent-filled button used for a view's single most important action.
///
/// Deliberately the only filled button style in Keys: if several actions on a
/// screen shout equally, none of them reads as primary.
///
/// `enabled` is honoured in both appearance and behaviour. An Item's `enabled`
/// already stops its MouseArea and key handlers, but nothing about a Rectangle
/// changes colour on its own - so a disabled button would look identical to a
/// live one and read as broken rather than as unavailable.
Rectangle {
    id: root

    property alias text: label.text
    signal clicked()

    implicitWidth: label.implicitWidth + Metrics.spacingLarge * 2
    implicitHeight: 36
    radius: Metrics.radiusMedium
    color: !root.enabled ? Theme.bgHover
         : mouse.pressed ? Theme.accentHover
         : mouse.containsMouse ? Theme.accentHover
         : Theme.accent

    Behavior on color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    activeFocusOnTab: enabled
    Accessible.role: Accessible.Button
    Accessible.name: label.text
    Accessible.onPressAction: root.clicked()

    // Focus ring outside the button's bounds, so showing it never shifts layout.
    Rectangle {
        anchors.fill: parent
        anchors.margins: -3
        radius: parent.radius + 3
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        visible: root.activeFocus
    }

    Text {
        id: label
        anchors.centerIn: parent
        color: root.enabled ? "white" : Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLarge
        font.weight: Font.Medium
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        // No pointing hand on a button that will not respond: the cursor is the
        // first thing that says whether something is clickable.
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.clicked()
    }

    Keys.onReturnPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()
}
