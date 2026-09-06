import QtQuick
import QtQuick.Dialogs

/// The platform's own save dialog.
///
/// Native rather than reimplemented, for the same reason as FolderPicker: the
/// system's browser brings bookmarks, network locations and the keyboard
/// conventions people already know.
FileDialog {
    id: root

    /// Emitted with a plain filesystem path; QML dialogs report a file:// URL
    /// and every Keys API takes a path.
    signal pathAccepted(string path)

    title: qsTr("Save As")
    fileMode: FileDialog.SaveFile

    onAccepted: root.pathAccepted(selectedFile.toString().replace(/^file:\/\/\//, ""))
}
