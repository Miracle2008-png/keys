import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The application menu bar.
///
/// **Every item does something.** Menus are the one place an application
/// advertises what it can do, so an item that opens nothing is worse than a
/// missing menu - it teaches the user the application is broken. Menus here
/// grow as features land rather than being laid out in advance and greyed.
///
/// **Commands, not handlers.** Each item invokes a registered command by id, so
/// the menu, the palette and a future keybinding all reach the same code. An
/// item is enabled when its command is, which is why "Save" dims with no file
/// open without the menu knowing why.
Item {
    id: root

    implicitHeight: 30

    /// Raised by items that need a name or a path from the user. The menu does
    /// not own the dialogs; Main.qml does, so one set serves the menu, the
    /// palette and the explorer's context menu alike.
    signal newFileRequested()
    signal newFolderRequested()
    signal openProjectRequested()
    signal saveAsRequested()
    signal aboutRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.bgChrome

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.border
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingSmall
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Repeater {
            model: [
                { title: qsTr("File"),  menu: fileMenu },
                { title: qsTr("Edit"),  menu: editMenu },
                { title: qsTr("View"),  menu: viewMenu },
                { title: qsTr("Build"), menu: buildMenu },
                { title: qsTr("Help"),  menu: helpMenu },
            ]

            delegate: Rectangle {
                id: titleButton

                required property var modelData

                width: titleText.implicitWidth + Metrics.spacingMedium
                height: 22
                radius: Metrics.radiusSmall
                color: titleButton.modelData.menu.opened ? Theme.bgHover
                     : titleHover.hovered ? Theme.bgHover
                     : "transparent"

                HoverHandler {
                    id: titleHover
                    cursorShape: Qt.PointingHandCursor
                }

                Text {
                    id: titleText

                    anchors.centerIn: parent
                    text: titleButton.modelData.title
                    color: Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        const menu = titleButton.modelData.menu;
                        if (menu.opened) {
                            menu.close();
                        } else {
                            menu.popup(titleButton, 0, titleButton.height + 2);
                        }
                    }

                    // Sliding along an open menu bar switches menus, which is
                    // what every desktop menu does and what makes browsing them
                    // feel right.
                    onEntered: {
                        for (const other of [fileMenu, editMenu, viewMenu,
                                             buildMenu, helpMenu]) {
                            if (other.opened && other !== titleButton.modelData.menu) {
                                other.close();
                                titleButton.modelData.menu.popup(
                                    titleButton, 0, titleButton.height + 2);
                                break;
                            }
                        }
                    }
                    hoverEnabled: true
                }
            }
        }
    }

    // ---- The menus ----

    ContextMenu {
        id: fileMenu

        MenuAction {
            text: qsTr("New File…")
            shortcut: "Ctrl+N"
            enabled: App.hasProject
            onTriggered: root.newFileRequested()
        }

        MenuAction {
            text: qsTr("New Folder…")
            enabled: App.hasProject
            onTriggered: root.newFolderRequested()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Open Project…")
            shortcut: "Ctrl+O"
            onTriggered: root.openProjectRequested()
        }

        MenuAction {
            text: qsTr("Close Project")
            enabled: App.hasProject
            onTriggered: App.closeProject()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Save")
            shortcut: "Ctrl+S"
            enabled: App.hasProject
            onTriggered: App.saveFile()
        }

        MenuAction {
            text: qsTr("Save As…")
            shortcut: "Ctrl+Shift+S"
            enabled: App.hasProject
            onTriggered: root.saveAsRequested()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Settings…")
            shortcut: "Ctrl+,"
            onTriggered: App.setSettingsOpen(true)
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Exit")
            onTriggered: Qt.quit()
        }
    }

    ContextMenu {
        id: editMenu

        MenuAction {
            text: qsTr("Undo")
            shortcut: "Ctrl+Z"
            enabled: root.activeEditor !== null
            onTriggered: root.activeEditor.undo()
        }

        MenuAction {
            text: qsTr("Redo")
            shortcut: "Ctrl+Y"
            enabled: root.activeEditor !== null
            onTriggered: root.activeEditor.redo()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Cut")
            shortcut: "Ctrl+X"
            enabled: root.activeEditor !== null
            onTriggered: root.activeEditor.cut()
        }

        MenuAction {
            text: qsTr("Copy")
            shortcut: "Ctrl+C"
            enabled: root.activeEditor !== null
            onTriggered: root.activeEditor.copy()
        }

        MenuAction {
            text: qsTr("Paste")
            shortcut: "Ctrl+V"
            enabled: root.activeEditor !== null
            onTriggered: root.activeEditor.paste()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Select All")
            shortcut: "Ctrl+A"
            enabled: root.activeEditor !== null
            onTriggered: root.activeEditor.selectAll()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Find in Files…")
            shortcut: "Ctrl+Shift+F"
            enabled: App.hasProject
            onTriggered: App.selectView("search")
        }
    }

    ContextMenu {
        id: viewMenu

        MenuAction {
            text: qsTr("Command Palette…")
            shortcut: "Ctrl+K"
            onTriggered: root.paletteRequested()
        }

        MenuAction {
            text: qsTr("Go to File…")
            shortcut: "Ctrl+P"
            enabled: App.hasProject
            onTriggered: root.quickOpenRequested()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Explorer")
            onTriggered: App.selectView("explorer")
        }

        MenuAction {
            text: qsTr("Search")
            onTriggered: App.selectView("search")
        }

        MenuAction {
            text: qsTr("Source Control")
            onTriggered: App.selectView("sourceControl")
        }

        MenuAction {
            text: qsTr("Run and Debug")
            onTriggered: App.selectView("debug")
        }

        MenuAction {
            text: qsTr("Extensions")
            onTriggered: App.selectView("extensions")
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Toggle Sidebar")
            shortcut: "Ctrl+B"
            onTriggered: App.invokeCommand("workbench.toggleSidebar")
        }

        MenuAction {
            text: qsTr("Split Editor")
            enabled: App.hasProject
            onTriggered: App.toggleSplit()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Toggle Color Theme")
            onTriggered: Theme.toggleMode()
        }
    }

    ContextMenu {
        id: buildMenu

        MenuAction {
            text: qsTr("Build")
            enabled: App.hasProject && !Runner.running
            onTriggered: Runner.runBuild()
        }

        MenuAction {
            text: qsTr("Run")
            enabled: App.hasProject && !Runner.running
            onTriggered: Runner.runRun()
        }

        MenuAction {
            text: qsTr("Stop")
            enabled: Runner.running
            onTriggered: Runner.stop()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Start Debugging")
            enabled: Debugger.configured && !Debugger.active
            onTriggered: Debugger.start()
        }

        MenuAction {
            text: qsTr("Stop Debugging")
            enabled: Debugger.active
            onTriggered: Debugger.stop()
        }

        MenuSeparator {}

        MenuAction {
            text: qsTr("Run and Debug View")
            onTriggered: App.selectView("debug")
        }
    }

    ContextMenu {
        id: helpMenu

        MenuAction {
            text: qsTr("About Keys")
            onTriggered: root.aboutRequested()
        }
    }

    /// The editor of the focused pane, or null. Edit actions need a document to
    /// act on, and reading it here keeps every item's enablement in one place.
    readonly property var activeEditor: {
        if (!App.hasProject) {
            return null;
        }
        const editor = App.activeGroup === 1 ? Editor1 : Editor0;
        return editor && editor.hasDocument ? editor : null;
    }

    signal paletteRequested()
    signal quickOpenRequested()
}
