import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Confirms discarding a file's changes.
///
/// Its own dialog rather than a generalised one: deleting a file goes to the
/// system trash and is recoverable, while discarding overwrites the working tree
/// from the index and is not. The wording has to say so, and a shared dialog
/// with a swappable message would blur exactly the distinction that matters.
KeysDialog {
    id: root

    property string targetPath: ""

    readonly property string targetName:
        targetPath.substring(targetPath.lastIndexOf("/") + 1)

    signal discardConfirmed(string path)

    width: 400

    title: qsTr("Discard changes")


    header: Text {
        text: root.title
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLarge
        font.weight: Font.Medium
        padding: Metrics.spacingMedium
    }

    contentItem: Text {
        text: qsTr("Discard your changes to \"%1\"? This cannot be undone.")
                  .arg(root.targetName)
        color: Theme.textSecondary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        wrapMode: Text.WordWrap
        leftPadding: Metrics.spacingMedium
        rightPadding: Metrics.spacingMedium
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
                text: qsTr("Discard")
                primary: true
                destructive: true
                onClicked: root.accept()
            }
        }
    }

    onAccepted: root.discardConfirmed(root.targetPath)

    function confirm(path) {
        targetPath = path;
        visible = true;
    }
}
