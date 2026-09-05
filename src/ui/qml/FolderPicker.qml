import QtQuick
import QtQuick.Dialogs

/// The platform's own folder-selection dialog.
///
/// Qt's FolderDialog is a thin wrapper over the native picker, so Keys gets the
/// system's file browser - with its bookmarks, network locations and keyboard
/// conventions - rather than a reimplementation that would be worse in every way.
FolderDialog {
    id: root

    /// Emitted with a plain filesystem path. QML dialogs report a file:// URL,
    /// and every Keys API takes a path, so the conversion happens once here
    /// instead of at each call site.
    signal folderAccepted(string path)

    title: qsTr("Open Project")

    onAccepted: root.folderAccepted(selectedFolder.toString().replace(/^file:\/\/\//, ""))
}
