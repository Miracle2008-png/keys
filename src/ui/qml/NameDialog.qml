import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Asks for a file or folder name.
///
/// One dialog for create-file, create-folder and rename, because the three ask
/// the same question and differ only in their title and what they do with the
/// answer. Three near-identical dialogs would drift apart.
Dialog {
    id: root

    enum Mode { CreateFile, CreateFolder, Rename }

    property int mode: NameDialog.CreateFile
    property string targetPath: ""

    // Anchored to the window rather than the panel so a long name is not
    // clipped by a narrow sidebar.
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 380
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    title: switch (mode) {
        case NameDialog.CreateFile:   return qsTr("New File");
        case NameDialog.CreateFolder: return qsTr("New Folder");
        default:                      return qsTr("Rename");
    }

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

    contentItem: TextField {
        id: field

        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        selectByMouse: true

        background: Rectangle {
            color: Theme.bgSurface
            border.width: 1
            border.color: field.activeFocus ? Theme.accent : Theme.border
            radius: Metrics.radiusSmall
        }

        // Enter confirms, so the common case never needs the mouse.
        onAccepted: if (text.trim().length > 0) root.accept()
    }

    // Nothing to submit until something is typed.
    onOpened: {
        field.forceActiveFocus();
        // On rename, preselect the base name so typing replaces it but the
        // extension survives a careless keystroke.
        if (mode === NameDialog.Rename) {
            const dot = field.text.lastIndexOf(".");
            if (dot > 0) {
                field.select(0, dot);
            } else {
                field.selectAll();
            }
        }
    }

    onAccepted: {
        const name = field.text.trim();
        if (name.length === 0) {
            return;
        }
        switch (mode) {
        case NameDialog.CreateFile:   FileTree.createFile(targetPath, name); break;
        case NameDialog.CreateFolder: FileTree.createFolder(targetPath, name); break;
        case NameDialog.Rename:       FileTree.rename(targetPath, name); break;
        }
    }

    function open(dialogMode, path, initialName) {
        mode = dialogMode;
        targetPath = path;
        field.text = initialName;
        visible = true;
    }
}
