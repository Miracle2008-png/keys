import QtQuick
import QtQuick.Controls
import Keys.Ui

/// A small square button in the find bar: an option that stays pressed, or a
/// one-shot action like next and previous.
///
/// One component for both because they look and behave identically apart from
/// whether `active` is bound to anything - two near-identical components would
/// drift apart.
Rectangle {
    id: root

    property string label: ""
    property string tip: ""

    /// Held down, for the option toggles. Left false by the action buttons.
    property bool active: false

    signal toggled()

    implicitWidth: 26
    implicitHeight: 26
    width: implicitWidth
    height: implicitHeight
    radius: Metrics.radiusSmall

    color: !enabled ? "transparent"
         : root.active ? Theme.selection
         : mouse.containsMouse ? Theme.bgHover
         : "transparent"

    border.width: root.active ? 1 : 0
    border.color: Theme.accentSoftBorder

    Behavior on color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    Text {
        anchors.centerIn: parent
        text: root.label
        color: !root.enabled ? Theme.textTertiary
             : root.active ? Theme.accent
             : mouse.containsMouse ? Theme.textPrimary
             : Theme.textSecondary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLabel
    }

    MouseArea {
        id: mouse

        anchors.fill: parent
        hoverEnabled: true
        enabled: root.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled()
    }

    ToolTip.visible: mouse.containsMouse && root.tip.length > 0
    ToolTip.text: root.tip
    ToolTip.delay: 500
}
