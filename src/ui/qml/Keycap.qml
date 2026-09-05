import QtQuick
import Keys.Ui

/// A keyboard shortcut rendered as a key cap.
///
/// Monospaced and boxed so shortcuts are scannable in a column and never confused
/// with the surrounding prose.
Rectangle {
    id: root

    property alias text: label.text

    implicitWidth: label.implicitWidth + 10
    implicitHeight: label.implicitHeight + 4
    radius: 4
    color: Theme.bgElevated
    border.width: 1
    border.color: Theme.border

    Text {
        id: label
        anchors.centerIn: parent
        color: Theme.textSecondary
        font.family: Fonts.mono
        font.pointSize: Metrics.fontSizeLabel
    }
}
