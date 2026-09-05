import QtQuick
import Keys.Ui

/// The editor region: the dominant area of the window.
///
/// With no project open it shows the welcome view. With a project open but no
/// file open it says so plainly - the text engine arrives in milestone 4, and
/// showing mock code here would misrepresent what the application can do.
Rectangle {
    id: root

    color: Theme.bgEditor

    WelcomeView {
        id: welcome
        anchors.fill: parent
        visible: !App.hasProject
    }

    CodeEditor {
        id: editor
        anchors.fill: parent
        visible: App.hasProject && Editor.hasDocument && Editor.path.length > 0
    }

    Text {
        anchors.centerIn: parent
        visible: App.hasProject && !editor.visible
        text: qsTr("No file is open")
        color: Theme.textSecondary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLarge
    }

    /// Main.qml routes the Ctrl+O shortcut here so the picker has one owner.
    function browseForProject() {
        welcome.browseForProject();
    }
}
