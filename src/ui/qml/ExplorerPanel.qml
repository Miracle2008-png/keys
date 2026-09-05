import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The file tree.
///
/// A plain ListView over the flat model: a collapsed folder's children are not
/// rows, so the view only ever instantiates what is on screen. That is what lets
/// the explorer stay smooth in a project with tens of thousands of files.
Item {
    id: root

    /// Rows are 26px with a 6px radius and 16px of indent per depth, matching
    /// the design's explorer.
    readonly property int rowHeight: 26
    readonly property int indentPerDepth: 16

    ListView {
        id: list

        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.bottomMargin: Metrics.spacingMedium

        model: FileTree
        clip: true
        // Recycling keeps delegate count bounded by the viewport rather than by
        // the model, which is the point of the flat-list design.
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AsNeeded
        }

        delegate: Item {
            id: row

            required property int index
            required property string name
            required property string path
            required property int depth
            required property bool isDirectory
            required property bool isExpanded
            required property bool hasChildren
            required property bool isSelected

            width: list.width
            height: root.rowHeight

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 1
                anchors.bottomMargin: 1
                radius: Metrics.radiusSmall
                color: row.isSelected ? Theme.accentSoft
                     : rowMouse.containsMouse ? Theme.bgHover
                     : "transparent"

                Behavior on color {
                    ColorAnimation { duration: App.fastAnimationDuration }
                }
            }

            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 10 + row.depth * root.indentPerDepth
                anchors.right: parent.right
                anchors.rightMargin: 10
                spacing: 6

                // The chevron occupies its slot even when absent, so file and
                // folder names line up at the same depth.
                Item {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 12
                    height: 12

                    Icon {
                        anchors.centerIn: parent
                        visible: row.isDirectory && row.hasChildren
                        source: row.isExpanded ? Icons.chevronDown : Icons.chevronRight
                        size: 12
                        strokeWidth: 2.2
                        color: Theme.textTertiary
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 12 - parent.spacing
                    text: row.name
                    color: row.isSelected ? Theme.textPrimary : Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                    elide: Text.ElideRight
                }
            }

            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                onClicked: (mouse) => {
                    FileTree.setSelectedPath(row.path);
                    list.currentIndex = row.index;
                    if (mouse.button === Qt.RightButton) {
                        contextMenu.targetPath = row.path;
                        contextMenu.targetIsDirectory = row.isDirectory;
                        contextMenu.popup();
                        return;
                    }
                    // A single click activates: a folder toggles, a file opens.
                    // Requiring a double click to open a file is a habit from
                    // file managers, not editors.
                    FileTree.activate(row.index);
                }
            }
        }

        // Keyboard navigation is a requirement, not a nicety: the explorer must
        // be usable without reaching for the mouse.
        keyNavigationEnabled: true
        focus: true

        Keys.onReturnPressed: if (currentIndex >= 0) FileTree.activate(currentIndex)
        Keys.onSpacePressed: if (currentIndex >= 0) FileTree.activate(currentIndex)

        // Arrow keys move currentIndex; the delegate syncs the model's notion of
        // selection from there, so mouse and keyboard converge on one state.
        onCurrentItemChanged: {
            if (currentItem) {
                FileTree.setSelectedPath(currentItem.path);
            }
        }
    }

    // Shown only when the model is genuinely empty and not mid-load, so an
    // in-flight read does not flash "empty" before the rows arrive.
    Text {
        anchors.centerIn: parent
        width: parent.width - Metrics.spacingLarge * 2
        visible: list.count === 0 && !FileTree.loading
        text: qsTr("This folder is empty.")
        color: Theme.textTertiary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    ExplorerContextMenu {
        id: contextMenu
    }
}
