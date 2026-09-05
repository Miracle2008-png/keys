import QtQuick
import Keys.Ui

/// Shown in the editor area when no project is open.
///
/// Follows the design's welcome screen: the product name and a one-line
/// statement, the two primary actions, the shortcut list, and recent projects.
/// Laid out in two columns so neither side feels crowded, collapsing to one when
/// the window is narrow.
Item {
    id: root

    /// The design pairs actions and history side by side; below this width the
    /// two columns would each be too narrow to read, so they stack instead.
    readonly property bool wide: width > 720

    Column {
        id: content

        anchors.centerIn: parent
        width: Math.min(root.wide ? 860 : 420, root.width - Metrics.spacingLarge * 3)
        spacing: 34

        Column {
            width: parent.width
            spacing: Metrics.spacingSmall

            Text {
                text: qsTr("Keys")
                color: Theme.textPrimary
                font.family: Fonts.ui
                font.pointSize: 34 * 0.75
                font.weight: Font.DemiBold
                font.letterSpacing: -0.5
            }

            Text {
                width: parent.width
                text: qsTr("A fast, focused editor for people who build software for a living.")
                color: Theme.textSecondary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLarge
                wrapMode: Text.WordWrap
            }
        }

        // Two columns when there is room, stacked when there is not. A Grid with
        // a computed column count expresses that without duplicating the children.
        Grid {
            width: parent.width
            columns: root.wide ? 2 : 1
            columnSpacing: 80
            rowSpacing: 34

            Column {
                width: root.wide ? (content.width - 80) / 2 : content.width
                spacing: 28

                Column {
                    spacing: 10

                    PrimaryButton {
                        text: qsTr("Open Project")
                        onClicked: folderDialog.open()
                    }
                }

                Column {
                    spacing: 10

                    SectionLabel { text: qsTr("Keyboard shortcuts") }

                    Column {
                        spacing: 7

                        Repeater {
                            model: [
                                { key: "Ctrl+O", label: qsTr("Open Project") },
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
                                    color: Theme.textSecondary
                                    font.family: Fonts.ui
                                    font.pointSize: Metrics.fontSizeBody
                                }
                            }
                        }
                    }
                }
            }

            Column {
                width: root.wide ? (content.width - 80) / 2 : content.width
                spacing: 12

                SectionLabel { text: qsTr("Recent Projects") }

                // Nothing invented: when there is no history the panel says so
                // rather than showing example projects that do not exist.
                Text {
                    visible: App.recentProjects.length === 0
                    text: qsTr("Projects you open will appear here.")
                    color: Theme.textTertiary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                }

                Column {
                    width: parent.width

                    Repeater {
                        model: App.recentProjects

                        Rectangle {
                            required property var modelData

                            width: parent.width
                            height: 46
                            radius: 7
                            color: recentMouse.containsMouse ? Theme.bgHover : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: App.fastAnimationDuration }
                            }

                            Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.right: whenLabel.left
                                anchors.rightMargin: Metrics.spacingSmall
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2

                                Text {
                                    width: parent.width
                                    text: parent.parent.modelData.name
                                    color: Theme.textPrimary
                                    font.family: Fonts.ui
                                    font.pointSize: Metrics.fontSizeMedium
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                }

                                Text {
                                    width: parent.width
                                    text: parent.parent.modelData.path
                                    color: Theme.textTertiary
                                    font.family: Fonts.ui
                                    font.pointSize: Metrics.fontSizeSmall
                                    elide: Text.ElideMiddle
                                }
                            }

                            Text {
                                id: whenLabel
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: parent.modelData.when
                                color: Theme.textTertiary
                                font.family: Fonts.ui
                                font.pointSize: Metrics.fontSizeLabel
                            }

                            MouseArea {
                                id: recentMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.openProject(parent.modelData.path)
                            }
                        }
                    }
                }
            }
        }
    }

    FolderPicker {
        id: folderDialog
        onFolderAccepted: (path) => App.openProject(path)
    }

    /// Opened by the Ctrl+O shortcut in Main.qml as well as the button.
    function browseForProject() {
        folderDialog.open();
    }
}
