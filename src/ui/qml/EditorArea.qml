import QtQuick
import Keys.Ui

/// The editor region: the dominant area of the window.
///
/// Milestone 1 has no text engine yet, so this shows the empty state rather than
/// a mock editor. Showing fake code here would be exactly the "placeholder
/// disguised as completed functionality" the engineering rules forbid - and it
/// would hide the real state of the project from anyone running the build.
Rectangle {
    id: root

    color: Theme.bgEditor

    Column {
        anchors.centerIn: parent
        spacing: Metrics.spacingLarge
        width: Math.min(360, parent.width - Metrics.spacingLarge * 2)

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("No file is open")
            color: Theme.textSecondary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLarge
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 7

            Repeater {
                model: [
                    { key: "Ctrl+K", label: qsTr("Command Palette") },
                    { key: "Ctrl+P", label: qsTr("Quick Open") },
                    { key: "Ctrl+B", label: qsTr("Toggle Sidebar") }
                ]

                Row {
                    required property var modelData
                    spacing: 10

                    Keycap {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 74
                        text: parent.modelData.key
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.modelData.label
                        color: Theme.textTertiary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeBody
                    }
                }
            }
        }
    }
}
