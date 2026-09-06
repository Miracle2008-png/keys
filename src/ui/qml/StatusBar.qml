import QtQuick
import Keys.Ui

/// The status bar.
///
/// **Quiet, not loud.** This was painted in the accent colour, which made a
/// 24px strip the strongest horizontal line in the window - the eye went to the
/// bottom edge rather than to the code. CLion and Atom both keep it in the
/// chrome surface with dim text, so it reads when you look for it and recedes
/// when you do not. That is what a status bar is for.
///
/// **Only what is true.** Nothing here is populated until the subsystem behind
/// it exists, and a field with nothing to say is hidden rather than showing a
/// dash. A status bar full of placeholders is worse than a short one.
Rectangle {
    id: root

    /// The editor of the focused pane, or null. The position and language
    /// readouts follow it, so they are blank with no file open rather than
    /// reporting the last one that was.
    readonly property var activeEditor: {
        if (!App.hasProject) {
            return null;
        }
        const editor = App.activeGroup === 1 ? Editor1 : Editor0;
        return editor && editor.hasDocument ? editor : null;
    }

    height: Metrics.statusBarHeight
    color: Theme.bgChrome

    // The bar is a distinct surface, not the bottom of the editor.
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        // The branch, and how far it has diverged. Shown only inside a
        // repository - a project without git shows nothing here rather than a
        // dash.
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            visible: SourceControl.hasRepository

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                source: Icons.git
                size: 12
                color: Theme.textSecondary
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: SourceControl.branch
                color: Theme.textSecondary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: SourceControl.hasUpstream
                         && (SourceControl.ahead > 0 || SourceControl.behind > 0)
                text: (SourceControl.behind > 0 ? "↓" + SourceControl.behind : "")
                      + (SourceControl.ahead > 0 && SourceControl.behind > 0 ? " " : "")
                      + (SourceControl.ahead > 0 ? "↑" + SourceControl.ahead : "")
                color: Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }

            // Pending changes, so the count is visible without opening the
            // panel.
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: SourceControl.count > 0
                text: "· " + SourceControl.count
                color: Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }
        }
    }

    // ---- The right-hand readouts ----
    //
    // The order is CLion's: position, indentation, encoding, line ending, then
    // language. People learn where a field is and look straight at it, so the
    // order matters more than any one item.

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        // Line and column, counted from one - editors are read by people, and
        // no one calls the first line "line 0". The column is in UTF-16 units,
        // which is what the buffer and the language server both use.
        StatusItem {
            visible: root.activeEditor !== null
            text: root.activeEditor
                  ? (root.activeEditor.cursorLine + 1) + ":"
                    + (root.activeEditor.cursorColumn + 1)
                  : ""
            tip: qsTr("Line and column")
        }

        StatusItem {
            visible: root.activeEditor !== null
            text: EditorConfig.insertSpaces
                  ? qsTr("Spaces: %1").arg(EditorConfig.tabSize)
                  : qsTr("Tab: %1").arg(EditorConfig.tabSize)
            tip: qsTr("Indentation")
        }

        StatusItem {
            text: "UTF-8"
            tip: qsTr("File encoding")
        }

        StatusItem {
            visible: root.activeEditor !== null
            text: root.activeEditor && root.activeEditor.languageName.length > 0
                  ? root.activeEditor.languageName
                  : qsTr("Plain text")
            tip: qsTr("File type")
        }
    }
}
