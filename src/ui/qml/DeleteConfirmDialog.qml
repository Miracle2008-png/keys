import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Confirms deleting a file or folder.
///
/// Deletion is confirmed even though it goes to the system trash: the user may
/// not know that, and a folder full of work vanishing on a stray click is
/// alarming regardless of recoverability. The message names the destination, so
/// the confirmation informs rather than trains a reflex click.
KeysDialog {
    id: root

    property string targetPath: ""

    readonly property string targetName:
        targetPath.substring(targetPath.lastIndexOf("/") + 1)

    width: 400

    title: qsTr("Delete")


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
                text: qsTr("Delete")
                primary: true
                destructive: true
                onClicked: root.accept()
            }
        }
    }

    onAccepted: FileTree.moveToTrash(targetPath)

    function open(path) {
        targetPath = path;
        visible = true;
    }
}
