import QtQuick
import Keys.Ui

/// One editor pane: a tab bar over a code editor.
///
/// A pane knows which group it represents so a split shows two independent sets
/// of tabs. Clicking anywhere in an unfocused pane focuses it, which is what
/// makes "the active editor" follow the user rather than a hidden selection.
Item {
    id: root

    /// Which editor group this pane shows.
    required property int groupIndex

    /// The tab model for that group.
    required property var tabs

    /// The view model driving the code area.
    required property var editorModel

    readonly property bool focused: App.activeGroup === groupIndex

    EditorTabBar {
        id: tabBar
        width: parent.width
        tabs: root.tabs
        paneFocused: root.focused
        // Only the first pane carries the split control; two would be ambiguous
        // about which pane they act on.
        showSplitControl: root.groupIndex === 0
    }

    // Between the tabs and the text: the bar belongs to this document, and
    // putting it over the editor would cover the matches it is finding.
    FindBar {
        id: findBar

        anchors.top: tabBar.bottom
        width: parent.width
        editor: root.editorModel

        onEditorFocusRequested: code.forceActiveFocus()
    }

    CodeEditor {
        id: code
        anchors.top: findBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        editor: root.editorModel
        visible: root.tabs.count > 0

        // Ctrl+F and Ctrl+H are handled here rather than as window shortcuts,
        // so they act on the pane the caret is in when the editor is split.
        Keys.onPressed: (event) => {
            if ((event.modifiers & Qt.ControlModifier) === 0) {
                return;
            }
            if (event.key === Qt.Key_F) {
                root.openFind(false);
                event.accepted = true;
            } else if (event.key === Qt.Key_H) {
                root.openFind(true);
                event.accepted = true;
            }
        }
    }

    /// Opens the find bar and puts the caret in its query field.
    function openFind(withReplace) {
        root.editorModel.openFind(withReplace);
        findBar.takeFocus();
    }

    // The completion popup, over the editor rather than inside it: it must be
    // able to extend past the editor's own clip rectangle near the bottom edge.
    CompletionPopup {
        anchors.fill: undefined
        parent: code
        caretX: code.gutterWidth + code.editor.cursorColumn * code.charWidth
        caretY: (code.editor.cursorLine + 1) * code.lineHeight - code.scrollOffset
        lineHeight: code.lineHeight

        visible: Language.completionVisible && root.focused
    }


    Text {
        anchors.centerIn: parent
        visible: root.tabs.count === 0
        text: qsTr("No file is open")
        color: Theme.textSecondary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeLarge
    }

    // Focus follows the click, including into the empty state, so a user can
    // select a pane before opening anything in it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        propagateComposedEvents: true
        z: -1
        onPressed: (mouse) => {
            App.focusGroup(root.groupIndex);
            mouse.accepted = false;
        }
    }
}
