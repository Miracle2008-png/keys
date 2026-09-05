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

    CodeEditor {
        id: code
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        editor: root.editorModel
        visible: root.tabs.count > 0
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
