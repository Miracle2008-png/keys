import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The tab bar for one editor group.
///
/// Follows the design: a file-type dot, the name, and either a dirty dot or a
/// close button — never both, because the same slot holds whichever applies and
/// a tab that showed two indicators would be wider than its neighbours.
Rectangle {
    id: root

    /// The group's tab model, supplied by the pane that owns it.
    required property var tabs

    /// Whether this bar belongs to the focused pane. An unfocused pane's tabs
    /// are dimmed so the user can see which one their typing goes to.
    property bool paneFocused: true

    /// The split control only appears on the first bar; two split buttons would
    /// be ambiguous about which pane they act on.
    property bool showSplitControl: true

    height: 38
    color: Theme.bgChrome

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    ListView {
        id: list

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: splitButton.visible ? splitButton.left : parent.right
        orientation: ListView.Horizontal
        model: root.tabs
        clip: true
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds

        // Tab overflow scrolls rather than shrinking tabs to illegibility. The
        // design's "intelligent compression" is a later refinement; scrolling is
        // the honest behaviour until then, and never makes a name unreadable.
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded; height: 3 }

        // Tabs slide in and out and close the gap behind them. Opening a file
        // is the most frequent thing anyone does in an editor, and a tab that
        // simply materialises makes the bar feel like a list being rewritten
        // rather than one gaining an item.
        add: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: App.fastAnimationDuration
                easing.type: Easing.OutCubic
            }
        }

        displaced: Transition {
            NumberAnimation {
                properties: "x"
                duration: App.animationDuration
                easing.type: Easing.OutCubic
            }
        }

        remove: Transition {
            NumberAnimation {
                property: "opacity"
                to: 0
                duration: App.fastAnimationDuration
                easing.type: Easing.InCubic
            }
        }

        delegate: Item {
            id: tab

            required property int index
            required property string name
            required property string path
            required property bool modified
            required property color accent

            readonly property bool active: index === root.tabs.activeIndex

            width: Math.min(220, label.implicitWidth + 58)
            height: list.height

            Rectangle {
                anchors.fill: parent
                color: tab.active ? Theme.bgEditor
                     : tabMouse.containsMouse ? Theme.bgHover
                     : "transparent"

                Behavior on color {
                    ColorAnimation { duration: App.fastAnimationDuration }
                }
            }

            // The active tab is marked by an accent line along its top edge.
            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 2
                color: tab.active ? Theme.accent : "transparent"
            }

            Rectangle {
                anchors.right: parent.right
                width: 1
                height: parent.height
                color: Theme.border
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 7
                    height: 7
                    radius: 2
                    color: tab.accent
                    opacity: root.paneFocused ? 1.0 : 0.5
                }

                Text {
                    id: label
                    anchors.verticalCenter: parent.verticalCenter
                    text: tab.name
                    color: tab.active ? Theme.textPrimary : Theme.textSecondary
                    opacity: root.paneFocused ? 1.0 : 0.6
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                    elide: Text.ElideMiddle
                    width: Math.min(implicitWidth, 150)
                }
            }

            // One slot: a dirty dot, or a close button on hover. A modified tab
            // shows the dot until hovered, so the close target is still
            // reachable without losing the unsaved signal at a glance.
            Item {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14

                Rectangle {
                    anchors.centerIn: parent
                    visible: tab.modified && !closeMouse.containsMouse
                    width: 6
                    height: 6
                    radius: 3
                    color: Theme.textSecondary
                }

                Icon {
                    anchors.centerIn: parent
                    visible: !tab.modified || closeMouse.containsMouse
                    opacity: tabMouse.containsMouse || closeMouse.containsMouse ? 1 : 0
                    source: Icons.close
                    size: 12
                    strokeWidth: 2.2
                    color: closeMouse.containsMouse ? Theme.textPrimary : Theme.textTertiary

                    Behavior on opacity {
                        NumberAnimation { duration: App.fastAnimationDuration }
                    }
                }

                MouseArea {
                    id: closeMouse
                    anchors.fill: parent
                    anchors.margins: -3
                    hoverEnabled: true
                    onClicked: root.tabs.close(tab.index)
                }
            }

            MouseArea {
                id: tabMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton

                onClicked: (mouse) => {
                    // Middle click closes, the convention every browser and
                    // editor shares.
                    if (mouse.button === Qt.MiddleButton) {
                        root.tabs.close(tab.index);
                        return;
                    }
                    root.tabs.activate(tab.index);
                }
            }
        }
    }

    IconButton {
        id: splitButton

        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: root.showSplitControl
        size: 26
        iconSize: 15
        source: Icons.split
        active: App.isSplit
        tooltip: App.isSplit ? qsTr("Close split") : qsTr("Split editor")
        onClicked: App.toggleSplit()
    }
}
