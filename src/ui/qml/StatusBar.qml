import QtQuick
import Keys.Ui

/// The 24px status bar.
///
/// The design paints it in the accent color, which makes it the strongest
/// horizontal line in the window - so it carries only state that is true. Nothing
/// here is populated until the subsystem behind it exists.
Rectangle {
    id: root

    height: Metrics.statusBarHeight
    color: Theme.accent

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Ready")
            color: "white"
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "UTF-8"
            color: "white"
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
        }
    }
}
