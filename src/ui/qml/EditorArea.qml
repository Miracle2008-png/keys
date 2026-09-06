import QtQuick
import Keys.Ui

/// The editor region: the dominant area of the window.
///
/// One pane, or two side by side when split. The divider is draggable, so the
/// user decides how the space is shared rather than being given a fixed half.
Rectangle {
    id: root

    color: Theme.bgEditor

    /// The first pane's share of the width, as a fraction. Clamped so neither
    /// pane can be dragged so narrow that code becomes unreadable.
    property real splitRatio: 0.5

    readonly property int dividerWidth: 1
    readonly property real minimumPaneWidth: 240

    // The three things this area can show cross-fade rather than replacing each
    // other in one frame. Opening a project changes the whole window, and doing
    // that instantly reads as the application restarting.
    FadeLayer {
        anchors.fill: parent
        shown: !App.hasProject && !App.settingsOpen

        WelcomeView {
            id: welcome

            anchors.fill: parent

            // Forwarded rather than answered here: the dialogs live on the
            // window, so one set serves the menu, the palette and this screen
            // alike.
            onNewProjectRequested: root.newProjectRequested()
        }
    }

    /// Raised by the welcome screen when the user wants a new folder to work in.
    signal newProjectRequested()

    /// Opens the project picker. Bound to Ctrl+O and to the File menu, so all
    /// three routes raise the same dialog.
    function browseForProject() {
        welcome.browseForProject();
    }

    // Settings take the whole editor area rather than opening as a tab: the page
    // is not a document, and giving it a tab would imply it can be split,
    // reordered and saved alongside files.
    FadeLayer {
        anchors.fill: parent
        shown: App.settingsOpen

        SettingsPanel {
            anchors.fill: parent
        }
    }

    FadeLayer {
        anchors.fill: parent
        shown: App.hasProject && !App.settingsOpen

        EditorPane {
            id: firstPane

            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: App.isSplit
                   ? Math.round(parent.width * root.splitRatio) - root.dividerWidth
                   : parent.width

            groupIndex: 0
            tabs: Tabs0
            editorModel: Editor0
        }

        Rectangle {
            id: divider

            anchors.left: firstPane.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.dividerWidth
            color: Theme.border
            visible: App.isSplit

            // A wider invisible grab area over the hairline: a one-pixel target
            // is precise to look at and painful to hit.
            MouseArea {
                anchors.fill: parent
                anchors.leftMargin: -3
                anchors.rightMargin: -3
                cursorShape: Qt.SizeHorCursor

                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return;
                    const x = mapToItem(divider.parent, mouse.x, 0).x;
                    const minimum = root.minimumPaneWidth / divider.parent.width;
                    root.splitRatio = Math.max(minimum, Math.min(1 - minimum,
                        x / divider.parent.width));
                }
            }
        }

        EditorPane {
            anchors.left: divider.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            visible: App.isSplit

            groupIndex: 1
            tabs: Tabs1
            editorModel: Editor1
        }
    }
}
