import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The settings page.
///
/// Shown in the editor area rather than as a modal dialog: settings are
/// something a user reads and adjusts while working, and a dialog would block
/// the editor they are adjusting it for. Changes apply immediately - there is no
/// OK button, because there is nothing to confirm.
Item {
    id: root

    /// The design asks for spacious layout; the content is held to a readable
    /// measure rather than stretching a label and its control to opposite ends
    /// of a wide window. The band is centred, and the header uses the same one
    /// so the title lines up with the settings beneath it.
    readonly property int horizontalPadding: Metrics.spacingLarge + Metrics.spacingSmall
    readonly property int contentMaxWidth: 720
    readonly property real contentWidth:
        Math.min(contentMaxWidth, width - horizontalPadding * 2)
    readonly property real contentX: Math.max(horizontalPadding, (width - contentWidth) / 2)


    Rectangle {
        anchors.fill: parent
        color: Theme.bgEditor
    }


    // ---- Header ----
    Item {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 72

        Text {
            id: heading

            x: root.contentX
            anchors.top: parent.top
            anchors.topMargin: Metrics.spacingLarge
            text: qsTr("Settings")
            color: Theme.textPrimary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeTitle
        }

        Text {
            anchors.left: heading.left
            anchors.top: heading.bottom
            anchors.topMargin: 2
            text: qsTr("Applies to every project. Changes are saved automatically.")
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeSmall
        }

        // Offered only when something has actually been changed, so the page
        // does not carry a destructive action that would do nothing.
        Item {
            id: resetAll

            x: root.contentX + root.contentWidth - width
            anchors.verticalCenter: heading.verticalCenter
            width: resetAllLabel.implicitWidth + 24
            height: 28
            visible: SettingsList.hasModifiedValues

            Rectangle {
                anchors.fill: parent
                radius: Metrics.radiusSmall
                color: resetAllMouse.containsMouse ? Theme.bgHover : "transparent"
                border.width: 1
                border.color: Theme.border

                Behavior on color {
                    ColorAnimation { duration: App.fastAnimationDuration }
                }
            }

            Text {
                id: resetAllLabel

                anchors.centerIn: parent
                text: qsTr("Reset all")
                color: Theme.textSecondary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeSmall
            }

            MouseArea {
                id: resetAllMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: SettingsList.resetAll()
            }
        }
    }

    Rectangle {
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.border
    }

    ListView {
        id: list

        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 1

        clip: true
        model: SettingsList
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            id: entry

            required property int index
            required property string key
            required property string title
            required property string description
            required property string group
            required property bool isGroupStart
            required property int control
            required property var value
            required property var choices
            required property real minimum
            required property real maximum
            required property bool isInteger
            required property bool isModified

            width: list.width
            height: column.height

            Column {
                id: column

                x: root.contentX
                width: root.contentWidth

                // The group heading is drawn by the first row of each group,
                // the same approach the palette uses: separate header rows
                // would complicate nothing but the model.
                Item {
                    width: parent.width
                    height: entry.isGroupStart ? 46 : 0
                    visible: entry.isGroupStart

                    SectionLabel {
                        anchors.left: parent.left
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Metrics.spacingSmall
                        text: entry.group
                    }
                }

                SettingRow {
                    width: parent.width
                    settingKey: entry.key
                    title: entry.title
                    description: entry.description
                    control: entry.control
                    value: entry.value
                    choices: entry.choices
                    minimum: entry.minimum
                    maximum: entry.maximum
                    isInteger: entry.isInteger
                    isModified: entry.isModified
                }
            }
        }

        footer: Item {
            width: list.width
            height: 60

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: Metrics.spacingMedium
                // Stated rather than hidden: the scale factor is read once when
                // the application starts, so a control that claimed to apply it
                // live would be lying.
                text: qsTr("Interface scale takes effect the next time Keys starts.")
                color: Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeSmall
            }
        }
    }
}
