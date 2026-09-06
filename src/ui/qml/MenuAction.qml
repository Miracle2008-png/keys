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
            color: root.enabled ? (root.highlighted ? Theme.textPrimary
                                                    : Theme.textSecondary)
                                : Theme.textTertiary
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
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeLabel
        }
    }

    background: Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 5
        anchors.rightMargin: 5
        radius: Metrics.radiusSmall
        color: root.highlighted ? Theme.accentSoft : "transparent"
    }
}
