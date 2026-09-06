import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Asks for a file or folder name.
///
/// One dialog for create-file, create-folder and rename, because the three ask
/// the same question and differ only in their title and what they do with the
/// answer. Three near-identical dialogs would drift apart.
KeysDialog {
    id: root

    enum Mode { CreateFile, CreateFolder, Rename }

    property int mode: NameDialog.CreateFile
    property string targetPath: ""

    // Anchored to the window rather than the panel, so a long name is not
    // clipped by a narrow sidebar.
    width: 380

    title: switch (mode) {
        case NameDialog.CreateFile:   return qsTr("New File");
        case NameDialog.CreateFolder: return qsTr("New Folder");
        default:                      return qsTr("Rename");
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

    // A themed footer rather than standardButtons: Basic draws two identical
    // grey blocks, giving the confirming action no more weight than cancelling.
    footer: Item {
        implicitHeight: 56

        Row {
            anchors.right: parent.right
            anchors.rightMargin: Metrics.spacingMedium
            anchors.verticalCenter: parent.verticalCenter
            spacing: Metrics.spacingSmall

            DialogButton {
                text: qsTr("Cancel")
                onClicked: root.reject()
            }

            DialogButton {
                text: qsTr("Create")
                primary: true
                // A name is required, so the action that needs one is disabled
                // until there is something to submit.
                enabled: field.text.trim().length > 0
                opacity: enabled ? 1.0 : 0.4
                onClicked: root.accept()
            }
        }
    }

    onOpened: {
        field.forceActiveFocus();
        // On rename, preselect the base name so typing replaces it but a
        // careless keystroke does not destroy the extension.
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
        case NameDialog.CreateFile:
            // Opened as well as created: a new file the user then has to go and
            // find in the tree is a worse outcome than one that is simply there,
            // ready to type into.
            if (FileTree.createFile(targetPath, name)) {
                App.openFile(targetPath + "/" + name);
            }
            break;
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
