import QtQuick
import QtQuick.Controls
import Keys.Ui

/// One menu item, with an optional shortcut shown on the right.
///
/// Carries its own contentItem rather than relying on ContextMenu's delegate.
/// A Menu's delegate replaces the whole item, so a property declared here would
/// never reach it - styling the item itself is what lets a menu row show a
/// shortcut at all.
///
/// The shortcut is a label. Keys routes keystrokes through the editor's own key
/// handling so a binding means the same thing whether or not a menu is open;
/// registering a second one here would give two owners for one chord.
MenuItem {
    id: root

    /// Shown right-aligned, quieter than the label - a reminder, not the thing
    /// being chosen.
    property string shortcut: ""

    implicitHeight: Metrics.rowHeight
    implicitWidth: 240
    leftPadding: 12
    rightPadding: 12

    contentItem: Item {
        implicitHeight: label.implicitHeight

        Text {
            id: label

            anchors.left: parent.left
            anchors.right: shortcutText.left
            anchors.rightMargin: Metrics.spacingMedium
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            // An enabled item reads as primary text, not secondary.
            //
            // It used to be textSecondary against textTertiary for disabled -
            // 0.755 and 0.615 lightness, close enough that a menu with a few
            // items greyed out looked like a menu with *everything* greyed out,
            // and nothing appeared clickable. Disabled items also drop to 55%
            // opacity, so the difference is carried twice rather than resting
            // on a hue step the eye has to hunt for.
            color: root.enabled ? Theme.textPrimary : Theme.textTertiary
            opacity: root.enabled ? 1.0 : 0.55
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            elide: Text.ElideRight
        }

        Text {
            id: shortcutText

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.shortcut
            color: Theme.textTertiary
            opacity: root.enabled ? 1.0 : 0.55
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeLabel
        }
    }

    background: Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        radius: Metrics.radiusSmall
        color: root.highlighted ? Theme.selection : "transparent"
    }
}
