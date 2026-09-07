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

    /// Which panel the dock is showing: "terminal" or "problems".
    property string dockPanel: "terminal"

    /// Opens the dock and focuses the terminal, starting one if none is running
    /// - the shortcut means "give me a terminal", not "reveal an empty panel".
    function toggleTerminal() {
        // Closes only when the terminal is what is showing. Ctrl+` while the
        // problems panel is up means "give me the terminal", not "hide this".
        if (dockOpen && dockPanel === "terminal") {
            dockOpen = false;
            return;
        }
        dockOpen = true;
        dockPanel = "terminal";
        if (Terminal.sessionCount === 0) {
            Terminal.openSession();
        }
        terminalPanel.takeFocus();
    }

    /// Shows the problems list, or hides the dock when it is already showing.
    /// Opens the new-project dialog. Reachable from the window so the startup
    /// flag and any future menu item go through one path.
    function showNewProject() {
        newProjectDialog.start();
    }

    function toggleProblems() {
        if (dockOpen && dockPanel === "problems") {
            dockOpen = false;
            return;
        }
        dockOpen = true;
        dockPanel = "problems";
    }

    Column {
        anchors.fill: parent
        spacing: 0

        MenuBar {
            id: menuBar

            width: parent.width

            onNewProjectRequested: root.showNewProject()

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

                        // The welcome screen asks for a project; the window
                        // owns the dialog that makes one.
                        onNewProjectRequested: newProjectDialog.start()
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

                        // The dock's own tab strip. Two panels share the
                        // bottom: a terminal and the problems list both need
                        // width, and neither is worth its own permanent strip
                        // of screen.
                        Row {
                            id: dockTabs

                            anchors.top: parent.top
                            anchors.topMargin: 1
                            anchors.left: parent.left
                            anchors.leftMargin: Metrics.spacingSmall
                            height: 28
                            spacing: 2

                            Repeater {
                                model: [
                                    { id: "terminal", label: qsTr("Terminal") },
                                    { id: "problems", label: qsTr("Problems") }
                                ]

                                Rectangle {
                                    required property var modelData

                                    readonly property bool current:
                                        root.dockPanel === modelData.id

                                    width: dockLabel.implicitWidth + 20
                                    height: 24
                                    anchors.verticalCenter: parent.verticalCenter
                                    radius: Metrics.radiusSmall
                                    color: current ? Theme.bgEditor
                                         : dockTabMouse.containsMouse ? Theme.bgHover
                                         : "transparent"

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: App.fastAnimationDuration
                                        }
                                    }

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        width: parent.width - 12
                                        height: 2
                                        radius: 1
                                        color: Theme.activeIndicator
                                        opacity: parent.current ? 1 : 0

                                        Behavior on opacity {
                                            NumberAnimation {
                                                duration: App.fastAnimationDuration
                                                easing.type: Easing.OutQuad
                                            }
                                        }
                                    }

                                    Text {
                                        id: dockLabel

                                        anchors.centerIn: parent
                                        text: parent.modelData.label
                                        color: parent.current ? Theme.textPrimary
                                                              : Theme.textSecondary
                                        font.family: Fonts.ui
                                        font.pointSize: Metrics.fontSizeLabel
                                    }

                                    // A badge on the Problems tab, so a new
                                    // error is noticed without the panel being
                                    // open. Errors outrank warnings: one number
                                    // that changes meaning would be worse than
                                    // no number.
                                    Rectangle {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 2
                                        anchors.top: parent.top
                                        anchors.topMargin: 1
                                        width: 6
                                        height: 6
                                        radius: 3
                                        visible: parent.modelData.id === "problems"
                                                 && (Problems.errorCount > 0
                                                     || Problems.warningCount > 0)
                                        color: Problems.errorCount > 0 ? Theme.red
                                                                       : Theme.yellow
                                    }

                                    MouseArea {
                                        id: dockTabMouse

                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.dockPanel = parent.modelData.id
                                    }
                                }
                            }
                        }

                        TerminalPanel {
                            id: terminalPanel

                            anchors.fill: parent
                            anchors.topMargin: dockTabs.height + 1
                            visible: root.dockPanel === "terminal"
                        }

                        ProblemsPanel {
                            anchors.fill: parent
                            anchors.topMargin: dockTabs.height + 1
                            visible: root.dockPanel === "problems"
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
        sequence: "Ctrl+Shift+N"
        onActivated: root.showNewProject()
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
        function onProblemsToggleRequested() { root.toggleProblems(); }
    }

    // Alt+6 is where JetBrains puts Problems, and Ctrl+Shift+M is where VS Code
    // does. Both are bound, because either is a reasonable thing to reach for.
    Shortcut {
        sequence: "Alt+6"
        onActivated: root.toggleProblems()
    }

    Shortcut {
        sequence: "Ctrl+Shift+M"
        onActivated: root.toggleProblems()
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
    NewProjectDialog {
        id: newProjectDialog
    }

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
