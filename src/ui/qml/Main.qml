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

        MenuBar {
            id: menuBar

            width: parent.width

            onNewFileRequested: nameDialog.open(NameDialog.CreateFile,
                                                App.newFileDirectory(), "")
            onNewFolderRequested: nameDialog.open(NameDialog.CreateFolder,
                                                  App.newFileDirectory(), "")
            onOpenProjectRequested: menuFolderPicker.open()
            onSaveAsRequested: saveAsDialog.open()
            onAboutRequested: aboutDialog.open()
            onPaletteRequested: palette.open(">")
            onQuickOpenRequested: palette.open("")
        }

        TopBar {
            id: topBar
            width: parent.width
            onPaletteRequested: palette.open("")
        }

        Item {
            width: parent.width
            height: parent.height - menuBar.height - topBar.height - statusBar.height

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

                    // The welcome screen asks for a folder; the window owns the
                    // dialog that makes one.
                    onNewProjectRequested: nameDialog.open(
                        NameDialog.CreateFolder, App.newFileDirectory(), "")
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
    // Opens the same picker the menu and the welcome screen use. This called
    // editorArea.browseForProject() until the dialogs moved up here - a
    // function EditorArea does not have, so Ctrl+O silently did nothing while
    // the welcome screen advertised it.
    Shortcut {
        sequences: [StandardKey.Open]
        onActivated: menuFolderPicker.open()
    }

    Shortcut {
        sequence: "Ctrl+B"
        onActivated: App.invokeCommand("workbench.toggleSidebar")
    }

    Shortcut {
        sequences: [StandardKey.Save]
        onActivated: App.invokeCommand("workspace.saveFile")
    }

    // Above everything, so the dimmed backdrop covers the whole workbench.
    CommandPalette {
        id: palette
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: palette.open(">")
    }

    Shortcut {
        sequence: "Ctrl+P"
        onActivated: palette.open("")
    }

    // ---- Dialogs the menu raises ----
    //
    // Owned by the window rather than by the menu, so a command invoked from the
    // palette raises the same dialog the menu does.

    // Creates through FileTree, against whatever directory it is opened with -
    // the same dialog the explorer's context menu uses, so a file made from the
    // menu and one made from a right-click behave identically.
    NameDialog {
        id: nameDialog
    }

    FolderPicker {
        id: menuFolderPicker
        onFolderAccepted: (path) => App.openProject(path)
    }

    SaveAsDialog {
        id: saveAsDialog
        onPathAccepted: (path) => App.saveFileAs(path)
    }

    AboutDialog {
        id: aboutDialog
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
