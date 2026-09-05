import QtQuick
import QtQuick.Window
import Keys.Ui

/// The application window.
///
/// Layout follows the design: a 46px top bar, then a row of activity rail +
/// sidebar + main area, then a 24px status bar. The editor area is deliberately
/// the only element that grows - everything else is fixed or user-sized, so the
/// editor stays dominant at every window size.
Window {
    id: root

    width: 1280
    height: 820
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: App.hasProject ? qsTr("%1 - Keys").arg(App.projectName) : qsTr("Keys")
    color: Theme.bgChrome

    Column {
        anchors.fill: parent
        spacing: 0

        TopBar {
            id: topBar
            width: parent.width
        }

        Item {
            width: parent.width
            height: parent.height - topBar.height - statusBar.height

            Row {
                anchors.fill: parent
                spacing: 0

                ActivityRail {
                    id: activityRail
                    height: parent.height
                }

                Sidebar {
                    id: sidebar
                    height: parent.height
                }

                // The editor area takes whatever remains. This is the one element
                // that absorbs resizing.
                EditorArea {
                    id: editorArea
                    width: parent.width - activityRail.width - sidebar.width
                    height: parent.height
                }
            }
        }

        StatusBar {
            id: statusBar
            width: parent.width
        }
    }

    // Only commands that exist are bound. Shortcuts for the palette, quick open
    // and the rest arrive with the milestones that implement them, so no key in
    // Keys is ever bound to something that does nothing.
    Shortcut {
        sequences: [StandardKey.Open]
        onActivated: editorArea.browseForProject()
    }

    Shortcut {
        sequence: "Ctrl+B"
        onActivated: App.invokeCommand("workbench.toggleSidebar")
    }

    // Failures the user caused are shown, never swallowed.
    ErrorToast {
        id: errorToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Metrics.statusBarHeight + Metrics.spacingLarge
    }

    Connections {
        target: App
        function onErrorOccurred(message) {
            errorToast.show(message);
        }
    }

    // File operations report through the model, not the controller, so the
    // toast listens to both rather than the model's failures going unseen.
    Connections {
        target: FileTree
        function onErrorOccurred(message) {
            errorToast.show(message);
        }
    }
}
