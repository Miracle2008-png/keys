import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The 52px vertical rail of primary views, with theme and settings pinned to
/// the bottom. Matches the design's activity bar.
Rectangle {
    id: root

    width: Metrics.activityRailWidth
    color: Theme.bgChrome

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        Repeater {
            model: [
                { id: "explorer",      icon: Icons.file,   label: qsTr("Explorer") },
                { id: "search",        icon: Icons.search, label: qsTr("Search") },
                { id: "sourceControl", icon: Icons.git,    label: qsTr("Source Control") },
                { id: "debug",         icon: Icons.bug,    label: qsTr("Run and Debug") },
                { id: "extensions",    icon: Icons.puzzle, label: qsTr("Extensions") }
            ]

            // The active view is marked by a bar on the leading edge and a
            // brighter icon, not by a filled block. A filled square in a 44px
            // rail is a large amount of accent for a small amount of meaning,
            // and the brief asks for the accent to be spent selectively.
            Item {
                id: railItem

                required property var modelData

                width: Metrics.railButtonSize
                height: Metrics.railButtonSize

                readonly property bool current:
                    App.activeView === modelData.id && App.sidebarVisible

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: -8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: railItem.current ? railItem.height - 8 : 0
                    radius: 1
                    color: Theme.activeIndicator

                    Behavior on height {
                        NumberAnimation {
                            duration: App.fastAnimationDuration
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    radius: Metrics.radiusSmall
                    color: railMouse.containsMouse && !railItem.current
                           ? Theme.bgHover : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: App.fastAnimationDuration }
                    }
                }

                Icon {
                    anchors.centerIn: parent
                    source: railItem.modelData.icon
                    size: Metrics.railIconSize
                    color: railItem.current ? Theme.textPrimary
                         : railMouse.containsMouse ? Theme.textSecondary
                         : Theme.textTertiary

                    Behavior on color {
                        ColorAnimation { duration: App.fastAnimationDuration }
                    }
                }

                MouseArea {
                    id: railMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: App.selectView(railItem.modelData.id)
                }

                ToolTip.visible: railMouse.containsMouse
                ToolTip.text: railItem.modelData.label
                ToolTip.delay: 500
            }
        }
    }

    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 6

        IconButton {
            size: Metrics.railButtonSize
            source: Theme.mode === Theme.Dark ? Icons.sun : Icons.moon
            tooltip: qsTr("Toggle theme")
            onClicked: Theme.toggleMode()
        }

        IconButton {
            size: Metrics.railButtonSize
            source: Icons.settings
            tooltip: qsTr("Settings")
            active: App.settingsOpen
            // Toggles: pressing the gear again returns to the code, so the
            // button is a place rather than a one-way trip.
            onClicked: App.setSettingsOpen(!App.settingsOpen)
        }
    }
}
