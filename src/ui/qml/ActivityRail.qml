import QtQuick
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

            IconButton {
                required property var modelData

                size: Metrics.railButtonSize
                source: modelData.icon
                tooltip: modelData.label
                active: App.activeView === modelData.id && App.sidebarVisible
                onClicked: App.selectView(modelData.id)
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
