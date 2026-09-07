import QtQuick
import Keys.Ui

/// Confirms removing an extension.
///
/// Its own dialog rather than DeleteConfirmDialog, which deletes a project file
/// and calls FileTree.moveToTrash directly. Reusing it would have meant a
/// dialog that says "Delete" about something that is not a file and does the
/// wrong thing when accepted.
///
/// The wording says what actually happens. An extension's files are removed and
/// its permissions are forgotten, so reinstalling later asks again - which is
/// the part someone would otherwise be surprised by.
KeysDialog {
    id: root

    /// The extension to remove, and what to call it.
    property string extensionId: ""
    property string extensionName: ""

    signal confirmed(string extensionId)

    width: 420
    title: qsTr("Uninstall extension")

    header: Text {
        text: root.title
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLarge
        font.weight: Font.Medium
        padding: Metrics.spacingMedium
    }

    contentItem: Column {
        spacing: Metrics.spacingSmall

        Text {
            width: parent.width
            text: qsTr("Remove %1?").arg(root.extensionName)
            color: Theme.textPrimary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            text: qsTr("Its files are deleted and the permissions you granted it "
                       + "are forgotten. Installing it again will ask for them "
                       + "afresh.")
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }

    onAccepted: root.confirmed(root.extensionId)

    function ask(id, name) {
        root.extensionId = id;
        root.extensionName = name;
        root.visible = true;
    }
}
