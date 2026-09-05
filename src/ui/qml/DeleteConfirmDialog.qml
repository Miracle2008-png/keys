import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Confirms deleting a file or folder.
///
/// Deletion is confirmed even though it goes to the system trash: the user may
/// not know that, and a folder full of work disappearing on a stray click is
/// alarming regardless of recoverability. The message says where it goes, so the
/// confirmation is informative rather than a reflex click.
Dialog {
    id: root

    property string targetPath: ""

    readonly property string targetName:
        targetPath.substring(targetPath.lastIndexOf("/") + 1)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 400
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    title: qsTr("Delete")
    standardButtons: Dialog.Ok | Dialog.Cancel

    background: Rectangle {
        color: Theme.bgElevated
        border.width: 1
        border.color: Theme.borderStrong
        radius: Metrics.radiusLarge
    }

    header: Text {
        text: root.title
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLarge
        font.weight: Font.Medium
        padding: Metrics.spacingMedium
    }

    contentItem: Text {
        text: qsTr("Move \"%1\" to the trash?").arg(root.targetName)
        color: Theme.textSecondary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        wrapMode: Text.WordWrap
    }

    onAccepted: FileTree.moveToTrash(targetPath)

    function open(path) {
        targetPath = path;
        visible = true;
    }
}
