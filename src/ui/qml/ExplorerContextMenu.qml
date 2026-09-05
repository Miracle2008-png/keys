import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Right-click actions for a row in the explorer.
///
/// Every entry here does something. Actions that depend on a milestone that has
/// not landed are absent rather than present-and-disabled: a menu of greyed-out
/// items tells the user nothing useful about what Keys can do.
ContextMenu {
    id: root

    /// The row the menu was opened on.
    property string targetPath: ""
    property bool targetIsDirectory: false

    /// New items go inside a folder, or beside a file.
    readonly property string containerPath:
        targetIsDirectory ? targetPath : parentOf(targetPath)

    function parentOf(path) {
        const cut = path.lastIndexOf("/");
        return cut > 0 ? path.substring(0, cut) : path;
    }

    MenuItem {
        text: qsTr("New File…")
        onTriggered: nameDialog.open(
            NameDialog.CreateFile, root.containerPath, "")
    }

    MenuItem {
        text: qsTr("New Folder…")
        onTriggered: nameDialog.open(
            NameDialog.CreateFolder, root.containerPath, "")
    }

    MenuSeparator {
        contentItem: Rectangle {
            implicitHeight: 1
            color: Theme.border
        }
        padding: 4
        leftPadding: 10
        rightPadding: 10
    }

    MenuItem {
        text: qsTr("Rename…")
        onTriggered: nameDialog.open(
            NameDialog.Rename, root.targetPath,
            root.targetPath.substring(root.targetPath.lastIndexOf("/") + 1))
    }

    MenuItem {
        text: qsTr("Delete")
        onTriggered: deleteConfirm.open(root.targetPath)
    }

    NameDialog {
        id: nameDialog
    }

    DeleteConfirmDialog {
        id: deleteConfirm
    }
}
