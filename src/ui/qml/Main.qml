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

    /// Whether the bottom dock is showing, and how tall the user has made it.
    /// Held on the window rather than in the dock so the shortcut, the menu and
    /// anything else that reveals it all read one answer.
    property bool dockOpen: false
    property real dockHeight: 260

    /// Opens the dock and focuses the terminal, starting one if none is running
    /// - the shortcut means "give me a terminal", not "reveal an empty panel".
    function toggleTerminal() {
        if (dockOpen) {
            dockOpen = false;
            return;
        }
        dockOpen = true;
        if (Terminal.sessionCount === 0) {
            Terminal.openSession();
        }
        terminalPanel.takeFocus();
    }

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

            // Find belongs to a pane, not the window: with the editor split it
            // has to open in whichever one holds the caret.
            onFindRequested: (withReplace) => editorArea.openFind(withReplace)
            onGoToLineRequested: goToLineDialog.open()
            onReloadRequested: App.reloadActiveFile()
            onRenameRequested: renameDialog.open(Language.symbolAtCursor())
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

                // The editor and the bottom dock share what is left. The editor
                // absorbs resizing; the dock keeps the height the user gave it.
                Item {
                    width: parent.width - activityRail.width - sidebar.width
                    height: parent.height

                    EditorArea {
                        id: editorArea

                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: bottomDock.visible ? bottomDock.top : parent.bottom

                        // The welcome screen asks for a folder; the window owns
                        // the dialog that makes one.
                        onNewProjectRequested: nameDialog.open(
                            NameDialog.CreateFolder, App.newFileDirectory(), "")
                    }

                    // The terminal lives across the bottom rather than in the
                    // sidebar: a shell needs width, and 248px of it would wrap
                    // every command a build prints.
                    Item {
                        id: bottomDock

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: root.dockHeight
                        visible: root.dockOpen

                        Rectangle {
                            anchors.fill: parent
                            color: Theme.bgSurface
                        }

                        // The grab handle: a one pixel line that reads, over a
                        // five pixel hit area that can actually be grabbed.
                        Rectangle {
                            anchors.top: parent.top
                            width: parent.width
                            height: 1
                            color: dockResize.pressed || dockResize.containsMouse
                                       ? Theme.accent : Theme.border

                            Behavior on color {
                                ColorAnimation { duration: App.fastAnimationDuration }
                            }
                        }

                        MouseArea {
                            id: dockResize

                            anchors.top: parent.top
                            anchors.topMargin: -2
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 5
                            hoverEnabled: true
                            cursorShape: Qt.SizeVerCursor

                            property real pressY: 0
                            property real pressHeight: 0

                            onPressed: (mouse) => {
                                pressY = mapToItem(null, mouse.x, mouse.y).y;
                                pressHeight = root.dockHeight;
                            }

                            onPositionChanged: (mouse) => {
                                if (!pressed) {
                                    return;
                                }
                                const delta = pressY - mapToItem(null, mouse.x, mouse.y).y;
                                root.dockHeight = Math.max(
                                    120, Math.min(bottomDock.parent.height - 120,
                                                  pressHeight + delta));
                            }
                        }

                        TerminalPanel {
                            id: terminalPanel

                            anchors.fill: parent
                            anchors.topMargin: 1
                        }
                    }
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

    // The conventional binding in every editor that has a terminal. Opening it
    // also focuses it: opening a terminal and then having to click into it is
    // the kind of small friction that makes a feature feel unfinished.
    Shortcut {
        sequence: "Ctrl+" + String.fromCharCode(96)
        onActivated: root.toggleTerminal()
    }

    // The same command from the palette and the View menu, so the terminal is
    // discoverable rather than only reachable by a shortcut nobody is told
    // about.
    Connections {
        target: App
        function onTerminalToggleRequested() { root.toggleTerminal(); }
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

    GoToLineDialog {
        id: goToLineDialog
        onLineAccepted: (line) => App.goToLine(line)
    }

    RenameDialog {
        id: renameDialog
        onNameAccepted: (name) => Language.renameSymbol(name)
    }

    // An update is not an error and not urgent, so it gets its own notice in
    // the opposite corner: persistent, with an action, and dismissable.
    UpdateNotice {
        id: updateNotice

        // Anchored to the window, clear of the status bar by its height.
        // `statusBar.top` is not reachable from here - the status bar lives
        // inside the layout Column and this is a direct child of the window,
        // so they are not siblings. QML reports that as a warning nobody sees
        // and leaves the item without geometry.
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Metrics.spacingLarge
        anchors.bottomMargin: Metrics.statusBarHeight + Metrics.spacingLarge
        z: 50
    }

    Connections {
        target: Updates

        function onUpdateFound(version) {
            updateNotice.show(version);
        }

        // A check the user asked for answers either way; the daily one stays
        // silent unless there is something to say.
        function onUpToDate() {
            App.reportNotice(qsTr("Keys %1 is the latest version.")
                             .arg(Updates.currentVersion));
        }

        function onCheckFailed(reason) {
            App.reportNotice(qsTr("Could not check for updates: %1").arg(reason));
        }
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
